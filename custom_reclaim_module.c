#include <linux/module.h>    // included for all kernel modules
#include <linux/init.h>      // included for __init and __exit macros
#include <linux/sysctl.h>   // for sysctl interface
#include <linux/kernel.h>    // included for KERN_INFO
#include <linux/errno.h>
#include <linux/swap.h>
#include <linux/kallsyms.h>

#include "scan_control.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vratislav Bendel <vbendel@redhat.com>");
MODULE_DESCRIPTION("Module to provide per-node interface similar to cgroup-v2 memory.reclaim.");

/**
 * #######################################################################
 * ########################## !! DISCLAIMER !! ###########################
 * #######################################################################
 *
 * This module is developed as "best-effort" ONLY and meant primarily for 
 * testing purposes. Red Hat holds no responsibility nor liability for 
 * it's use or misuse.
 *
 * #######################################################################
 */

/**
 * DEBUG CONFIGS
 */
#define CUSTOM_RECLAIM_DEBUG 1

/**
 * Number of reclaimed pages over module's uptime
 */
unsigned long total_nr_reclaimed = 0;

/**
 * RECLAIM CONFIGS
 */
#define CUSTOM_RECLAIM_MAY_SWAP 1

/**
 * Input array
 */
unsigned long custom_reclaim_inputs[2] = { 0, 0};


/**
 * Non-included definitions
 */
static unsigned long (*do_ttfp)(struct zonelist *zonelist,
                      struct scan_control *sc);

#define GFP_RECLAIM_MASK (__GFP_RECLAIM|__GFP_HIGH|__GFP_IO|__GFP_FS|\
            __GFP_NOWARN|__GFP_RETRY_MAYFAIL|__GFP_NOFAIL|\
            __GFP_NORETRY|__GFP_MEMALLOC|__GFP_NOMEMALLOC|\
            __GFP_ATOMIC)

/**
 * Call kernel reclaim - do_try_to_free_pages()
 * 	  nr_to_reclaim - number of pages to reclaim
 * 	  node_id		- node index to reclaim from
 *
 * Sets scan control in the same way as cgroup-v2 memory.reclaim,
 * but doesn't target any cgroup.
 */
int do_custom_reclaim(unsigned long nr_to_reclaim, int node_id)
{
	unsigned long nr_reclaimed = 0;
	struct scan_control sc = {
	    .nr_to_reclaim = max(nr_to_reclaim, SWAP_CLUSTER_MAX),
    	.gfp_mask = (GFP_KERNEL & GFP_RECLAIM_MASK) |
				(GFP_HIGHUSER_MOVABLE & ~GFP_RECLAIM_MASK),
	    .reclaim_idx = MAX_NR_ZONES - 1,
    	.priority = DEF_PRIORITY,
	    .may_writepage = !laptop_mode,
    	.may_unmap = 1,
	    .may_swap = CUSTOM_RECLAIM_MAY_SWAP,
	};
    struct zonelist *zonelist = node_zonelist(node_id, sc.gfp_mask);

	nr_reclaimed = do_ttfp(zonelist, &sc);

	return nr_reclaimed;
}

/**
 * Handler function for sysctl file
 */
int custom_reclaim_handler(struct ctl_table *ctl, int write,
                          void __user *buffer, size_t *lenp, loff_t *ppos)
{
	int err;
	int nr_reclaimed = 0;
	unsigned long nr_pages = 0;

	if (!write)
		return -EINVAL;

	err = proc_doulongvec_minmax(ctl, write, buffer, lenp, ppos);
	if (err)
		return err;
	if (custom_reclaim_inputs[0] >= nr_online_nodes) {
		printk(KERN_WARNING "Custom-Reclaim: Input node_idx=%ld larger than nr_online_nodes=%d\n",
				custom_reclaim_inputs[0], nr_online_nodes);
		return -EINVAL;
	}

	nr_pages = min(custom_reclaim_inputs[1] / PAGE_SIZE, (u64)PAGE_COUNTER_MAX);

#ifdef CUSTOM_RECLAIM_DEBUG
	printk(KERN_INFO "Custom-Reclaim: DEBUG: input node_idx=%ld nr_to_reclaim=%ld nr_pages=%ld\n", 
					custom_reclaim_inputs[0], custom_reclaim_inputs[1], nr_pages);
#endif
	if (nr_pages > 0)
		nr_reclaimed = do_custom_reclaim(nr_pages, custom_reclaim_inputs[0]);

	total_nr_reclaimed += nr_reclaimed;

	return 0;
}

/**
 * SYSCTL Definitions
 */
static struct ctl_table_header *custom_reclaim_sysctl_header = NULL;

static struct ctl_table custom_reclaim_table[] = {
	{
		.procname	= "custom_reclaim",
		.data		= &custom_reclaim_inputs,
		.maxlen		= 2*sizeof(long),	
		.mode           = 0200,
		.proc_handler   = &custom_reclaim_handler,
	},
	{
		.procname	= "total_nr_reclaimed_pages",
		.data		= &total_nr_reclaimed,
		.maxlen		= sizeof(long),
		.mode		= 0444,
		.proc_handler	= proc_doulongvec_minmax,
	},
	{ }
};

static struct ctl_table custom_reclaim_dir_table[] = {
	{
		.procname       = "custom_reclaim",
		.mode           = 0555,
		.child          = custom_reclaim_table,
	},
	{ }
};


/**
 * INIT/EXIT functions:
 */
static int __init custom_reclaim_init(void)
{
	printk(KERN_INFO "Custom-Reclaim: Module loading...\n");

	do_ttfp = (void *) kallsyms_lookup_name("do_try_to_free_pages");

#ifdef CUSTOM_RECLAIM_DEBUG
	printk(KERN_INFO "Custom-Reclaim: DEBUG: do_ttfp = %lx\n",(unsigned long) do_ttfp);
#endif

    custom_reclaim_sysctl_header = register_sysctl_table(custom_reclaim_dir_table);
    if (!custom_reclaim_sysctl_header) {
        goto fail_sysctl;
	}

	return 0;

fail_sysctl:
	return -EFAULT;
}

static void __exit custom_reclaim_cleanup(void)
{
    unregister_sysctl_table(custom_reclaim_sysctl_header);
    printk(KERN_INFO "Custom-Reclaim: Module unloaded.\n");
}

module_init(custom_reclaim_init);
module_exit(custom_reclaim_cleanup);


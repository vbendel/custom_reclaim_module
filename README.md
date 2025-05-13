# Custom Reclaim kernel module

This kernel module provides a procfs interface to manually trigger memory reclaim for specific NUMA node, similar to cgroup-v2 `memory.reclaim`. 

## Installation

Repo contains a Makefile that builds a .ko object file which can be loaded into running kernel.

## Usage

Exports the following procfs interface:
~~~
/proc/sys/custom_reclaim/total_nr_reclaimed_pages
/proc/sys/custom_reclaim/custom_reclaim
~~~
* custom_reclaim
  Root write-only file. Takes 2 inpt values - node_id (int node index) and nr_to_reclaim (in bytes)
  Example - reclaim 200MB on node 0:
  ~~~
  # echo "0 209715200" > /proc/sys/custom_reclaim/custom/reclaim
  ~~~
* total_nr_reclaimed_pages
  Read-only file that return the number of pages that were succesfully reclaimed by this module, as returned by `do_try_to_free_pages()` kernel function.

## DISCLAIMER

This module is developed rather as an experiment. It has not been anyhow extensively tested and should be used with care in mind.


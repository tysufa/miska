/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 Meta Platforms, Inc. and affiliates.
 * Copyright (c) 2022 Tejun Heo <tj@kernel.org>
 * Copyright (c) 2022 David Vernet <dvernet@meta.com>
 */
#include "bpf/libbpf.h"
#include <scx/common.h>
#include "scx_ksched.bpf.skel.h"
#include <assert.h>
#include <bpf/bpf.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

const char help_fmt[] =
    "A simple sched_ext scheduler.\n"
    "\n"
    "See the top-level comment in .bpf.c for more details.\n"
    "\n"
    "Usage: %s [-f] [-v]\n"
    "\n"
    "  -f            Use FIFO scheduling instead of weighted vtime scheduling\n"
    "  -v            Print libbpf debug messages\n"
    "  -h            Display this help and exit\n";

static bool verbose;
static volatile int exit_req;

struct sched_stats {
  u64 total_latency;
  u64 task_count;
};

struct bst_node {
  int pid;
  bool active;
  bool is_red;
  int parent;
  int left;
  int right;
};

struct rb_tree {
  uint32_t lock; // bpf_spin_lock is a 4-byte opaque value (init to 0)
  int root_idx;
  int free_top;
  int free_stack[1024];
  struct bst_node nodes[1024];
};

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
                           va_list args) {
  if (level == LIBBPF_DEBUG && !verbose)
    return 0;
  return vfprintf(stderr, format, args);
}

static void sigint_handler(int simple) { exit_req = 1; }

// static void read_stats(struct scx_ksched *skel, __u64 *stats)
// {
// 	int nr_cpus = libbpf_num_possible_cpus();
// 	assert(nr_cpus > 0);
// 	// __u64 cnts[3][nr_cpus];
// 	// __u32 idx;
//
// 	memset(stats, 0, sizeof(stats[0]) * 3);
//
// 	// for (idx = 0; idx < 2; idx++) {
// 	// 	int ret, cpu;
// 	//
// 	// 	ret = bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats), &idx,
// 	// 				  cnts[idx]);
// 	// 	if (ret < 0)
// 	// 		continue;
// 	// 	for (cpu = 0; cpu < nr_cpus; cpu++) {
// 	// 		stats[idx] = cnts[idx][cpu];
// 	// 	}
// 	// }
//
// 	__u32 idx = 2;
// 	bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats), &idx, &stats[2]);
// 	idx = 3;
// 	bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats), &idx, &stats[3]);
// }

static void initialize_tree_map(int map_fd) {
  uint32_t key = 0;
  struct rb_tree tree;

  // everything to 0, especially important for the lock
  memset(&tree, 0, sizeof(tree));

  tree.root_idx = -1;

  tree.free_top = 1024;
  for (int i = 0; i < 1024; i++) {
    // We push indices in reverse so index 0 is at the top of the stack
    tree.free_stack[i] = 1023 - i;

    tree.nodes[i].active = -1;
    tree.nodes[i].is_red = 0;
    tree.nodes[i].parent = -1;
    tree.nodes[i].left = -1;
    tree.nodes[i].right = -1;
  }

  // Update the BPF map
  if (bpf_map_update_elem(map_fd, &key, &tree, BPF_ANY) != 0) {
    fprintf(stderr, "Failed to initialize tree_map\n");
  }
}

static int latency(int map_fd) {
  __u32 key = 0;
  struct sched_stats stats;

  bpf_map_lookup_elem(map_fd, &key, &stats);

  int latency = 0;
  if (stats.task_count > 0)
    latency = stats.total_latency / stats.task_count;

  // Find the first key
  printf("latency : %.2f ms\n", (double)latency / 1000000);

  return 0;
}

int main(int argc, char **argv) {
  struct scx_ksched *skel;
  struct bpf_link *link;
  __u32 opt;
  __u64 ecode;

  libbpf_set_print(libbpf_print_fn);
  signal(SIGINT, sigint_handler);
  signal(SIGTERM, sigint_handler);
restart:
  skel = SCX_OPS_OPEN(ksched_ops, scx_ksched);

  while ((opt = getopt(argc, argv, "fvh")) != -1) {
    switch (opt) {
    case 'f':
      skel->rodata->min_pid = true;
      break;
    case 'v':
      verbose = true;
      break;
    default:
      fprintf(stderr, help_fmt, basename(argv[0]));
      return opt != 'h';
    }
  }

  SCX_OPS_LOAD(skel, ksched_ops, scx_ksched, uei);

  // when we start, all slots are inactive
  int fd = bpf_map__fd(skel->maps.tree_map);
  initialize_tree_map(fd);

  // on initialise toute les cases libres dans la pile

  link = SCX_OPS_ATTACH(skel, ksched_ops, scx_ksched);

  while (!exit_req && !UEI_EXITED(skel, uei)) {
    // read_stats(skel, stats);

    latency(bpf_map__fd(skel->maps.stats));

    // printf("global=%llu, pid=%llu, latency=%llu\n", stats[1],
    //        stats[2], stats[3]);
    fflush(stdout);
    sleep(1);
  }

  __u32 key = 0;
  struct sched_stats stats;

  bpf_map_lookup_elem(bpf_map__fd(skel->maps.stats), &key, &stats);

  printf("latence totale : %lu\n", stats.total_latency);

  bpf_link__destroy(link);
  ecode = UEI_REPORT(skel, uei);
  scx_ksched__destroy(skel);

  if (UEI_ECODE_RESTART(ecode))
    goto restart;
  return 0;
}

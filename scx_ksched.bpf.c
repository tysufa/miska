#include "vmlinux.h"
#include <scx/common.bpf.h>

char _license[] SEC("license") = "GPL";

const volatile bool min_pid;

static u64 vtime_now;
UEI_DEFINE(uei);

#define SHARED_DSQ 0
#define MAX_PROC 200
#define EMPTY -1

struct bst_node {
  int pid;
  bool active;
  bool is_red;
  int parent;
  int left; // Index dans la pool (-1 si NULL)
  int right;
};

struct rb_tree {
  struct bpf_spin_lock lock;
  int root_idx;
  int free_top;
  int free_stack[1024];
  struct bst_node nodes[1024];
};

struct sched_stats {
  u64 total_latency;
  u64 task_count;
};

// MAPS

struct {
  __uint(type, BPF_MAP_TYPE_ARRAY);
  __uint(max_entries, 1);
  __type(key, u32);
  __type(value, struct rb_tree);
} tree_map SEC(".maps");
struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, 1024);
  __type(key, u32);
  __type(value, u64);
} latency_map SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_ARRAY);
  __uint(max_entries, 1);
  __type(key, u32);
  __type(value, struct sched_stats);
} stats SEC(".maps");

static int create_node(struct rb_tree *tree, int pid) {
  if (tree->free_top <= 0 || tree->free_top > 1024)
    return -1;

  tree->free_top--;
  u32 idx = tree->free_top;

  // Boundary check idx for the stack access
  if (idx >= 1024)
    return -1;

  int new_idx = tree->free_stack[idx];

  // bpf verifier condition
  if (new_idx < 0 || new_idx >= 1024) {
    // we give back the slot to the freelist
    tree->free_top++;
    return -1;
  }

  struct bst_node *node = &tree->nodes[new_idx];
  node->pid = pid;
  node->parent = -1;
  node->active = true;
  node->is_red = true;
  node->left = -1;
  node->right = -1;

  return new_idx;
}

static inline void left_rotate(struct rb_tree *tree, int x_idx) {
  if (x_idx < 0 || x_idx >= 1024)
    return;

  int right_child = tree->nodes[x_idx].right;
  if (right_child < 0 || right_child >= 1024)
    return;

  // left sub tree of the right child becomes the right sub tree of x
  tree->nodes[x_idx].right = tree->nodes[right_child].left;
  if (tree->nodes[right_child].left >= 0 &&
      tree->nodes[right_child].left < 1024) {
    tree->nodes[tree->nodes[right_child].left].parent = x_idx;
  }

  // x parent becomes right child parent
  tree->nodes[right_child].parent = tree->nodes[x_idx].parent;
  if (tree->nodes[x_idx].parent == -1) {
    tree->root_idx = right_child;
  } else if (tree->nodes[x_idx].parent >= 0 &&
             tree->nodes[x_idx].parent < 1024) {
    if (x_idx == tree->nodes[tree->nodes[x_idx].parent].left) {
      tree->nodes[tree->nodes[x_idx].parent].left = right_child;
    } else {
      tree->nodes[tree->nodes[x_idx].parent].right = right_child;
    }
  }

  // x becomes the sub tree of left child
  tree->nodes[right_child].left = x_idx;
  tree->nodes[x_idx].parent = right_child;
}

static inline void right_rotate(struct rb_tree *tree, int x_idx) {
  if (x_idx < 0 || x_idx >= 1024)
    return;

  int left_child = tree->nodes[x_idx].left;
  if (left_child < 0 || left_child >= 1024)
    return;

  tree->nodes[x_idx].left = tree->nodes[left_child].right;
  if (tree->nodes[left_child].right >= 0 &&
      tree->nodes[left_child].right < 1024) {
    tree->nodes[tree->nodes[left_child].right].parent = x_idx;
  }

  tree->nodes[left_child].parent = tree->nodes[x_idx].parent;
  if (tree->nodes[x_idx].parent == -1) {
    tree->root_idx = left_child;
  } else if (tree->nodes[x_idx].parent >= 0 &&
             tree->nodes[x_idx].parent < 1024) {
    if (x_idx == tree->nodes[tree->nodes[x_idx].parent].left) {
      tree->nodes[tree->nodes[x_idx].parent].left = left_child;
    } else {
      tree->nodes[tree->nodes[x_idx].parent].right = left_child;
    }
  }

  tree->nodes[left_child].right = x_idx;
  tree->nodes[x_idx].parent = left_child;
}

static void insert_fixup(struct rb_tree *tree, int z_idx) {
#pragma unroll
  for (int i = 0; i < 24; i++) {
    if (z_idx < 0 || z_idx >= 1024)
      break;

    int p_idx = tree->nodes[z_idx].parent;
    if (p_idx < 0 || p_idx >= 1024)
      break;

    // if parent is black it's okay we can stop
    if (!tree->nodes[p_idx].is_red)
      break;

    int pp_idx = tree->nodes[p_idx].parent; // Grand-parent;
    if (pp_idx < 0 || pp_idx >= 1024)
      break;

    if (p_idx == tree->nodes[pp_idx].left) {
      int unc_idx = tree->nodes[pp_idx].right; // Uncle

      // Case 1 : the uncle is red
      if (unc_idx >= 0 && unc_idx < 1024 &&
          tree->nodes[unc_idx].is_red == true) {
        tree->nodes[p_idx].is_red = false;
        tree->nodes[unc_idx].is_red = false;
        tree->nodes[pp_idx].is_red = true;
        z_idx = pp_idx;
      } else {
        // Case 2 : Z is the right child (Triangle) -> left rotation
        if (z_idx == tree->nodes[p_idx].right) {
          z_idx = p_idx;
          left_rotate(tree, z_idx);

          p_idx = tree->nodes[z_idx].parent;
          if (p_idx >= 0 && p_idx < 1024)
            pp_idx = tree->nodes[p_idx].parent;
          else
            break;
        }
        // Case 3 : Z is a left child (Line) -> right rotation
        if (p_idx >= 0 && p_idx < 1024)
          tree->nodes[p_idx].is_red = false;
        if (pp_idx >= 0 && pp_idx < 1024) {
          tree->nodes[pp_idx].is_red = true;
          right_rotate(tree, pp_idx);
        }
      }
    } else {
      int unc_idx = tree->nodes[pp_idx].left;

      // Case 1
      if (unc_idx >= 0 && unc_idx < 1024 &&
          tree->nodes[unc_idx].is_red == true) {
        tree->nodes[p_idx].is_red = false;
        tree->nodes[unc_idx].is_red = false;
        tree->nodes[pp_idx].is_red = true;
        z_idx = pp_idx;
      } else {
        // Case 2
        if (z_idx == tree->nodes[p_idx].right) {
          z_idx = p_idx;
          left_rotate(tree, z_idx);

          p_idx = tree->nodes[z_idx].parent;
          if (p_idx >= 0 && p_idx < 1024)
            pp_idx = tree->nodes[p_idx].parent;
          else
            break;
        }
        // Case 3
        if (p_idx >= 0 && p_idx < 1024)
          tree->nodes[p_idx].is_red = false;
        if (pp_idx >= 0 && pp_idx < 1024) {
          tree->nodes[pp_idx].is_red = true;
          right_rotate(tree, pp_idx);
        }
      }
    }
  }

  // Root has to be black
  if (tree->root_idx >= 0 && tree->root_idx < 1024) {
    tree->nodes[tree->root_idx].is_red = false;
  }
}

static int bst_insert_pid(int pid) {
  u32 key = 0;
  struct rb_tree *tree = bpf_map_lookup_elem(&tree_map, &key);
  if (!tree)
    return -1;

  bpf_spin_lock(&tree->lock);

  // If tree is empty, we create the root
  if (tree->root_idx == -1) {
    int new_idx = create_node(tree, pid);
    if (new_idx >= 0)
      tree->root_idx = new_idx;
    bpf_spin_unlock(&tree->lock);
    return new_idx >= 0 ? 0 : -1;
  }

  int curr_idx = tree->root_idx;
  int parent_idx = -1;
  bool found_leaf = false;

  // For loop because bpf doesn't allow us to make loops with unclear endpoint
  for (int i = 0; i < 64; i++) {
    // We found were we want to insert
    if (curr_idx < 0 || curr_idx >= 1024) {
      found_leaf = true;
      break;
    }

    parent_idx = curr_idx;
    struct bst_node *node = &tree->nodes[curr_idx];

    if (pid == node->pid) {
      bpf_spin_unlock(&tree->lock);
      // if we already have this pid, we don't do anything
      return 0;
    }

    if (pid < node->pid)
      curr_idx = node->left;
    else
      curr_idx = node->right;
  }

  // If we went further than the loop, the tree is to big, we refuse the
  // insertion
  if (!found_leaf) {
    bpf_spin_unlock(&tree->lock);
    return -1;
  }

  // We know that we can insert, so we can consume a node
  int new_idx = create_node(tree, pid);
  if (new_idx == -1) {
    bpf_spin_unlock(&tree->lock);
    return -1;
  }

  // We insert the node
  if (pid < tree->nodes[parent_idx].pid)
    tree->nodes[parent_idx].left = new_idx;
  else
    tree->nodes[parent_idx].right = new_idx;

  tree->nodes[new_idx].parent = parent_idx;

  insert_fixup(tree, new_idx);

  bpf_spin_unlock(&tree->lock);
  return 0;
}

static inline void delete_fixup(struct rb_tree *tree, int x_idx,
                                int parent_idx) {
#pragma unroll
  for (int i = 0; i < 24; i++) {
    // Condition d'arrêt : X est rouge ou X est la racine
    if (x_idx == tree->root_idx ||
        (x_idx >= 0 && x_idx < 1024 && tree->nodes[x_idx].is_red == true)) {
      if (x_idx >= 0 && x_idx < 1024)
        tree->nodes[x_idx].is_red = 0;
      break;
    }

    if (parent_idx < 0 || parent_idx >= 1024)
      break;

    if (x_idx == tree->nodes[parent_idx].left) {
      int w_idx = tree->nodes[parent_idx].right; // Frère
      if (w_idx < 0 || w_idx >= 1024)
        break;

      if (tree->nodes[w_idx].is_red == 1) { // Cas 1 : Frère rouge
        tree->nodes[w_idx].is_red = 0;
        tree->nodes[parent_idx].is_red = 1;
        left_rotate(tree, parent_idx);
        w_idx = tree->nodes[parent_idx].right;
      }

    } else {
      int w_idx = tree->nodes[parent_idx].left;
      if (w_idx < 0 || w_idx >= 1024)
        break;

      if (tree->nodes[w_idx].is_red == 1) { // Cas 1 : Frère rouge
        tree->nodes[w_idx].is_red = 0;
        tree->nodes[parent_idx].is_red = 1;
        left_rotate(tree, parent_idx);
        w_idx = tree->nodes[parent_idx].left;
      }
    }
  }
}

static int bst_pop_min_pid() {
  u32 key = 0;
  struct rb_tree *tree = bpf_map_lookup_elem(&tree_map, &key);
  if (!tree)
    return -1;

  bpf_spin_lock(&tree->lock);

  // If we have nothing in the tree
  if (tree->root_idx == -1) {
    bpf_spin_unlock(&tree->lock);
    return -1;
  }

  int curr_idx = tree->root_idx;
  int parent_idx = -1;
  int found_pid = -1;

  for (int i = 0; i < 16; i++) {
    if (curr_idx < 0 || curr_idx >= 1024)
      break;

    struct bst_node *curr_node = &tree->nodes[curr_idx];

    if (curr_node->left == -1) {
      found_pid = curr_node->pid;
      int right_child = curr_node->right;

      // If we are at the root of the tree
      if (parent_idx == -1)
        tree->root_idx = right_child;
      // Since we remove the smallest, we know there is nothing on the left,
      // only the right child is considered
      else
        tree->nodes[parent_idx].left = right_child;

      if (right_child >= 0 && right_child < 1024) {
        tree->nodes[right_child].parent = parent_idx;
      }

      // If the stack is not full, we give back the spot
      if (tree->free_top >= 0 && tree->free_top < 1024) {
        tree->free_stack[tree->free_top] = curr_idx;
        tree->free_top++;
      }
      break;
    }
    parent_idx = curr_idx;
    curr_idx = curr_node->left;
    if (curr_idx >= 0 && curr_idx < 1024)
      delete_fixup(tree, curr_idx, tree->nodes[curr_idx].parent);
  }

  bpf_spin_unlock(&tree->lock);

  return found_pid;
}

static int bst_pop_max_pid() {
  u32 key = 0;
  struct rb_tree *tree = bpf_map_lookup_elem(&tree_map, &key);
  if (!tree)
    return -1;

  bpf_spin_lock(&tree->lock);

  // If we have nothing in the tree
  if (tree->root_idx == -1) {
    bpf_spin_unlock(&tree->lock);
    return -1;
  }

  int curr_idx = tree->root_idx;
  int parent_idx = -1;
  int found_pid = -1;

  for (int i = 0; i < 64; i++) {
    if (curr_idx < 0 || curr_idx >= 1024)
      break;

    struct bst_node *curr_node = &tree->nodes[curr_idx];

    if (curr_node->right == -1) {
      found_pid = curr_node->pid;
      int left_child = curr_node->left;

      if (parent_idx == -1)
        tree->root_idx = left_child;
      else
        tree->nodes[parent_idx].right = left_child;

      if (left_child >= 0 && left_child < 1024) {
        tree->nodes[left_child].parent = parent_idx;
      }

      // If the stack is not full, we give back the spot
      if (tree->free_top >= 0 && tree->free_top < 1024) {
        tree->free_stack[tree->free_top] = curr_idx;
        tree->free_top++;
      }
      break;
    }
    parent_idx = curr_idx;
    curr_idx = curr_node->right;
  }

  bpf_spin_unlock(&tree->lock);

  return found_pid;
}

static void stat_inc(u32 idx, pid_t value) {
  u64 *cnt_p = bpf_map_lookup_elem(&stats, &idx);
  if (cnt_p && value == 0)
    (*cnt_p)++;
  if (cnt_p && value > 0)
    (*cnt_p) = value;
}

s32 BPF_STRUCT_OPS(ksched_select_cpu, struct task_struct *p, s32 prev_cpu,
                   u64 wake_flags) {
  bool is_idle = false;
  s32 cpu;

  cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &is_idle);
  // if (is_idle) {
  // 	if (bst_insert_pid(p->pid) == 0) {
  // 		// add insert time to
  // 		u64 start_time = bpf_ktime_get_ns();
  // 		u32 pid_map = p->pid;
  // 		bpf_map_update_elem(&latency_map, &pid_map, &start_time,
  // 				    BPF_NOEXIST);
  //
  // 		stat_inc(0, 0); /* count local queueing */
  // 		bpf_printk("IABR: Insertion DSQ locale %d", p->pid);
  // 		stat_inc(2, p->pid);
  // 	}
  // }

  return cpu;
}

void BPF_STRUCT_OPS(ksched_enqueue, struct task_struct *p, u64 enq_flags) {
  stat_inc(1, 0); /* count global queueing */

  u64 now = bpf_ktime_get_ns();
  u32 pid = p->pid;

  bpf_map_update_elem(&latency_map, &pid, &now, BPF_ANY);

  if (bst_insert_pid(p->pid) == 0) {
    bpf_printk("IABR: Insertion du PID %d", p->pid);
    stat_inc(2, p->pid);
  } else {
    bpf_printk("ATTENTION enqueue a echoue");
  }
}

void BPF_STRUCT_OPS(ksched_dispatch, s32 cpu, struct task_struct *prev) {
  // debug nodes array :
  // 	u32 key = 0;
  // 	struct rb_tree *tree = bpf_map_lookup_elem(&tree_map, &key);
  // 	if (!tree)
  // 		return;
  //
  // 	// LOGGING TRIGGER: Only dump the tree once every 1 billion nanoseconds
  // (1s) 	static u64 last_dump = 0; 	u64 now = bpf_ktime_get_ns();
  //
  // 	bpf_spin_lock(&tree->lock);
  //
  // 	if (now - last_dump > 1000000000) {
  // 		last_dump = now;
  //
  // 		bpf_printk("--- TREE SNAPSHOT (Root: %d) ---", tree->root_idx);
  //
  // 		// Simple iterative dump (Right -> Node -> Left for a vertical
  // look) 		int curr = tree->root_idx; #pragma unroll 16
  // for (int i = 0; i < 16; i++) { 			if (curr < 0 || curr >=
  // 1024) 				break; 			struct bst_node
  // *n = &tree->nodes[curr]; 			bpf_printk("  [%d] PID: %d (L:%d
  // R:%d P:%d)", curr, 				   n->pid, n->left, n->right, n->parent);
  //
  // 			// For a simple visual, just follow the left-most path
  // 			// or modify to use the stack-traversal provided
  // previously 			curr = n->left;
  // 		}
  // 	}
  //
  // 	bpf_spin_unlock(&tree->lock);

  int pid_to_run = bst_pop_min_pid();

  if (pid_to_run != -1) {
    u64 now = bpf_ktime_get_ns();
    u64 *start_time = bpf_map_lookup_elem(&latency_map, &pid_to_run);

    if (start_time) {
      u64 diff = now - *start_time;

      u32 key = 0;
      struct sched_stats *lat_stats = bpf_map_lookup_elem(&stats, &key);

      if (lat_stats) {
        // Add the latency to the running total
        __sync_fetch_and_add(&lat_stats->total_latency, diff);
        // Increment the number of tasks processed
        __sync_fetch_and_add(&lat_stats->task_count, 1);
      }

      // Clean up the map
      bpf_map_delete_elem(&latency_map, &pid_to_run);
    }

    struct task_struct *p = bpf_task_from_pid(pid_to_run);
    if (p) {
      scx_bpf_dsq_insert(p, SCX_DSQ_LOCAL, SCX_SLICE_DFL, 0);

      bpf_printk("DABR: Dispatching PID %d to CPU %d", pid_to_run, cpu);

      u32 key = 0;
      struct rb_tree *tree = bpf_map_lookup_elem(&tree_map, &key);

      bpf_printk("ROOT root of abr : %d", tree->root_idx);

      if (tree->root_idx >= 0 && tree->root_idx < 1024)
        bpf_printk("ROOT of abr : %d", tree->nodes[tree->root_idx].pid);

      bpf_task_release(p);
    }
  }
}

void BPF_STRUCT_OPS(ksched_running, struct task_struct *p) {
  if (min_pid)
    return;

  /*
   * Global vtime always progresses forward as tasks start executing. The
   * test and update can be performed concurrently from multiple CPUs and
   * thus racy. Any error should be contained and temporary. Let's just
   * live with it.
   */
  if (time_before(vtime_now, p->scx.dsq_vtime))
    vtime_now = p->scx.dsq_vtime;
}

void BPF_STRUCT_OPS(ksched_stopping, struct task_struct *p, bool runnable) {
  if (min_pid) {
    return;
  }

  /*
   * Scale the execution time by the inverse of the weight and charge.
   *
   * Note that the default yield implementation yields by setting
   * @p->scx.slice to zero and the following would treat the yielding task
   * as if it has consumed all its slice. If this penalizes yielding tasks
   * too much, determine the execution time by taking explicit timestamps
   * instead of depending on @p->scx.slice.
   */
  p->scx.dsq_vtime += (SCX_SLICE_DFL - p->scx.slice) * 100 / p->scx.weight;
}

void BPF_STRUCT_OPS(ksched_enable, struct task_struct *p) {
  p->scx.dsq_vtime = vtime_now;
}

s32 BPF_STRUCT_OPS_SLEEPABLE(ksched_init) {
  return scx_bpf_create_dsq(SHARED_DSQ, -1);
}

void BPF_STRUCT_OPS(ksched_exit, struct scx_exit_info *ei) {
  UEI_RECORD(uei, ei);
}

SCX_OPS_DEFINE(ksched_ops, .select_cpu = (void *)ksched_select_cpu,
               .enqueue = (void *)ksched_enqueue,
               .dispatch = (void *)ksched_dispatch,
               .running = (void *)ksched_running,
               .stopping = (void *)ksched_stopping,
               .enable = (void *)ksched_enable, .init = (void *)ksched_init,
               .flags = SCX_OPS_SWITCH_PARTIAL,
               // .init_task = (void *)ksched_init_task,
               .exit = (void *)ksched_exit, .name = "ksched",
               .timeout_ms = 30000);

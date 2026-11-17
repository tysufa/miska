/* SPDX-License-Identifier: GPL-2.0 */
/*
 * A simple scheduler.
 *
 * By default, it operates as a simple global weighted vtime scheduler and can
 * be switched to FIFO scheduling. It also demonstrates the following niceties.
 *
 * - Statistics tracking how many tasks are queued to local and global dsq's.
 * - Termination notification for userspace.
 *
 * While very simple, this scheduler should work reasonably well on CPUs with a
 * uniform L3 cache topology. While preemption is not implemented, the fact that
 * the scheduling queue is shared across all CPUs means that whatever is at the
 * front of the queue is likely to be executed fairly quickly given enough
 * number of CPUs. The FIFO scheduling mode may be beneficial to some workloads
 * but comes with the usual problems with FIFO scheduling where saturating
 * threads can easily drown out interactive ones.
 *
 * Copyright (c) 2022 Meta Platforms, Inc. and affiliates.
 * Copyright (c) 2022 Tejun Heo <tj@kernel.org>
 * Copyright (c) 2022 David Vernet <dvernet@meta.com>
 */
 
#include <scx/common.bpf.h>


#define SEUIL_DYNA 3000000


// ma struct qui représente le message

struct task_event{
	pid_t pid;
	char comm[16]; // nom du processus
	char prio; // s'il est prioritaire ou pas
};

// ringBuffer pour la communication
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 40 * 1024 * 1024);
} rb SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(s32));
	__uint(max_entries, 1);		
} decision_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u_char));
	__uint(max_entries, 64);		
} idle_map SEC(".maps");


struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 500);		
	__type(key, u32);
	__type(value, u64);
} time_exuted SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 500);		
	__type(key, u32);
	__type(value, u64);
} latence_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 500);		
	__type(key, u32);
	__type(value, u64);
} sum_latence_map SEC(".maps");


// maps pour la dyna
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 500);		
	__type(key, u32);
	__type(value, u64);
} time_from_last_switch SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 500);		
	__type(key, u32);
	__type(value, u8);
} prio SEC(".maps");

// 1 prioritaire, 0 pas prioritaire


char _license[] SEC("license") = "GPL";

const volatile bool fifo_sched;

static u64 vtime_now;
UEI_DEFINE(uei);

/*
 * Built-in DSQs such as SCX_DSQ_GLOBAL cannot be used as priority queues
 * (meaning, cannot be dispatched to with scx_bpf_dsq_insert_vtime()). We
 * therefore create a separate DSQ with ID 0 that we dispatch to and consume
 * from. If scx_simple only supported global FIFO scheduling, then we could just
 * use SCX_DSQ_GLOBAL.
 */
#define SHARED_DSQ 0

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(key_size, sizeof(u32));
	__uint(value_size, sizeof(u64));
	__uint(max_entries, 2);			/* [local, global] */
} stats SEC(".maps");

static void stat_inc(u32 idx)
{
	u64 *cnt_p = bpf_map_lookup_elem(&stats, &idx);
	if (cnt_p)
		(*cnt_p)++;
}

s32 BPF_STRUCT_OPS(mine_select_cpu, struct task_struct *p, s32 prev_cpu, u64 wake_flags)
{
	bool is_idle = false;
	s32 cpu;

	cpu = scx_bpf_select_cpu_dfl(p, prev_cpu, wake_flags, &is_idle);

	return cpu;
}


void BPF_STRUCT_OPS(mine_enqueue, struct task_struct *p, u64 enq_flags)
{	

	// envoie un message a l'espace utilisateur
	struct task_event *e;
	e = bpf_ringbuf_reserve(&rb, sizeof(*e), 0);
	u64 time_now = p->utime+p->stime;
	pid_t current_pid = p->pid;
	u64 * last_time = bpf_map_lookup_elem(&time_from_last_switch, &current_pid);
	u8 tmp_prio = 1;
	if (!last_time) {
		bpf_map_update_elem(&time_from_last_switch, &current_pid, &time_now, BPF_ANY);
	}
	else if(time_now - *last_time > SEUIL_DYNA){
		tmp_prio = 0; 
	}
	bpf_map_update_elem(&prio, &current_pid, &tmp_prio, BPF_ANY);

	if (e){
		e->pid = p->pid;
		u8 * tmp_prio = bpf_map_lookup_elem(&prio, &current_pid);
		if (tmp_prio){
			e->prio = *tmp_prio;
		}
		u32 pid_lat = p->pid;
		__u64 timer = bpf_ktime_get_ns();
		bpf_map_update_elem(&latence_map, &pid_lat, &timer, BPF_ANY);
		bpf_probe_read_kernel_str(&e->comm, sizeof(e->comm), p->comm);
		bpf_ringbuf_submit(e, 0);
	}


}

void BPF_STRUCT_OPS(mine_dispatch, s32 cpu, struct task_struct *prev)
{


	 if (scx_bpf_dsq_move_to_local(SHARED_DSQ))
        return;

	u32 key = 0; // on a qu'une case dans notre boite au lettre
	u32 * pid = bpf_map_lookup_elem(&decision_map, &key);
	if (pid && *pid != 0){
		struct task_struct *p = bpf_task_from_pid(*pid);

		if (p){
			// Insère dans la file globale partagée
            scx_bpf_dsq_insert(p, SHARED_DSQ, SCX_SLICE_DFL, 0);

			// sum la latence de cette task
			u64 * zz = bpf_map_lookup_elem(&latence_map, pid);
			if (zz && *zz > 0){
				u64 latence = bpf_ktime_get_ns() - *zz;
				u64 empty = 0;
				bpf_map_update_elem(&latence_map, pid, &empty, BPF_ANY);
				u64 * prevSum = bpf_map_lookup_elem(&sum_latence_map, pid);

				u64 new_sum = latence;
                
                // 4. S'il y avait déjà une somme avant, on l'ajoute
                if (prevSum) {
                    new_sum += *prevSum;
                }
				bpf_map_update_elem(&sum_latence_map, pid, &new_sum, BPF_ANY);
			}

			u8 * task_tmp_prio = bpf_map_lookup_elem(&prio, pid);
			if (task_tmp_prio){
				if (*task_tmp_prio == 0){
					// si lors du dispatch, pas la prio c'est qu'il y a eu un switch
					// donc je change de prio dans la map
					u8 t_prio = 1;
					bpf_map_update_elem(&prio, pid, &t_prio, BPF_ANY);
					u64 new_time = p->utime + p->stime;
					bpf_map_update_elem(&time_from_last_switch, pid, &new_time, BPF_ANY);
				}
			}


			
			bpf_task_release(p); // décrémente le compteur de ref
		}
		
		u32 empty = 0;
		bpf_map_update_elem(&decision_map, &key, &empty, BPF_ANY);

		// 3. Consommer immédiatement ce qu'on vient d'insérer
        scx_bpf_dsq_move_to_local(SHARED_DSQ);
	}else{
		// cas ou le cpu devient idle, on prévient que le cpu est idle au travers de notre map
		key = cpu;
		u_char v = 1;
		bpf_map_update_elem(&idle_map, &key, &v, BPF_ANY);
	}
}

void BPF_STRUCT_OPS(mine_running, struct task_struct *p)
{
	if (fifo_sched)
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

void BPF_STRUCT_OPS(mine_stopping, struct task_struct *p, bool runnable)
{
	if (fifo_sched)
		return;

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

void BPF_STRUCT_OPS(mine_enable, struct task_struct *p)
{
	p->scx.dsq_vtime = vtime_now;
}

s32 BPF_STRUCT_OPS_SLEEPABLE(mine_init)
{
	return scx_bpf_create_dsq(SHARED_DSQ, -1);
}

void BPF_STRUCT_OPS(mine_exit, struct scx_exit_info *ei)
{
	UEI_RECORD(uei, ei);
}

SEC("syscall")
int BPF_PROG(ring_doorbell, u32 cpu) {
    // Force le CPU {cpu} à se réveiller et à appeler mine_dispatch immédiatement
    scx_bpf_kick_cpu(cpu, 0);
	u_char v = 0;
	bpf_map_update_elem(&idle_map, &cpu, &v, BPF_ANY);
    return 0;
}

// SEC("syscall")
// int BPF_PROG(calcul_latence_pid, u32 pid){
// 	// sum la latence de cette task
// 	u64 * zz = bpf_map_lookup_elem(&latence_map, &pid);
// 	if (zz && *zz > 0){
// 		u64 latence = bpf_ktime_get_ns() - *zz;
// 		u64 empty = 0;
// 		bpf_map_update_elem(&latence_map, &pid, &empty, BPF_ANY);
// 		u64 * prevSum = bpf_map_lookup_elem(&sum_latence_map, &pid);

// 		u64 new_sum = latence;
		
// 		// 4. S'il y avait déjà une somme avant, on l'ajoute
// 		if (prevSum) {
// 			new_sum += *prevSum;
// 		}
// 		bpf_map_update_elem(&sum_latence_map, &pid, &new_sum, BPF_ANY);
		
// 	}
// 	return 0;
// }


SCX_OPS_DEFINE(mine_ops,
	       .select_cpu		= (void *)mine_select_cpu,
	       .enqueue			= (void *)mine_enqueue,
	       .dispatch		= (void *)mine_dispatch,
	       .running			= (void *)mine_running,
	       .stopping		= (void *)mine_stopping,
	       .enable			= (void *)mine_enable,
	       .init			= (void *)mine_init,
	       .exit			= (void *)mine_exit,
		   .flags			= SCX_OPS_SWITCH_PARTIAL,
	       .name			= "mine");

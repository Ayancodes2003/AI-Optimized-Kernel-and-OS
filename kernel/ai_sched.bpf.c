/*
 * AIE-OS AI Scheduler - sched_ext eBPF Implementation
 *
 * This scheduler intelligently routes AI and non-AI workloads to heterogeneous
 * compute resources (CPU, GPU, NPU) based on workload classification and energy policies.
 *
 * Key Features:
 * - Per-task AI workload classification (realtime, interactive, batch, background)
 * - Heterogeneous compute routing to CPU (perf/eff), GPU, and NPU
 * - Energy-aware scheduling policies
 * - Telemetry extraction for userspace daemon feedback
 * - Dynamic priority and affinity adjustments
 *
 * Architecture:
 * - kernel/ai_sched.bpf.c (this file): policy and task routing
 * - daemon/aie_daemon.cpp: userspace AI classifier and decision engine
 * - ai/classifier.cpp: ML-based task classification
 * - Dynamic feedback loop: scheduler -> telemetry maps -> daemon -> decision maps -> scheduler
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "ai_sched.h"

/* BPF Macro Helpers */
#define BPF_STRUCT_OPS(name, args...)	\
	SEC("struct_ops/"#name)	BPF_PROG(name, ##args)

#define BPF_STRUCT_OPS_SLEEPABLE(name, args...)	\
	SEC("struct_ops.s/"#name) BPF_PROG(name, ##args)

/* ======================== Dispatch Queue IDs ======================== */

#define DSQ_PERF		0	/* High-performance core cluster */
#define DSQ_EFF			1	/* Energy-efficient core cluster */
#define DSQ_GPU			2	/* GPU workqueue (stub, requires GPU integration) */
#define DSQ_NPU			3	/* NPU workqueue (AMD Ryzen AI, stub) */
#define DSQ_FALLBACK		4	/* Fallback for unclassified tasks */

/* ======================== Maps ======================== */

/* Task telemetry ring buffer for streaming to userspace daemon */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} telemetry_ringbuf SEC(".maps");

/* Scheduling decisions from userspace daemon (PID -> decision) */
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 16384);
	__type(key, __u32);		/* PID */
	__type(value, struct ai_sched_decision);
} sched_decisions SEC(".maps");

/* Energy policy configuration (global) */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, __u32);		/* ai_energy_mode */
} energy_mode SEC(".maps");

/* Per-class time slice defaults (energy mode aware) */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 5);		/* One per task class */
	__type(key, __u32);
	__type(value, __u64);		/* Time slice in nanoseconds */
} time_slices SEC(".maps");

/* Scheduler statistics */
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, struct ai_sched_stats);
} sched_stats SEC(".maps");

/* Energy estimate accumulator per task class */
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 8192);
	__type(key, __u32);		/* PID */
	__type(value, __u64);		/* Energy estimate in millijoules */
} task_energy_estimate SEC(".maps");

/* ======================== Helper Functions ======================== */

/* Get current energy mode policy */
static __always_inline __u32 get_energy_mode(void)
{
	__u32 key = 0;
	__u32 *mode = bpf_map_lookup_elem(&energy_mode, &key);
	return mode ? *mode : AI_ENERGY_BALANCED;
}

/* Get task time slice based on class and energy mode */
static __always_inline __u64 get_time_slice_ns(__u32 task_class)
{
	__u32 key = task_class % 5;
	__u64 *slice = bpf_map_lookup_elem(&time_slices, &key);
	
	if (slice) {
		return *slice;
	}
	
	/* Default slices if not configured */
	switch (task_class) {
	case AI_TASK_CLASS_REALTIME_AI:
		return 1000000ULL;		/* 1ms for low-latency AI */
	case AI_TASK_CLASS_INTERACTIVE_AI:
		return 2000000ULL;		/* 2ms */
	case AI_TASK_CLASS_BATCH_AI:
		return 10000000ULL;		/* 10ms for throughput */
	case AI_TASK_CLASS_BACKGROUND:
		return 20000000ULL;		/* 20ms for background */
	default:
		return 5000000ULL;		/* 5ms default */
	}
}

/* Classify task based on heuristics (fallback if daemon is unavailable) */
static __always_inline __u32 classify_task_fallback(struct task_struct *p,
						     struct ai_task_telemetry *telem)
{
	/* Heuristic classification based on thread count, memory, scheduler class */
	
	/* Realtime threads -> realtime AI (assume realtime task might be AI inference) */
	if (p->__state == TASK_RUNNING && p->prio < 100) {
		return AI_TASK_CLASS_REALTIME_AI;
	}
	
	/* Kernel threads and system tasks -> background */
	if (p->flags & (PF_KTHREAD | PF_IDLE)) {
		return AI_TASK_CLASS_BACKGROUND;
	}
	
	/* Multi-threaded processes with significant memory -> batch AI candidate */
	if (telem->num_threads > 4 && telem->memory_rss_mb > 100) {
		return AI_TASK_CLASS_BATCH_AI;
	}
	
	/* High syscall rate -> interactive */
	if (telem->syscall_count > 100) {
		return AI_TASK_CLASS_INTERACTIVE_AI;
	}
	
	/* Default to unknown */
	return AI_TASK_CLASS_UNKNOWN;
}

/* Select dispatch queue based on task class and energy policy */
static __always_inline __u64 select_dsq(__u32 task_class, __u32 preferred_device)
{
	__u32 energy = get_energy_mode();
	
	/* If daemon provided explicit preference, respect it */
	if (preferred_device != AI_DEVICE_AUTO) {
		switch (preferred_device) {
		case AI_DEVICE_PERF_CORE:
			return DSQ_PERF;
		case AI_DEVICE_EFF_CORE:
			return DSQ_EFF;
		case AI_DEVICE_GPU:
			return DSQ_GPU;		/* May be stubbed */
		case AI_DEVICE_NPU:
			return DSQ_NPU;		/* May be stubbed */
		default:
			break;
		}
	}
	
	/* Dynamic selection based on task class and energy mode */
	switch (task_class) {
	case AI_TASK_CLASS_REALTIME_AI:
		/* Always prefer performance cores for lowest latency */
		return DSQ_PERF;
	
	case AI_TASK_CLASS_INTERACTIVE_AI:
		/* Prefer perf cores, but can use efficiency if short-lived */
		return DSQ_PERF;
	
	case AI_TASK_CLASS_BATCH_AI:
		/* Batch workloads are flexible; energy mode decides */
		if (energy == AI_ENERGY_EFFICIENT || energy == AI_ENERGY_POWER_SAVER) {
			return DSQ_EFF;
		}
		return DSQ_PERF;
	
	case AI_TASK_CLASS_BACKGROUND:
		/* Background always prefers efficiency cores */
		return DSQ_EFF;
	
	default:
		/* Unknown tasks go to fallback */
		return DSQ_FALLBACK;
	}
}

/* Capture task telemetry into telemetry ring buffer */
static __always_inline void emit_telemetry(struct task_struct *p, __u64 enqueue_ts)
{
	struct ai_task_telemetry *telem;
	
	telem = bpf_ringbuf_reserve(&telemetry_ringbuf, sizeof(*telem), 0);
	if (!telem) {
		return;		/* Ringbuf full, drop sample */
	}
	
	telem->pid = bpf_get_current_pid_tgid() >> 32;
	telem->uid = bpf_get_current_uid_gid() & 0xFFFFFFFF;
	telem->ts_enqueue = bpf_ktime_get_ns();
	
	/* TODO: Extract actual metrics from task_struct and kernel interfaces
	 * - CPU utilization from task_struct->se.avg
	 * - Memory from task_struct->mm->rss_stat
	 * - Syscalls from task_struct instrumentation or tracepoint
	 * - I/O from task_struct->ioac or blk tracepoints
	 */
	telem->cpu_util_recent = 0;	/* Placeholder */
	telem->memory_rss_mb = 0;	/* Placeholder */
	telem->syscall_count = 0;	/* Placeholder */
	telem->io_read_bytes = 0;	/* Placeholder */
	telem->io_write_bytes = 0;	/* Placeholder */
	telem->num_threads = 1;		/* Placeholder */
	telem->nice_value = p->static_prio - 120;
	telem->sched_class = p->policy;
	
	__builtin_memcpy(telem->comm, p->comm, sizeof(telem->comm));
	
	bpf_ringbuf_submit(telem, 0);
}

/* Update scheduler statistics */
static __always_inline void update_stats(__u32 task_class, __u32 target_dsq)
{
	struct ai_sched_stats *stats;
	__u32 key = 0;
	
	stats = bpf_map_lookup_elem(&sched_stats, &key);
	if (!stats) {
		return;
	}
	
	__sync_fetch_and_add(&stats->tasks_enqueued, 1);
	
	/* Track routing decisions */
	if (target_dsq == DSQ_PERF && task_class < AI_TASK_CLASS_BACKGROUND) {
		__sync_fetch_and_add(&stats->tasks_ai_routed_cpu, 1);
	} else if (target_dsq == DSQ_NPU) {
		__sync_fetch_and_add(&stats->tasks_ai_routed_npu, 1);
	} else if (target_dsq == DSQ_EFF) {
		__sync_fetch_and_add(&stats->tasks_bg_routed_eff, 1);
	}
}

/* ======================== Scheduler Hooks ======================== */

/*
 * sched_init: Initialize scheduler state
 * Called once at scheduler attachment
 */
s32 BPF_STRUCT_OPS_SLEEPABLE(ai_sched_init)
{
	__u32 key = 0;
	__u32 mode = AI_ENERGY_BALANCED;
	struct ai_sched_stats empty_stats = {};
	
	/* Create dispatch queues for each compute resource */
	scx_bpf_create_dsq(DSQ_PERF, -1);
	scx_bpf_create_dsq(DSQ_EFF, -1);
	scx_bpf_create_dsq(DSQ_GPU, -1);
	scx_bpf_create_dsq(DSQ_NPU, -1);
	scx_bpf_create_dsq(DSQ_FALLBACK, -1);
	
	/* Initialize energy mode */
	bpf_map_update_elem(&energy_mode, &key, &mode, 0);
	
	/* Initialize statistics */
	bpf_map_update_elem(&sched_stats, &key, &empty_stats, 0);
	
	return 0;
}

/*
 * ai_sched_enqueue: Enqueue a task to appropriate queue
 *
 * Decision flow:
 * 1. Check daemon for pre-computed decision (fast path)
 * 2. Otherwise, apply heuristic classification
 * 3. Emit telemetry for offline daemon learning
 * 4. Dispatch to appropriate DSQ based on task class and energy mode
 */
int BPF_STRUCT_OPS(ai_sched_enqueue, struct task_struct *p, u64 enq_flags)
{
	__u32 pid = p->tgid;
	__u32 task_class = AI_TASK_CLASS_UNKNOWN;
	__u32 preferred_device = AI_DEVICE_AUTO;
	__u64 time_slice_ns;
	__u64 target_dsq;
	struct ai_sched_decision *decision;
	struct ai_task_telemetry telem = {};
	
	/* Check if daemon has a pre-computed decision for this PID */
	decision = bpf_map_lookup_elem(&sched_decisions, &pid);
	if (decision) {
		task_class = decision->task_class;
		preferred_device = decision->preferred_device;
		time_slice_ns = decision->time_slice_us * 1000ULL;
	} else {
		/* Fallback to heuristic classification */
		telem.pid = pid;
		task_class = classify_task_fallback(p, &telem);
		time_slice_ns = get_time_slice_ns(task_class);
	}
	
	/* Emit telemetry for daemon learning */
	emit_telemetry(p, bpf_ktime_get_ns());
	
	/* Select target dispatch queue */
	target_dsq = select_dsq(task_class, preferred_device);
	
	/* Update statistics */
	update_stats(task_class, target_dsq);
	
	/* Dispatch to selected queue with appropriate time slice */
	scx_bpf_dsq_insert(p, target_dsq, time_slice_ns, enq_flags);
	
	return 0;
}

/*
 * ai_sched_dispatch: Move tasks from dispatch queues to CPU runqueues
 *
 * Decision: For now, use simple load-balanced dispatch across CPUs.
 * Future enhancements:
 * - Prefer specific CPU cores (P/E cluster placement)
 * - Respect NUMA affinity
 * - GPU/NPU dispatch handling
 */
int BPF_STRUCT_OPS(ai_sched_dispatch, s32 cpu, struct task_struct *prev)
{
	/* Try performance core queue first */
	if (scx_bpf_dsq_move_to_local(DSQ_PERF)) {
		return 0;
	}
	
	/* Then efficiency core queue */
	if (scx_bpf_dsq_move_to_local(DSQ_EFF)) {
		return 0;
	}
	
	/* Then GPU queue (currently stubbed) */
	if (scx_bpf_dsq_move_to_local(DSQ_GPU)) {
		return 0;
	}
	
	/* Then NPU queue (currently stubbed) */
	if (scx_bpf_dsq_move_to_local(DSQ_NPU)) {
		return 0;
	}
	
	/* Finally fallback queue */
	if (scx_bpf_dsq_move_to_local(DSQ_FALLBACK)) {
		return 0;
	}
	
	return 0;
}

/* ======================== Scheduler Registration ======================== */

SEC(".struct_ops.link")
struct sched_ext_ops sched_ops = {
	.enqueue	= (void *)ai_sched_enqueue,
	.dispatch	= (void *)ai_sched_dispatch,
	.init		= (void *)ai_sched_init,
	.flags		= SCX_OPS_ENQ_LAST | SCX_OPS_KEEP_BUILTIN_IDLE,
	.name		= "aie_scheduler"
};

char _license[] SEC("license") = "GPL";

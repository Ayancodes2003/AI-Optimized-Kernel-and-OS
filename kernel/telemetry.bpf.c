/*
 * AIE-OS Telemetry Extraction - eBPF Instrumentation
 *
 * This module captures detailed runtime telemetry about task behavior for use by
 * the userspace AI classifier daemon. It hooks into:
 * - Task lifecycle events (fork, exec, exit)
 * - System calls (for interactive vs batch classification)
 * - Memory pressure
 * - I/O patterns
 *
 * Telemetry is exported via ring buffers to userspace daemon with minimal overhead.
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "ai_sched.h"

/* Ring buffer for streaming telemetry samples */
struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 512 * 1024);
} telemetry_events SEC(".maps");

/* Per-task telemetry accumulator (PID -> counters) */
struct task_telemetry_accum {
	__u64 total_syscalls;
	__u64 io_read_bytes;
	__u64 io_write_bytes;
	__u32 last_sample_cpu_util;
	__u64 last_sample_ts;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 16384);
	__type(key, __u32);		/* PID */
	__type(value, struct task_telemetry_accum);
} task_accumulators SEC(".maps");

/* ======================== Syscall Tracing ======================== */

/*
 * Trace entry of any syscall
 * Used to count syscall rate, infer interactivity
 */
TRACEPOINT_PROBE(raw_syscalls, sys_enter)
{
	__u32 pid = bpf_get_current_pid_tgid() >> 32;
	struct task_telemetry_accum *accum;
	struct task_telemetry_accum zero = {};
	
	accum = bpf_map_lookup_elem(&task_accumulators, &pid);
	if (!accum) {
		bpf_map_update_elem(&task_accumulators, &pid, &zero, 0);
		accum = bpf_map_lookup_elem(&task_accumulators, &pid);
		if (!accum) {
			return 0;
		}
	}
	
	__sync_fetch_and_add(&accum->total_syscalls, 1);
	
	return 0;
}

/* ======================== I/O Tracing ======================== */

/*
 * Trace block I/O submissions
 * Capture read/write patterns for workload characterization
 */
TRACEPOINT_PROBE(block, block_rq_issue)
{
	__u32 pid = bpf_get_current_pid_tgid() >> 32;
	struct task_telemetry_accum *accum;
	struct task_telemetry_accum zero = {};
	
	accum = bpf_map_lookup_elem(&task_accumulators, &pid);
	if (!accum) {
		bpf_map_update_elem(&task_accumulators, &pid, &zero, 0);
		accum = bpf_map_lookup_elem(&task_accumulators, &pid);
		if (!accum) {
			return 0;
		}
	}
	
	/* TODO: Extract op (read vs write) and bytes from tracepoint struct
	 * Placeholder implementation just counts events
	 */
	
	return 0;
}

/* ======================== Memory Events ======================== */

/*
 * Monitor page faults to detect memory-intensive workloads
 * TODO: Hook into page fault handler for detailed memory tracking
 */

/* ======================== Task Lifecycle ======================== */

/*
 * Track task creation for initial telemetry setup
 */
TRACEPOINT_PROBE(sched, sched_process_fork)
{
	__u32 parent_pid = args->parent_pid;
	__u32 child_pid = args->child_pid;
	struct task_telemetry_accum zero = {};
	
	/* Pre-allocate accumulator for child */
	bpf_map_update_elem(&task_accumulators, &child_pid, &zero, 0);
	
	return 0;
}

/*
 * Clean up telemetry on task exit
 */
TRACEPOINT_PROBE(sched, sched_process_exit)
{
	__u32 pid = args->pid;
	
	bpf_map_delete_elem(&task_accumulators, &pid);
	
	return 0;
}

/* ======================== Sampling ======================== */

/*
 * Periodic sampling of task statistics
 * Triggered by timer events (e.g., scheduler tick) or external interfaces
 * Updates ring buffer with periodic telemetry snapshots
 */
int sample_task_telemetry(__u32 pid, struct task_struct *p)
{
	struct ai_task_telemetry *sample;
	struct task_telemetry_accum *accum;
	
	sample = bpf_ringbuf_reserve(&telemetry_events, sizeof(*sample), 0);
	if (!sample) {
		return -1;
	}
	
	accum = bpf_map_lookup_elem(&task_accumulators, &pid);
	
	sample->pid = pid;
	sample->ts_enqueue = bpf_ktime_get_ns();
	
	if (accum) {
		sample->syscall_count = accum->total_syscalls;
		sample->io_read_bytes = accum->io_read_bytes;
		sample->io_write_bytes = accum->io_write_bytes;
	}
	
	/* Extract from task_struct where available */
	if (p) {
		sample->nice_value = p->static_prio - 120;
		sample->sched_class = p->policy;
		__builtin_memcpy(sample->comm, p->comm, sizeof(sample->comm));
	}
	
	bpf_ringbuf_submit(sample, 0);
	
	return 0;
}

char _license[] SEC("license") = "GPL";

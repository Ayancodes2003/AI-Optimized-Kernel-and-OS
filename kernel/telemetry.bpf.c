/*
 * AIE-OS Telemetry Extraction - eBPF Instrumentation
 */

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "ai_sched.h"

/* SINGLE ring buffer — THIS NAME MUST MATCH DAEMON */
struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1 << 24);
} telemetry_ringbuf SEC(".maps");

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
	__type(key, __u32);
	__type(value, struct task_telemetry_accum);
} task_accumulators SEC(".maps");

/* Syscall tracepoint args */
struct sys_enter_args {
	__u64 pad;
	long syscall_nr;
	long args[6];
};

/* Block tracepoint args */
struct block_rq_issue_args {
	__u64 pad;
	__u32 dev;
	__u64 sector;
	__u32 nr_sector;
	__u32 bytes;
	char rwbs[8];
	char comm[16];
};

/* Sched fork args */
struct sched_process_fork_args {
	__u64 pad;
	char parent_comm[16];
	__u32 parent_pid;
	char child_comm[16];
	__u32 child_pid;
};

/* Sched exit args */
struct sched_process_exit_args {
	__u64 pad;
	char comm[16];
	__u32 pid;
	int prio;
};

SEC("tp/raw_syscalls/sys_enter")
int handle_sys_enter(struct sys_enter_args *args)
{
	__u32 pid = bpf_get_current_pid_tgid() >> 32;
	struct task_telemetry_accum *accum;
	struct task_telemetry_accum zero = {};

	accum = bpf_map_lookup_elem(&task_accumulators, &pid);
	if (!accum) {
		bpf_map_update_elem(&task_accumulators, &pid, &zero, 0);
		accum = bpf_map_lookup_elem(&task_accumulators, &pid);
		if (!accum)
			return 0;
	}
	__sync_fetch_and_add(&accum->total_syscalls, 1);
	return 0;
}

SEC("tp/block/block_rq_issue")
int handle_block_rq_issue(struct block_rq_issue_args *args)
{
	__u32 pid = bpf_get_current_pid_tgid() >> 32;
	struct task_telemetry_accum *accum;
	struct task_telemetry_accum zero = {};

	accum = bpf_map_lookup_elem(&task_accumulators, &pid);
	if (!accum) {
		bpf_map_update_elem(&task_accumulators, &pid, &zero, 0);
		accum = bpf_map_lookup_elem(&task_accumulators, &pid);
		if (!accum)
			return 0;
	}
	return 0;
}

SEC("tp/sched/sched_process_fork")
int handle_sched_fork(struct sched_process_fork_args *args)
{
        __u32 child_pid = args->child_pid;

        /* Initialize accumulator */
        struct task_telemetry_accum zero = {};
        bpf_map_update_elem(&task_accumulators, &child_pid, &zero, 0);

        /* Send telemetry event to userspace */
        struct ai_task_telemetry *event;

        event = bpf_ringbuf_reserve(&telemetry_ringbuf, sizeof(*event), 0);
        if (!event)
                return 0;

        event->pid = child_pid;
        event->cpu_util_recent = 60;
        event->memory_rss_mb = 300;
        event->num_threads = 4;
        event->syscall_count = 250;
        event->sched_class = 1;
        event->nice_value = 0;

        bpf_ringbuf_submit(event, 0);

        return 0;
}

SEC("tp/sched/sched_process_exit")
int handle_sched_exit(struct sched_process_exit_args *args)
{
	__u32 pid = args->pid;
	bpf_map_delete_elem(&task_accumulators, &pid);
	return 0;
}

char _license[] SEC("license") = "GPL";

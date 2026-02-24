/*
 * AIE-OS AI Scheduler Shared Definitions
 * Kernel-userspace interface for AI workload management
 *
 * This header defines common types and interfaces used by the eBPF scheduler kernel
 * and the userspace daemon for AI workload classification and routing.
 */

#ifndef __AIE_OS_AI_SCHED_H
#define __AIE_OS_AI_SCHED_H

#include <linux/types.h>

/*
 * AI Workload Classification
 * Determined by runtime telemetry (syscall patterns, memory behavior, CPU utilization)
 */
enum ai_task_class {
	AI_TASK_CLASS_REALTIME_AI = 0,		/* Interactive AI inference, strict latency */
	AI_TASK_CLASS_INTERACTIVE_AI = 1,	/* Responsive AI, user-facing */
	AI_TASK_CLASS_BATCH_AI = 2,		/* Training/batch processing, throughput focus */
	AI_TASK_CLASS_BACKGROUND = 3,		/* Non-AI background tasks */
	AI_TASK_CLASS_UNKNOWN = 4,		/* Unclassified or generic workload */
};

/*
 * Compute Device Target
 * Where the scheduler prefers to route the task
 */
enum ai_compute_device {
	AI_DEVICE_PERF_CORE = 0,		/* High-performance CPU core cluster */
	AI_DEVICE_EFF_CORE = 1,		/* Energy-efficient CPU core cluster */
	AI_DEVICE_GPU = 2,			/* GPU accelerator */
	AI_DEVICE_NPU = 3,			/* AMD Ryzen AI NPU (preferred for AI inference) */
	AI_DEVICE_AUTO = 4,			/* Scheduler decides dynamically */
};

/*
 * Energy Policy Mode
 * High-level energy vs performance trade-off
 */
enum ai_energy_mode {
	AI_ENERGY_PERFORMANCE = 0,		/* Maximize throughput, less concern for power */
	AI_ENERGY_BALANCED = 1,			/* Default: balance power and performance */
	AI_ENERGY_EFFICIENT = 2,		/* Maximize efficiency, accept latency trade-off */
	AI_ENERGY_POWER_SAVER = 3,		/* Minimal power consumption */
};

/*
 * Task Telemetry Snapshot
 * Captured at enqueue time by eBPF scheduler
 * Used by userspace daemon for classification
 */
struct ai_task_telemetry {
	__u32 pid;				/* Process ID */
	__u32 uid;				/* User ID */
	__u64 ts_enqueue;			/* Enqueue timestamp (nanoseconds) */
	__u32 cpu_util_recent;			/* Recent CPU utilization % (0-100) */
	__u32 memory_rss_mb;			/* Resident memory in MB */
	__u64 syscall_count;			/* System call count since last sample */
	__u32 io_read_bytes;			/* I/O read bytes this quantum */
	__u32 io_write_bytes;			/* I/O write bytes this quantum */
	__u32 num_threads;			/* Thread count in process */
	__u32 nice_value;			/* Task priority hint */
	__u32 sched_class;			/* Kernel SCHED_* class flag */
	char comm[16];				/* Task name/command string */
};

/*
 * Scheduling Decision (from daemon)
 * Returned by userspace AI classifier and applied by kernel scheduler
 */
struct ai_sched_decision {
	__u32 pid;				/* Process ID this decision applies to */
	__u32 task_class;			/* Assigned ai_task_class */
	__u32 preferred_device;			/* ai_compute_device preference */
	__u32 cpu_affinity_mask;		/* CPU binding preference (if applicable) */
	__u32 time_slice_us;			/* Requested time slice in microseconds */
	__u32 priority_boost;			/* Priority adjustment (-20 to +20 range) */
	__u64 ts_decision;			/* Decision timestamp */
};

/*
 * Scheduler Statistics (exported to userspace)
 * Provides visibility into scheduler behavior
 */
struct ai_sched_stats {
	__u64 tasks_enqueued;			/* Total tasks enqueued */
	__u64 tasks_dispatched;			/* Total tasks dispatched to CPU */
	__u64 tasks_ai_routed_cpu;		/* AI tasks routed to performance cores */
	__u64 tasks_ai_routed_npu;		/* AI tasks routed to NPU (attempted) */
	__u64 tasks_bg_routed_eff;		/* Background tasks on efficiency cores */
	__u64 avg_enqueue_latency_us;		/* Average enqueue-to-dispatch latency */
	__u64 total_energy_estimate_mj;		/* Energy consumption estimate (millijoules) */
};

#endif /* __AIE_OS_AI_SCHED_H */

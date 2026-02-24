/*
 * AIE-OS Daemon IPC Interface
 *
 * Inter-process communication between kernel eBPF scheduler and userspace daemon.
 * Provides:
 * - Telemetry streaming from kernel via ring buffers
 * - Scheduling decisions pushed back to kernel via BPF maps
 * - Configuration interface for energy policy, time slices
 * - Statistics/monitoring interface
 */

#ifndef __AIE_OS_DAEMON_IPC_H
#define __AIE_OS_DAEMON_IPC_H

#include <cstdint>
#include <string>
#include <memory>

namespace aie {

/* Ring buffer reader for kernel telemetry streaming */
class TelemetryReader {
public:
	TelemetryReader();
	~TelemetryReader();
	
	/* Connect to kernel telemetry ring buffer */
	int connect(const char *bpf_obj_path = "/sys/kernel/btf/aie_scheduler");
	
	/* Poll for next telemetry sample (blocking with timeout_ms) */
	int read_sample(struct ai_task_telemetry *sample, int timeout_ms = 100);
	
	/* Close connection */
	void disconnect();
	
private:
	void *ringbuf_ctx_;
	int ringbuf_fd_;
};

/* Scheduling decision writer to kernel */
class DecisionWriter {
public:
	DecisionWriter();
	~DecisionWriter();
	
	/* Connect to kernel decision map */
	int connect(const char *bpf_obj_path = "/sys/kernel/btf/aie_scheduler");
	
	/* Write scheduling decision for a task (kernel will read and apply) */
	int write_decision(const struct ai_sched_decision *decision);
	
	/* Batch write multiple decisions */
	int write_decisions(const struct ai_sched_decision *decisions, int count);
	
	/* Close connection */
	void disconnect();
	
private:
	int decision_map_fd_;
};

/* Configuration interface */
class SchedulerConfig {
public:
	SchedulerConfig();
	~SchedulerConfig();
	
	/* Connect to kernel configuration maps */
	int connect(const char *bpf_obj_path = "/sys/kernel/btf/aie_scheduler");
	
	/* Set energy policy mode */
	int set_energy_mode(enum ai_energy_mode mode);
	
	/* Get current energy mode */
	enum ai_energy_mode get_energy_mode() const;
	
	/* Set time slice for task class (in microseconds) */
	int set_time_slice(__u32 task_class, __u64 time_slice_us);
	
	/* Get current time slice for class */
	__u64 get_time_slice(__u32 task_class) const;
	
private:
	int energy_mode_map_fd_;
	int time_slices_map_fd_;
};

/* Telemetry and statistics monitoring */
class StatsMonitor {
public:
	StatsMonitor();
	~StatsMonitor();
	
	/* Connect to kernel stats map */
	int connect(const char *bpf_obj_path = "/sys/kernel/btf/aie_scheduler");
	
	/* Get current scheduler statistics */
	int read_stats(struct ai_sched_stats *stats);
	
	/* Pretty-print statistics to stdout */
	void print_stats(const struct ai_sched_stats *stats);
	
private:
	int stats_map_fd_;
};

} // namespace aie

#endif /* __AIE_OS_DAEMON_IPC_H */

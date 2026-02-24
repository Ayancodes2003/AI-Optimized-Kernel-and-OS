/*
 * AIE-OS Daemon IPC Interface Implementation
 *
 * Bridges kernel eBPF scheduler and userspace daemon via BPF maps and ring buffers.
 */

#include "ipc_interface.h"
#include "ai_sched.h"
#include <bpf/libbpf.h>
#include <sys/resource.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>

namespace aie {

/* ======================== TelemetryReader ======================== */

TelemetryReader::TelemetryReader()
	: ringbuf_ctx_(nullptr), ringbuf_fd_(-1)
{
}

TelemetryReader::~TelemetryReader()
{
	disconnect();
}

int TelemetryReader::connect(const char *bpf_obj_path)
{
	/* TODO: Use libbpf to locate and attach to telemetry_ringbuf
	 *
	 * Steps:
	 * 1. Load BPF object file (ai_sched.bpf.o or combined object)
	 * 2. Open telemetry_ringbuf by name
	 * 3. Create ring buffer reader context
	 * 4. Store fd for polling
	 *
	 * For now: stub implementation
	 */
	fprintf(stderr, "TelemetryReader::connect not yet implemented\n");
	return -1;
}

int TelemetryReader::read_sample(struct ai_task_telemetry *sample, int timeout_ms)
{
	if (ringbuf_fd_ < 0) {
		return -1;
	}
	
	/* TODO: ring_buffer__poll or ring_buffer__consume to get next sample
	 * For now: stub
	 */
	
	return -1;
}

void TelemetryReader::disconnect()
{
	if (ringbuf_ctx_) {
		/* TODO: ring_buffer__free(ringbuf_ctx_) */
		ringbuf_ctx_ = nullptr;
	}
	ringbuf_fd_ = -1;
}

/* ======================== DecisionWriter ======================== */

DecisionWriter::DecisionWriter()
	: decision_map_fd_(-1)
{
}

DecisionWriter::~DecisionWriter()
{
	disconnect();
}

int DecisionWriter::connect(const char *bpf_obj_path)
{
	/* TODO: Use libbpf to locate sched_decisions map
	 * For now: stub
	 */
	fprintf(stderr, "DecisionWriter::connect not yet implemented\n");
	return -1;
}

int DecisionWriter::write_decision(const struct ai_sched_decision *decision)
{
	if (decision_map_fd_ < 0 || !decision) {
		return -1;
	}
	
	/* Update BPF map: decisions[pid] = decision */
	int ret = bpf_map_update_elem(decision_map_fd_, 
				       (void *)&decision->pid,
				       (void *)decision, 0);
	
	if (ret < 0) {
		perror("bpf_map_update_elem");
		return -1;
	}
	
	return 0;
}

int DecisionWriter::write_decisions(const struct ai_sched_decision *decisions, int count)
{
	if (!decisions || count <= 0) {
		return -1;
	}
	
	int succeeded = 0;
	for (int i = 0; i < count; ++i) {
		if (write_decision(&decisions[i]) == 0) {
			succeeded++;
		}
	}
	
	return (succeeded == count) ? 0 : -1;
}

void DecisionWriter::disconnect()
{
	if (decision_map_fd_ >= 0) {
		close(decision_map_fd_);
		decision_map_fd_ = -1;
	}
}

/* ======================== SchedulerConfig ======================== */

SchedulerConfig::SchedulerConfig()
	: energy_mode_map_fd_(-1), time_slices_map_fd_(-1)
{
}

SchedulerConfig::~SchedulerConfig()
{
}

int SchedulerConfig::connect(const char *bpf_obj_path)
{
	/* TODO: Locate energy_mode and time_slices maps via libbpf
	 * For now: stub
	 */
	fprintf(stderr, "SchedulerConfig::connect not yet implemented\n");
	return -1;
}

int SchedulerConfig::set_energy_mode(enum ai_energy_mode mode)
{
	if (energy_mode_map_fd_ < 0) {
		return -1;
	}
	
	__u32 key = 0;
	__u32 mode_val = (__u32)mode;
	
	int ret = bpf_map_update_elem(energy_mode_map_fd_, &key, &mode_val, 0);
	if (ret < 0) {
		perror("set_energy_mode");
		return -1;
	}
	
	return 0;
}

enum ai_energy_mode SchedulerConfig::get_energy_mode() const
{
	/* TODO: Implement */
	return AI_ENERGY_BALANCED;
}

int SchedulerConfig::set_time_slice(__u32 task_class, __u64 time_slice_us)
{
	if (time_slices_map_fd_ < 0) {
		return -1;
	}
	
	__u32 key = task_class % 5;
	__u64 slice_ns = time_slice_us * 1000ULL;
	
	int ret = bpf_map_update_elem(time_slices_map_fd_, &key, &slice_ns, 0);
	if (ret < 0) {
		perror("set_time_slice");
		return -1;
	}
	
	return 0;
}

__u64 SchedulerConfig::get_time_slice(__u32 task_class) const
{
	/* TODO: Implement */
	return 5000000ULL;		/* Default 5ms */
}

/* ======================== StatsMonitor ======================== */

StatsMonitor::StatsMonitor()
	: stats_map_fd_(-1)
{
}

StatsMonitor::~StatsMonitor()
{
}

int StatsMonitor::connect(const char *bpf_obj_path)
{
	/* TODO: Locate sched_stats map via libbpf
	 * For now: stub
	 */
	fprintf(stderr, "StatsMonitor::connect not yet implemented\n");
	return -1;
}

int StatsMonitor::read_stats(struct ai_sched_stats *stats)
{
	if (stats_map_fd_ < 0 || !stats) {
		return -1;
	}
	
	__u32 key = 0;
	struct ai_sched_stats *map_stats = (struct ai_sched_stats *)
		bpf_map_lookup_elem(stats_map_fd_, &key);
	
	if (!map_stats) {
		return -1;
	}
	
	*stats = *map_stats;
	return 0;
}

void StatsMonitor::print_stats(const struct ai_sched_stats *stats)
{
	if (!stats) {
		return;
	}
	
	printf("=== AIE-OS Scheduler Statistics ===\n");
	printf("  Tasks Enqueued:           %llu\n", stats->tasks_enqueued);
	printf("  Tasks Dispatched:         %llu\n", stats->tasks_dispatched);
	printf("  AI Tasks -> CPU:          %llu\n", stats->tasks_ai_routed_cpu);
	printf("  AI Tasks -> NPU:          %llu\n", stats->tasks_ai_routed_npu);
	printf("  Background -> Eff Cores:  %llu\n", stats->tasks_bg_routed_eff);
	printf("  Avg Enqueue Latency:      %llu us\n", stats->avg_enqueue_latency_us);
	printf("  Est. Energy:              %llu mJ\n", stats->total_energy_estimate_mj);
	printf("====================================\n");
}

} // namespace aie

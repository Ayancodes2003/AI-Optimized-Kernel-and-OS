/*
 * AIE-OS Daemon IPC Interface Implementation
 *
 * Bridges kernel eBPF scheduler and userspace daemon via BPF maps and ring buffers.
 */

#include "ipc_interface.h"
#include "ai_sched.h"
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <bpf/bpf.h>
#include <sys/resource.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <thread>
#include <chrono>
namespace aie {

/* ======================== TelemetryReader ======================== */


TelemetryReader::TelemetryReader()
{
    ringbuf_ctx_ = NULL;
    ringbuf_fd_ = -1;
    obj_ = NULL;
}

TelemetryReader::~TelemetryReader()
{
    disconnect();
}

int TelemetryReader::connect(const char *bpf_obj_path)
{
    const char *path = "/usr/local/lib/aie-os/telemetry.bpf.o";

    printf("Opening telemetry BPF object: %s\n", path);

    obj_ = bpf_object__open_file(path, NULL);
    if (!obj_) {
        perror("bpf_object__open_file");
        return -1;
    }

    if (bpf_object__load(obj_)) {
        perror("bpf_object__load");
        return -1;
    }

    struct bpf_map *rb_map =
        bpf_object__find_map_by_name(obj_, "telemetry_ringbuf");

    if (!rb_map) {
        fprintf(stderr, "ERROR: telemetry_ringbuf map not found\n");
        return -1;
    }

    ringbuf_fd_ = bpf_map__fd(rb_map);

    ringbuf_ctx_ = ring_buffer__new(
        ringbuf_fd_,
        [](void *ctx, void *data, size_t len) -> int {
            /* Real telemetry callback */
            struct ai_task_telemetry *sample =
                (struct ai_task_telemetry *)data;

            printf("Telemetry event: PID=%u CPU=%u MEM=%u\n",
                   sample->pid,
                   sample->cpu_util_recent,
                   sample->memory_rss_mb);

            return 0;
        },
        NULL,
        NULL);

    if (!ringbuf_ctx_) {
        fprintf(stderr, "ERROR: ring_buffer__new failed\n");
        return -1;
    }

    printf("TelemetryReader connected successfully\n");

    return 0;
}

int TelemetryReader::read_sample(struct ai_task_telemetry *sample, int timeout_ms)
{
    if (!ringbuf_ctx_)
        return -1;

    return ring_buffer__poll(ringbuf_ctx_, timeout_ms);
}

void TelemetryReader::disconnect()
{
    if (ringbuf_ctx_) {
        ring_buffer__free(ringbuf_ctx_);
        ringbuf_ctx_ = NULL;
    }

    if (obj_) {
        bpf_object__close(obj_);
        obj_ = NULL;
    }

    ringbuf_fd_ = -1;
}




/* ======================== DecisionWriter ======================== */

DecisionWriter::DecisionWriter()
    : decision_map_fd_(-1)
{
    struct bpf_object *obj;

    obj = bpf_object__open_file("/usr/local/lib/aie-os/ai_sched.bpf.o", NULL);
    if (!obj) {
        perror("DecisionWriter: open failed");
        return;
    }

    if (bpf_object__load(obj)) {
        perror("DecisionWriter: load failed");
        return;
    }

    struct bpf_map *map =
        bpf_object__find_map_by_name(obj, "sched_decisions");

    if (!map) {
        printf("DecisionWriter: sched_decisions map not found\n");
        return;
    }

    decision_map_fd_ = bpf_map__fd(map);

    printf("DecisionWriter: REAL kernel mode enabled\n");
}

DecisionWriter::~DecisionWriter()
{
	disconnect();
}

int DecisionWriter::connect(const char *bpf_obj_path)
{
    (void)bpf_obj_path;

    fprintf(stderr, "DecisionWriter: Prototype mode enabled\n");

    decision_map_fd_ = 1;  // fake valid map fd

    return 0;
}

int DecisionWriter::write_decision(const struct ai_sched_decision *decision)
{
    (void)decision;

    // Prototype mode: skip kernel update
    return 0;
}

int DecisionWriter::write_decisions(const struct ai_sched_decision *decisions, int count)
{
    (void)decisions;
    (void)count;

    // Prototype mode: skip kernel updates
    return 0;
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
    (void)bpf_obj_path;

    fprintf(stderr, "SchedulerConfig: Prototype mode enabled\n");

    return 0;
}

int SchedulerConfig::set_energy_mode(enum ai_energy_mode mode)
{
	if (energy_mode_map_fd_ < 0) {
		return -1;
	}
	
	__u32 key = 0;
	__u32 mode_val = (__u32)mode;
	

	
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
    (void)bpf_obj_path;

    fprintf(stderr, "StatsMonitor: Prototype mode enabled\n");

    return 0;
}
int StatsMonitor::read_stats(struct ai_sched_stats *stats)
{
	if (stats_map_fd_ < 0 || !stats) {
		return -1;
	}
	
	__u32 key = 0;
	struct ai_sched_stats tmp_stats;
	int lookup_ret = bpf_map_lookup_elem(stats_map_fd_, &key, &tmp_stats);
	struct ai_sched_stats *map_stats = (lookup_ret == 0) ? &tmp_stats : nullptr;
	
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

#include <ctime>
/*
 * AIE-OS Task Classifier Implementation
 *
 * Heuristic-based classification with extensible interface for ML models.
 * Classifies tasks into:
 * - REALTIME_AI: strict latency AI inference (e.g., voice input)
 * - INTERACTIVE_AI: responsive AI (e.g., chatbot response)
 * - BATCH_AI: training or background inference
 * - BACKGROUND: generic/non-AI tasks
 *
 * Heuristics based on:
 * - Syscall patterns (interactive = high syscall density)
 * - Memory footprint (large memory = batch processing)
 * - CPU utilization (bursty = interactive, sustained = batch)
 * - Thread/process patterns
 */

#include "classifier.h"
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace aie {

Classifier::Classifier()
{
}

Classifier::~Classifier()
{
}

int Classifier::init()
{
	/* TODO: Load ML model if available (ONNX runtime, etc)
	 * For now: use pure heuristics
	 */
	return 0;
}

int Classifier::classify(const struct ai_task_telemetry *telemetry,
			 struct ai_sched_decision *decision)
{
	if (!telemetry || !decision) {
		return -1;
	}
	
	std::memset(decision, 0, sizeof(*decision));
	decision->pid = telemetry->pid;
	struct timespec _ts; clock_gettime(CLOCK_MONOTONIC, &_ts); decision->ts_decision = (__u64)_ts.tv_sec * 1000000000ULL + _ts.tv_nsec;
	
	/* Stage 1: Heuristic classification */
	decision->task_class = classify_heuristic(telemetry);
	
	/* Stage 2: Select compute device */
	decision->preferred_device = select_device(telemetry, decision->task_class);
	
	/* Stage 3: Estimate time slice */
	decision->time_slice_us = estimate_time_slice(telemetry, decision->task_class) / 1000ULL;
	
	/* Set reasonable defaults for other fields */
	decision->cpu_affinity_mask = 0;		/* 0 = no affinity preference */
	decision->priority_boost = 0;		/* No priority adjustment by default */
	
	return 0;
}

int Classifier::update_model(const struct ai_task_telemetry *telemetry,
			     __u32 true_class)
{
	/* TODO: Model fine-tuning with ground-truth labels
	 * For now: no-op
	 */
	(void)telemetry;
	(void)true_class;
	return 0;
}

__u32 Classifier::classify_heuristic(const struct ai_task_telemetry *telemetry)
{
    /* Safety check */
    if (!telemetry)
        return AI_TASK_CLASS_BACKGROUND;

    /* Pattern 1: System / kernel tasks → BACKGROUND */
    if (telemetry->sched_class == 5 || telemetry->sched_class == 0)
        return AI_TASK_CLASS_BACKGROUND;

    /* Pattern 2: REALTIME_AI
     * High priority (nice <= -10) AND high CPU usage
     * Example: voice assistant, real-time inference
     */
    if (telemetry->nice_value <= -10 && telemetry->cpu_util_recent >= 40)
        return AI_TASK_CLASS_REALTIME_AI;

    /* Pattern 3: BATCH_AI (GPU/NPU candidate)
     * Large memory usage OR multi-threaded workload
     * Example: model training, batch inference
     */
    if (telemetry->memory_rss_mb >= 100 || telemetry->num_threads >= 2)
        return AI_TASK_CLASS_BATCH_AI;

    /* Pattern 4: INTERACTIVE_AI
     * Moderate CPU usage
     * Example: chatbot, incremental inference
     */
    if (telemetry->cpu_util_recent >= 20)
        return AI_TASK_CLASS_INTERACTIVE_AI;

    /* Pattern 5: fallback → BACKGROUND */
    return AI_TASK_CLASS_BACKGROUND;
}

__u32 Classifier::select_device(const struct ai_task_telemetry *telemetry,
				 __u32 task_class)
{
	(void)telemetry;	/* May be used for device affinity heuristics later */
	
	/* Device selection policy (can be overridden by energy policy) */
	switch (task_class) {
	case AI_TASK_CLASS_REALTIME_AI:
		/* Realtime AI always prefers perf cores for lowest latency */
		return AI_DEVICE_PERF_CORE;
	
	case AI_TASK_CLASS_INTERACTIVE_AI:
		/* Interactive prefers performance, but could use NPU for inference
		 * For now: defer to scheduler's dynamic choice */
		return AI_DEVICE_AUTO;
	
	case AI_TASK_CLASS_BATCH_AI:
		/* Batch processing is flexible
		 * Prefer NPU for pure inference, perf cores for training
		 * For now: auto (scheduler decides based on energy mode)
		 */
		return AI_DEVICE_AUTO;
	
	case AI_TASK_CLASS_BACKGROUND:
		/* Background tasks can use efficiency cores */
		return AI_DEVICE_EFF_CORE;
	
	default:
		return AI_DEVICE_AUTO;
	}
}

__u64 Classifier::estimate_time_slice(const struct ai_task_telemetry *telemetry,
				       __u32 task_class)
{
	(void)telemetry;
	
	/* Time slice estimates (nanoseconds) based on task class
	 * These are starting points; daemon/kernel can override
	 */
	switch (task_class) {
	case AI_TASK_CLASS_REALTIME_AI:
		return 1000000ULL;		/* 1ms - minimize latency jitter */
	
	case AI_TASK_CLASS_INTERACTIVE_AI:
		return 2000000ULL;		/* 2ms - responsive feel */
	
	case AI_TASK_CLASS_BATCH_AI:
		return 20000000ULL;		/* 20ms - throughput optimized */
	
	case AI_TASK_CLASS_BACKGROUND:
		return 50000000ULL;		/* 50ms - energy efficient */
	
	default:
		return 5000000ULL;		/* 5ms - safe default */
	}
}

} // namespace aie

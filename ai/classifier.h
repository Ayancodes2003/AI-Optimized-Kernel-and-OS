/*
 * AIE-OS Task Classifier
 *
 * Classifies AI and non-AI workloads based on runtime telemetry.
 *
 * Classification stages (in order):
 * 1. Heuristic fast-path (pattern matching on syscall, memory, CPU behavior)
 * 2. ML model inference (ONNX runtime, once available)
 * 3. Fallback to most likely class
 *
 * This module provides a pluggable interface so that different backends
 * (ONNX, TensorFlow Lite, or custom logic) can be swapped.
 */

#ifndef __AIE_OS_CLASSIFIER_H
#define __AIE_OS_CLASSIFIER_H

#include "ai_sched.h"

namespace aie {

/* Classify a task based on telemetry and return scheduling decision */
class Classifier {
public:
	Classifier();
	virtual ~Classifier();
	
	/* Initialize classifier (load models, etc) */
	virtual int init();
	
	/* Classify a task and return scheduling decision
	 * Input: telemetry snapshot
	 * Output: decision (class + device preference)
	 * Returns: 0 on success, <0 on error
	 */
	virtual int classify(const struct ai_task_telemetry *telemetry,
			     struct ai_sched_decision *decision);
	
	/* (Optional) Train/update classifier with feedback feedback
	 * Used when ground-truth labels are available
	 */
	virtual int update_model(const struct ai_task_telemetry *telemetry,
				 __u32 true_class);
	
protected:
	/* Helper: heuristic classification (no ML required) */
	__u32 classify_heuristic(const struct ai_task_telemetry *telemetry);
	
	/* Helper: select compute device based on class and profile */
	__u32 select_device(const struct ai_task_telemetry *telemetry,
			    __u32 task_class);
	
	/* Helper: estimate time slice for class */
	__u64 estimate_time_slice(const struct ai_task_telemetry *telemetry,
				  __u32 task_class);
};

} // namespace aie

#endif /* __AIE_OS_CLASSIFIER_H */

#include <ctime>
#include "onnx_classifier.h"
#include <onnxruntime_cxx_api.h>
#include <fstream>
#include <iostream>
#include <algorithm>

namespace aie {

OnnxClassifier::OnnxClassifier(const std::string &model_path)
    : model_path_(model_path), loaded_(false), load_failed_(false),
      env_(nullptr), session_(nullptr), allocator_(nullptr)
{
    // nothing else
}

OnnxClassifier::~OnnxClassifier()
{
    if (allocator_) {
        delete allocator_;
        allocator_ = nullptr;
    }
    if (env_) {
        delete env_;
        env_ = nullptr;
    }
}

int OnnxClassifier::init()
{
    /* Do not eagerly load model; lazy load on first classify call */
    if (model_path_.empty()) {
        return 0;
    }
    return 0;
}

bool OnnxClassifier::load_model()
{
    if (loaded_ || load_failed_)
        return loaded_;

    std::ifstream f(model_path_);
    if (!f.good()) {
        std::cerr << "[OnnxClassifier] model not found at " << model_path_
                  << "; falling back to heuristic" << std::endl;
        load_failed_ = true;
        return false;
    }

    try {
        /* create environment and allocator */
        env_ = new Ort::Env(ORT_LOGGING_LEVEL_WARNING, "aie");
        allocator_ = new Ort::AllocatorWithDefaultOptions();

        Ort::SessionOptions opts;
        opts.SetIntraOpNumThreads(1);
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        session_ = std::make_unique<Ort::Session>(*env_, model_path_.c_str(), opts);

        /* cache input/output names */
        size_t num_inputs = session_->GetInputCount();
        for (size_t i = 0; i < num_inputs; i++) {
            auto name_a = session_->GetInputNameAllocated(i, *allocator_); input_names_.emplace_back(name_a.get());
        }
        size_t num_outputs = session_->GetOutputCount();
        for (size_t i = 0; i < num_outputs; i++) {
            auto name_b = session_->GetOutputNameAllocated(i, *allocator_); output_names_.emplace_back(name_b.get());
        }

        /* remember input shape (assume first input) */
        if (num_inputs > 0) {
            auto info = session_->GetInputTypeInfo(0);
            auto tensor_info = info.GetTensorTypeAndShapeInfo();
            input_shape_ = tensor_info.GetShape();
        }

        loaded_ = true;
        std::cerr << "[OnnxClassifier] Loaded model " << model_path_ << std::endl;
    } catch (const Ort::Exception &e) {
        std::cerr << "[OnnxClassifier] ONNX runtime error: " << e.what()
                  << "; falling back to heuristic" << std::endl;
        load_failed_ = true;
        loaded_ = false;
    }

    return loaded_;
}

int OnnxClassifier::classify(const struct ai_task_telemetry *telemetry,
                             struct ai_sched_decision *decision)
{
    if (!telemetry || !decision)
        return -1;

    /* attempt to load model if not done yet */
    if (!loaded_ && !load_failed_) {
        load_model();
    }

    if (!loaded_) {
        /* fallback to heuristic provided by base class */
        return Classifier::classify(telemetry, decision);
    }

    /* prepare input features - model expects 6 floats
     * mapping from telemetry to feature vector:
     *   cpu_time           -> cpu_util_recent (approx)
     *   mem_bytes          -> memory_rss_mb * 1MB
     *   io_rate            -> io_read+io_write
     *   syscall_rate       -> syscall_count
     *   runtime_ms         -> enqueue age in ms
     *   context_switch_rate-> num_threads (proxy)
     */
    float features[6];
    features[0] = (float)telemetry->cpu_util_recent;
    features[1] = (float)telemetry->memory_rss_mb * 1024.0f * 1024.0f;
    features[2] = (float)(telemetry->io_read_bytes + telemetry->io_write_bytes);
    features[3] = (float)telemetry->syscall_count;
    /* compute age in ms using current kernel time */
    struct timespec _ts; clock_gettime(CLOCK_MONOTONIC, &_ts); __u64 now = (__u64)_ts.tv_sec * 1000000000ULL + _ts.tv_nsec;
    features[4] = (float)((now - telemetry->ts_enqueue) / 1000000ULL);
    features[5] = (float)telemetry->num_threads;

    /* create input tensor */
    std::vector<int64_t> shape{1, 6};
    Ort::MemoryInfo mem_info =
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);
    Ort::Value input_tensor =
        Ort::Value::CreateTensor<float>(mem_info, features, 6, shape.data(), shape.size());

    std::vector<Ort::Value> inputs;
    inputs.emplace_back(std::move(input_tensor));

    const char *in_names[] = { input_names_[0].c_str() };
    const char *out_names[] = { output_names_[0].c_str() };

    try {
        auto output_tensors = session_->Run(Ort::RunOptions{nullptr},
                                            in_names, inputs.data(), 1,
                                            out_names, 1);
        float *prob = output_tensors[0].GetTensorMutableData<float>();
        /* choose argmax */
        int max_idx = std::distance(prob, std::max_element(prob, prob + 5));

        decision->pid = telemetry->pid;
        struct timespec _ts3; clock_gettime(CLOCK_MONOTONIC, &_ts3); decision->ts_decision = (__u64)_ts3.tv_sec * 1000000000ULL + _ts3.tv_nsec;
        decision->task_class = (__u32)max_idx;
        decision->preferred_device = select_device(telemetry, decision->task_class);
        decision->time_slice_us = estimate_time_slice(telemetry, decision->task_class) / 1000ULL;
        decision->cpu_affinity_mask = 0;
        decision->priority_boost = 0;

        return 0;
    } catch (const Ort::Exception &e) {
        std::cerr << "[OnnxClassifier] inference error: " << e.what()
                  << "; falling back to heuristic" << std::endl;
        return Classifier::classify(telemetry, decision);
    }
}

int OnnxClassifier::update_model(const struct ai_task_telemetry *telemetry,
                                 __u32 true_class)
{
    (void)telemetry;
    (void)true_class;
    /* not implemented yet */
    return 0;
}

} // namespace aie

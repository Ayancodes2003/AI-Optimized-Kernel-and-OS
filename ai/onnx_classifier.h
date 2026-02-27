#ifndef __AIE_OS_ONNX_CLASSIFIER_H
#define __AIE_OS_ONNX_CLASSIFIER_H

#include "classifier.h"
#include <string>
#include <memory>

/* Forward declare ONNX Runtime classes (included in cpp) */
namespace Ort {
    class Env;
    class Session;
    class AllocatorWithDefaultOptions;
}

namespace aie {

class OnnxClassifier : public Classifier {
public:
    explicit OnnxClassifier(const std::string &model_path = "");
    virtual ~OnnxClassifier();

    int init() override;
    int classify(const struct ai_task_telemetry *telemetry,
                 struct ai_sched_decision *decision) override;
    int update_model(const struct ai_task_telemetry *telemetry,
                     __u32 true_class) override;

private:
    bool load_model();

    std::string model_path_;
    bool loaded_;
    bool load_failed_;

    /* ONNX Runtime objects (created lazily) */
    Ort::Env *env_;
    std::unique_ptr<Ort::Session> session_;
    Ort::AllocatorWithDefaultOptions *allocator_;
    std::vector<std::string> input_names_;
    std::vector<std::string> output_names_;
    std::vector<int64_t> input_shape_;
};

} // namespace aie

#endif /* __AIE_OS_ONNX_CLASSIFIER_H */

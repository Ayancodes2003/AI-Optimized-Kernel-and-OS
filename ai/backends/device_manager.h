#ifndef __AIE_OS_DEVICE_MANAGER_H
#define __AIE_OS_DEVICE_MANAGER_H

#include <memory>
#include <vector>
#include <string>
#include "ai_sched.h"

namespace aie {

class DeviceBackend {
public:
    virtual ~DeviceBackend() {}
    virtual void dispatch(const struct ai_sched_decision &decision) = 0;
    virtual const char *name() const = 0;
};

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    /* probe available devices and instantiate backends */
    int init();

    /* select backend given daemon-preferred device code */
    DeviceBackend *select_backend(__u32 preferred_device);

    bool has_gpu() const { return gpu_available_; }
    bool has_npu() const { return npu_available_; }
    const char *backend_name(__u32 preferred_device);

private:
    std::unique_ptr<DeviceBackend> cpu_backend_;
    std::unique_ptr<DeviceBackend> gpu_backend_;
    std::unique_ptr<DeviceBackend> npu_backend_;

    bool gpu_available_;
    bool npu_available_;

    bool detect_gpu();
    bool detect_npu();
};

} // namespace aie

#endif /* __AIE_OS_DEVICE_MANAGER_H */

#include "device_manager.h"
#include "cpu_backend.h"
#include "gpu_backend.h"
#include "npu_backend.h"
#include <unistd.h>
#include <glob.h>
#include <syslog.h>
#include <iostream>

namespace aie {

DeviceManager::DeviceManager()
    : gpu_available_(false), npu_available_(false)
{
}

DeviceManager::~DeviceManager() = default;

bool DeviceManager::detect_gpu()
{
    /* Check for common GPU device nodes */
    if (access("/dev/dri", F_OK) == 0)
        return true;
    if (access("/dev/nvidia0", F_OK) == 0)
        return true;
    return false;
}

bool DeviceManager::detect_npu()
{
    if (access("/dev/amdxdna", F_OK) == 0)
        return true;

    glob_t results;
    int ret = glob("/dev/npu*", 0, nullptr, &results);
    bool found = (ret == 0 && results.gl_pathc > 0);
    globfree(&results);
    return found;
}

int DeviceManager::init()
{
    gpu_available_ = detect_gpu();
    npu_available_ = detect_npu();

    /* always have a CPU backend */
    cpu_backend_ = std::make_unique<CpuBackend>();

    if (gpu_available_) {
        syslog(LOG_INFO, "[DeviceManager] GPU detected, GPU backend enabled");
        gpu_backend_ = std::make_unique<GpuBackend>();
    } else {
        syslog(LOG_INFO, "[DeviceManager] GPU not found");
    }

    if (npu_available_) {
        syslog(LOG_INFO, "[DeviceManager] NPU detected, NPU backend enabled");
        npu_backend_ = std::make_unique<NpuBackend>();
    } else {
        syslog(LOG_INFO, "[DeviceManager] NPU not found");
    }

    return 0;
}

DeviceBackend *DeviceManager::select_backend(__u32 preferred_device)
{
    switch (preferred_device) {
    case AI_DEVICE_NPU:
        if (npu_available_ && npu_backend_)
            return npu_backend_.get();
        /* fallthrough to GPU */
    case AI_DEVICE_GPU:
        if (gpu_available_ && gpu_backend_)
            return gpu_backend_.get();
        /* fallthrough to CPU */
    default:
        return cpu_backend_.get();
    }
}

const char *DeviceManager::backend_name(__u32 preferred_device)
{
    DeviceBackend *b = select_backend(preferred_device);
    return b ? b->name() : "unknown";
}

} // namespace aie

#ifndef __AIE_OS_GPU_BACKEND_H
#define __AIE_OS_GPU_BACKEND_H

#include "device_manager.h"

namespace aie {

class GpuBackend : public DeviceBackend {
public:
    void dispatch(const struct ai_sched_decision &decision) override;
    const char *name() const override;
};

} // namespace aie

#endif /* __AIE_OS_GPU_BACKEND_H */

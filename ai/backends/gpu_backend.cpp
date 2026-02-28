#include "gpu_backend.h"
#include <syslog.h>

namespace aie {

void GpuBackend::dispatch(const struct ai_sched_decision &decision)
{
    syslog(LOG_DEBUG, "[GpuBackend] (stub) routing pid=%u to GPU", decision.pid);
}

const char *GpuBackend::name() const
{
    return "GPU";
}

} // namespace aie

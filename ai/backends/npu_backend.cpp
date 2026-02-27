#include "npu_backend.h"
#include <syslog.h>

namespace aie {

void NpuBackend::dispatch(const struct ai_sched_decision &decision)
{
    syslog(LOG_DEBUG, "[NpuBackend] (stub) routing pid=%u to NPU", decision.pid);
}

const char *NpuBackend::name() const
{
    return "NPU";
}

} // namespace aie

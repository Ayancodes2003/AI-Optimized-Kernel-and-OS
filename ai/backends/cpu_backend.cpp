#include "cpu_backend.h"
#include <syslog.h>

namespace aie {

void CpuBackend::dispatch(const struct ai_sched_decision &decision)
{
    syslog(LOG_DEBUG, "[CpuBackend] dispatch pid=%u to CPU cores", decision.pid);
}

const char *CpuBackend::name() const
{
    return "CPU";
}

} // namespace aie

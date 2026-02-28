#include <iostream>
#include <iomanip>
#include <string>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <linux/bpf.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include "ai_sched.h"

#define SCHED_STATS_PIN     "/sys/fs/bpf/aie_sched_stats"
#define ENERGY_MODE_PIN     "/sys/fs/bpf/aie_energy_mode"

static volatile bool running = true;
static void signal_handler(int) { running = false; }

static void clear_screen() { std::cout << "\033[2J\033[H"; std::cout.flush(); }

static const char* energy_mode_str(uint32_t mode) {
    switch (mode) {
        case 0: return "PERFORMANCE";
        case 1: return "BALANCED";
        case 2: return "EFFICIENT";
        case 3: return "POWER_SAVER";
        default: return "UNKNOWN";
    }
}

static void display_header() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n"
              << "║        AIE-OS Scheduler Monitor (aie_top)                ║\n"
              << "║  AI-Native Kernel Scheduler Real-Time Dashboard           ║\n"
              << "╚════════════════════════════════════════════════════════════╝\n\n";
}

static void display_stats(const struct ai_sched_stats *s, bool live) {
    std::cout << "╔════ SCHEDULER STATISTICS " << (live ? "[LIVE]" : "[OFFLINE]") << " ══════════════════════════╗\n";
    std::cout << "║ Total Tasks Enqueued:      " << std::setw(25) << s->tasks_enqueued      << " │\n";
    std::cout << "║ Total Tasks Dispatched:    " << std::setw(25) << s->tasks_dispatched    << " │\n";
    std::cout << "║ AI Tasks → Perf Cores:     " << std::setw(25) << s->tasks_ai_routed_cpu << " │\n";
    std::cout << "║ AI Tasks → NPU:            " << std::setw(25) << s->tasks_ai_routed_npu << " │\n";
    std::cout << "║ Background → Eff Cores:    " << std::setw(25) << s->tasks_bg_routed_eff << " │\n";
    std::cout << "║ Avg Enqueue Latency:       " << std::setw(20) << s->avg_enqueue_latency_us << " µs │\n";
    std::cout << "║ Estimated Energy (~):      " << std::setw(20) << s->total_energy_estimate_mj << " mJ │\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
}

static void display_energy(uint32_t mode) {
    std::cout << "╔════ POWER MANAGEMENT ═════════════════════════════════════╗\n";
    std::cout << "║ Energy Mode:   " << std::setw(20) << energy_mode_str(mode) << "                        │\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
}

static void display_task_classes() {
    std::cout << "╔════ TASK CLASSIFICATION ══════════════════════════════════╗\n";
    std::cout << "║ REALTIME_AI    → Perf Core  (voice, real-time AI)        │\n";
    std::cout << "║ INTERACTIVE_AI → Perf Core  (chatbot, incremental)       │\n";
    std::cout << "║ BATCH_AI       → NPU/GPU    (training, batch infer)      │\n";
    std::cout << "║ BACKGROUND     → Eff Core   (daemons, logging)           │\n";
    std::cout << "║ UNKNOWN        → Fallback   (unclassified)               │\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n\n";
}

int main(int argc, char **argv) {
    int refresh = 2;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-n" && i+1 < argc)
            refresh = std::atoi(argv[++i]);
        else if (std::string(argv[i]) == "-h") {
            std::cout << "Usage: aie_top [-n SECONDS]\n";
            return 0;
        }
    }
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    int stats_fd = bpf_obj_get(SCHED_STATS_PIN);
    int energy_fd = bpf_obj_get(ENERGY_MODE_PIN);

    if (stats_fd < 0)
        std::cerr << "[WARN] Stats map not pinned yet. Run: sudo ./pin_maps.sh\n\n";

    while (running) {
        clear_screen();
        display_header();
        struct ai_sched_stats stats = {};
        bool live = false;
        if (stats_fd >= 0) {
            uint32_t key = 0;
            if (bpf_map_lookup_elem(stats_fd, &key, &stats) == 0)
                live = true;
        }
        uint32_t energy_mode = 1;
        if (energy_fd >= 0) {
            uint32_t key = 0;
            bpf_map_lookup_elem(energy_fd, &key, &energy_mode);
        }
        display_stats(&stats, live);
        display_energy(energy_mode);
        display_task_classes();
        std::cout << "[Refreshing every " << refresh << "s. Ctrl+C to exit]\n";
        sleep(refresh);
    }
    if (stats_fd >= 0) close(stats_fd);
    if (energy_fd >= 0) close(energy_fd);
    return 0;
}

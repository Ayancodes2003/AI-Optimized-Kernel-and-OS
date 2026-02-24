/*
 * AIE-OS Scheduler Monitor Tool (aie_top)
 *
 * Real-time CLI dashboard showing:
 * - Scheduler statistics (tasks routed per device, energy estimates)
 * - Task classification breakdown
 * - Energy mode and policy configuration
 * - Top tasks by CPU/memory/AI classification
 *
 * Similar to 'top' or 'htop' but scheduler-focused.
 * Updates every 2-3 seconds.
 */

#include <iostream>
#include <iomanip>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>

#include "ai_sched.h"

namespace aie {

/* Simple monitoring client */
class SchedulerMonitor {
public:
	SchedulerMonitor();
	~SchedulerMonitor();
	
	int init();
	int run(int refresh_interval_sec = 2);
	void shutdown();
	
private:
	void display_header();
	void display_stats(const struct ai_sched_stats *stats);
	void display_energy_mode();
	void display_task_classes();
	void clear_screen();
	
	bool running_;
};

SchedulerMonitor::SchedulerMonitor()
	: running_(false)
{
}

SchedulerMonitor::~SchedulerMonitor()
{
}

int SchedulerMonitor::init()
{
	/* TODO: Connect to stats monitor interface
	 * For now: stub
	 */
	running_ = true;
	return 0;
}

void SchedulerMonitor::shutdown()
{
	running_ = false;
}

int SchedulerMonitor::run(int refresh_interval_sec)
{
	struct ai_sched_stats stats;
	std::memset(&stats, 0, sizeof(stats));
	
	while (running_) {
		clear_screen();
		display_header();
		
		/* TODO: Read actual stats from kernel
		 * For now: display placeholder
		 */
		display_stats(&stats);
		display_energy_mode();
		display_task_classes();
		
		/* Print refresh hint */
		std::cout << "\n[Refreshing every " << refresh_interval_sec
			  << " seconds. Press Ctrl+C to exit]\n";
		
		sleep(refresh_interval_sec);
	}
	
	return 0;
}

void SchedulerMonitor::clear_screen()
{
	/* ANSI clear screen */
	std::cout << "\033[2J\033[H";
	std::cout.flush();
}

void SchedulerMonitor::display_header()
{
	std::cout << "╔════════════════════════════════════════════════════════════╗\n"
		  << "║        AIE-OS Scheduler Monitor (aie_top)                ║\n"
		  << "║  AI-Native Kernel Scheduler Real-Time Dashboard           ║\n"
		  << "╚════════════════════════════════════════════════════════════╝\n";
	std::cout << "\n";
}

void SchedulerMonitor::display_stats(const struct ai_sched_stats *stats)
{
	std::cout << "╔════ SCHEDULER STATISTICS ════════════════════════════════╗\n";
	
	std::cout << std::fixed << std::setprecision(2);
	
	std::cout << "║ Total Tasks Enqueued:           " 
		  << std::setw(20) << stats->tasks_enqueued << " │\n";
	
	std::cout << "║ Total Tasks Dispatched:         " 
		  << std::setw(20) << stats->tasks_dispatched << " │\n";
	
	
	std::cout << "║ AI Tasks → Perf Cores:          " 
		  << std::setw(20) << stats->tasks_ai_routed_cpu << " │\n";
	
	std::cout << "║ AI Tasks → NPU:                 " 
		  << std::setw(20) << stats->tasks_ai_routed_npu << " │\n";
	
	std::cout << "║ Background → Eff Cores:        " 
		  << std::setw(20) << stats->tasks_bg_routed_eff << " │\n";
	
	std::cout << "║ Avg Enqueue Latency:            " 
		  << std::setw(15) << stats->avg_enqueue_latency_us << " µs │\n";
	
	std::cout << "║ Estimated Energy (~):           " 
		  << std::setw(15) << stats->total_energy_estimate_mj << " mJ │\n";
	
	std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
	std::cout << "\n";
}

void SchedulerMonitor::display_energy_mode()
{
	std::cout << "╔════ POWER MANAGEMENT ═════════════════════════════════════╗\n";
	std::cout << "║ Energy Mode:        BALANCED (override with aie_config)   │\n";
	std::cout << "║ Options:            PERFORMANCE, BALANCED, EFFICIENT      │\n";
	std::cout << "║                     POWER_SAVER                           │\n";
	std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
	std::cout << "\n";
}

void SchedulerMonitor::display_task_classes()
{
	std::cout << "╔════ TASK CLASSIFICATION REFERENCE ════════════════════════╗\n";
	std::cout << "║ 🔴 REALTIME_AI       - Ultra-low latency inference        │\n";
	std::cout << "║                         (voice input, time-critical)      │\n";
	std::cout << "║                         → Route: Perf Core (P-cluster)    │\n";
	std::cout << "║                                                            │\n";
	std::cout << "║ 🟠 INTERACTIVE_AI     - Responsive inference/processing   │\n";
	std::cout << "║                         (chatbot, incremental inference)  │\n";
	std::cout << "║                         → Route: Perf Core or Auto        │\n";
	std::cout << "║                                                            │\n";
	std::cout << "║ 🟡 BATCH_AI          - Model training, background infer   │\n";
	std::cout << "║                         (training, batch processing)      │\n";
	std::cout << "║                         → Route: Auto (NPU/GPU/Perf)     │\n";
	std::cout << "║                                                            │\n";
	std::cout << "║ 🟢 BACKGROUND        - Non-AI system / daemon tasks      │\n";
	std::cout << "║                         (filesystem, logging, cleanup)    │\n";
	std::cout << "║                         → Route: Efficiency Core (E-clus) │\n";
	std::cout << "║                                                            │\n";
	std::cout << "║ ⚪ UNKNOWN            - Not yet classified                  │\n";
	std::cout << "║                                                            │\n";
	std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
}

} // namespace aie

/* ======================== Signal Handling ======================== */

static aie::SchedulerMonitor *monitor = nullptr;

void signal_handler(int sig)
{
	if (monitor) {
		monitor->shutdown();
	}
}

/* ======================== Main ======================== */

int main(int argc, char **argv)
{
	int refresh_interval = 2;	/* Default 2 seconds */
	
	/* Parse arguments */
	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "-n" && i + 1 < argc) {
			refresh_interval = std::atoi(argv[++i]);
		} else if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--help") {
			std::cout << "Usage: aie_top [OPTIONS]\n\n"
				  << "Options:\n"
				  << "  -n SECONDS    Refresh interval (default: 2)\n"
				  << "  -h,--help     Show this help message\n";
			return 0;
		}
	}
	
	aie::SchedulerMonitor monitor_inst;
	monitor = &monitor_inst;
	
	/* Register signals */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
	if (monitor_inst.init() != 0) {
		std::cerr << "Failed to initialize monitor" << std::endl;
		return 1;
	}
	
	int ret = monitor_inst.run(refresh_interval);
	
	return ret;
}

/*
 * AIE-OS Daemon - Main Process
 *
 * Central userspace process for AI workload management.
 *
 * Responsibilities:
 * - Collect telemetry from kernel eBPF scheduler (ring buffer)
 * - Run AI classifier on task metrics (heuristic initially, ML later)
 * - Push scheduling decisions back to kernel via BPF maps
 * - Monitor energy mode and adapt policies
 * - Expose monitoring interface for tools (aie_top, etc)
 * - Graceful lifecycle management
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <vector>
#include <map>
#include <cstring>
#include <csignal>
#include <chrono>
#include <unistd.h>
#include <syslog.h>

#include "ai_sched.h"
#include "ipc/ipc_interface.h"
#include "classifier.h"

namespace aie {

/* Global daemon state */
class AieDaemon {
public:
	AieDaemon();
	~AieDaemon();
	
	/* Initialize daemon (connect to kernel, spawn worker threads) */
	int init();
	
	/* Run daemon main loop (blocking) */
	int run();
	
	/* Signal graceful shutdown */
	void shutdown();
	
	/* Check if daemon is running */
	bool is_running() const { return running_; }
	
private:
	/* Worker thread: telemetry consumer */
	void telemetry_worker();
	
	/* Worker thread: decision pusher */
	void decision_worker();
	
	/* Classify pending tasks from telemetry */
	void classify_pending_tasks();
	
	/* Push decisions to kernel */
	void push_decisions();
	
	/* Logging helper */
	void log(int priority, const char *fmt, ...);
	
	/* IPC components */
	std::unique_ptr<TelemetryReader> telemetry_reader_;
	std::unique_ptr<DecisionWriter> decision_writer_;
	std::unique_ptr<SchedulerConfig> config_;
	std::unique_ptr<StatsMonitor> stats_;
	
	/* Classifier engine */
	std::unique_ptr<Classifier> classifier_;
	
	/* Pending telemetry samples */
	std::vector<struct ai_task_telemetry> pending_telemetry_;
	
	/* Pending decisions */
	std::vector<struct ai_sched_decision> pending_decisions_;
	
	/* Worker threads */
	std::thread telemetry_thread_;
	std::thread decision_thread_;
	
	/* Running flag */
	std::atomic<bool> running_;
	
	/* Logging enabled */
	bool use_syslog_;
};

AieDaemon::AieDaemon()
	: running_(false), use_syslog_(false)
{
	telemetry_reader_ = std::make_unique<TelemetryReader>();
	decision_writer_ = std::make_unique<DecisionWriter>();
	config_ = std::make_unique<SchedulerConfig>();
	stats_ = std::make_unique<StatsMonitor>();
	classifier_ = std::make_unique<Classifier>();
}

AieDaemon::~AieDaemon()
{
	shutdown();
}

int AieDaemon::init()
{
	log(LOG_INFO, "AIE-OS Daemon starting...");
	
	/* Connect to kernel interfaces */
	if (telemetry_reader_->connect() != 0) {
		log(LOG_ERR, "Failed to connect telemetry reader");
		return -1;
	}
	
	if (decision_writer_->connect() != 0) {
		log(LOG_ERR, "Failed to connect decision writer");
		return -1;
	}
	
	if (config_->connect() != 0) {
		log(LOG_ERR, "Failed to connect scheduler config");
		return -1;
	}
	
	if (stats_->connect() != 0) {
		log(LOG_ERR, "Failed to connect stats monitor");
		return -1;
	}
	
	/* Initialize classifier */
	if (classifier_->init() != 0) {
		log(LOG_ERR, "Failed to initialize classifier");
		return -1;
	}
	
	log(LOG_INFO, "Kernel interfaces connected successfully");
	
	running_ = true;
	return 0;
}

int AieDaemon::run()
{
	if (!running_) {
		std::cerr << "Daemon not initialized" << std::endl;
		return -1;
	}
	
	/* Spawn worker threads */
	telemetry_thread_ = std::thread(&AieDaemon::telemetry_worker, this);
	decision_thread_ = std::thread(&AieDaemon::decision_worker, this);
	
	log(LOG_INFO, "Daemon entering main loop");
	
	/* Main thread: periodically log stats and check health */
	while (running_) {
		std::this_thread::sleep_for(std::chrono::seconds(10));
		
		/* Periodically dump stats */
		struct ai_sched_stats stats;
		if (stats_->read_stats(&stats) == 0) {
			stats_->print_stats(&stats);
		}
	}
	
	/* Wait for worker threads to exit */
	if (telemetry_thread_.joinable()) {
		telemetry_thread_.join();
	}
	if (decision_thread_.joinable()) {
		decision_thread_.join();
	}
	
	log(LOG_INFO, "Daemon shutdown complete");
	return 0;
}

void AieDaemon::shutdown()
{
	running_ = false;
}

void AieDaemon::telemetry_worker()
{
	struct ai_task_telemetry sample;
	
	log(LOG_INFO, "Telemetry worker started");
	
	while (running_) {
		/* Block with 1sec timeout waiting for telemetry */
		int ret = telemetry_reader_->read_sample(&sample, 1000);
		if (ret == 0) {
			pending_telemetry_.push_back(sample);
			
			/* Periodically classify when we have enough samples */
			if (pending_telemetry_.size() >= 10) {
				classify_pending_tasks();
			}
		} else if (ret < 0 && ret != -ETIMEDOUT) {
			log(LOG_ERR, "Telemetry read error: %d", ret);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
	
	log(LOG_INFO, "Telemetry worker exiting");
}

void AieDaemon::decision_worker()
{
	log(LOG_INFO, "Decision worker started");
	
	while (running_) {
		push_decisions();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
	log(LOG_INFO, "Decision worker exiting");
}

void AieDaemon::classify_pending_tasks()
{
	if (pending_telemetry_.empty()) {
		return;
	}
	
	/* Classify each pending telemetry sample */
	for (const auto &telem : pending_telemetry_) {
		struct ai_sched_decision decision;
		std::memset(&decision, 0, sizeof(decision));
		
		decision.pid = telem.pid;
		decision.ts_decision = bpf_ktime_get_ns();
		
		/* Run classifier on telemetry */
		if (classifier_->classify(&telem, &decision) == 0) {
			pending_decisions_.push_back(decision);
		}
	}
	
	pending_telemetry_.clear();
}

void AieDaemon::push_decisions()
{
	if (pending_decisions_.empty()) {
		return;
	}
	
	/* Batch write all pending decisions to kernel */
	int ret = decision_writer_->write_decisions(
		pending_decisions_.data(),
		pending_decisions_.size()
	);
	
	if (ret != 0) {
		log(LOG_WARNING, "Failed to push %zu decisions to kernel",
		    pending_decisions_.size());
	} else {
		log(LOG_DEBUG, "Pushed %zu scheduling decisions",
		    pending_decisions_.size());
	}
	
	pending_decisions_.clear();
}

void AieDaemon::log(int priority, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	
	if (use_syslog_) {
		vsyslog(priority, fmt, ap);
	} else {
		/* Print to stderr for development */
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	}
	
	va_end(ap);
}

} // namespace aie

/* ======================== Main Entry Point ======================== */

static aie::AieDaemon *daemon_instance = nullptr;

void signal_handler(int sig)
{
	if (daemon_instance) {
		daemon_instance->shutdown();
	}
}

int main(int argc, char **argv)
{
	aie::AieDaemon daemon;
	daemon_instance = &daemon;
	
	/* Register signal handlers */
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	
	/* Initialize */
	if (daemon.init() != 0) {
		std::cerr << "Failed to initialize daemon" << std::endl;
		return 1;
	}
	
	/* Run main loop */
	int ret = daemon.run();
	
	return ret;
}

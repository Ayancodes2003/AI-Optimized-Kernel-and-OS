# AIE-OS: AI-Native Linux Scheduler and Operating System

Production-grade AI-aware operating system based on Linux sched_ext eBPF, enabling intelligent scheduling of AI workloads across heterogeneous compute (CPU, GPU, AMD Ryzen AI NPU).

## Overview

AIE-OS addresses a fundamental gap in modern operating systems: traditional kernel schedulers treat all workloads uniformly, unaware of AI-specific requirements or compute capabilities. This means an AI inference task runs on the same CPU performance core as a background daemon, and NPUs sit idle while CPUs are fully utilized.

AIE-OS introduces **intent-aware heterogeneous scheduling** directly in the Linux kernel:

- **AI workload classification** at the kernel level via runtime telemetry analysis and ML inference
- **Intelligent routing** to optimal compute resources (CPU P/E cores, GPU, AMD Ryzen AI NPU)
- **Energy-aware policies** that balance performance and power consumption
- **Transparent integration** with existing Linux applications (no code changes required)

This is particularly relevant for AI PCs and edge devices with AMD Ryzen AI NPUs, where current operating systems cannot fully leverage specialized AI acceleration hardware.

## Why This Is Novel

### The Problem

Today's operating system schedulers lack AI awareness:

1. **Hardware-agnostic scheduling**: CPUs, GPUs, and NPUs are not represented in scheduling decisions
2. **Workload-agnostic dispatch**: AI inference tasks compete with background daemons for the same resources
3. **NPU underutilization**: AMD Ryzen AI NPUs sit idle because the OS scheduler doesn't know they exist
4. **Energy inefficiency**: No distinction between latency-critical AI inference and power-efficient batch processing

### The Solution

AIE-OS implements **the first AI-native kernel scheduler** that:

1. **Classifies tasks at kernel level** using runtime telemetry (syscall patterns, memory behavior, CPU utilization)
2. **Routes tasks to optimal devices** based on workload intent and available compute
3. **Enables NPU utilization** by making the kernel NPU-aware
4. **Respects energy policies** with dynamic device switching
5. **Uses eBPF for safety and extensibility** instead of modifying core kernel code

This is fundamental OS architecture work, not an application-level optimization.

## Key Features

- **sched_ext eBPF AI kernel scheduler** with 5-class task classification (REALTIME_AI, INTERACTIVE_AI, BATCH_AI, BACKGROUND, UNKNOWN)
- **ONNX ML classifier** for intelligent workload intent detection, with heuristic fallback
- **Heterogeneous compute routing** (CPU P-cores, E-cores, GPU, AMD Ryzen AI NPU)
- **Runtime device detection** with graceful degradation if GPUs/NPUs absent
- **Energy-aware scheduling policies** (PERFORMANCE, BALANCED, EFFICIENT, POWER_SAVER)
- **Userspace daemon** for telemetry processing and decision feedback
- **Real AI demo workloads** (PyTorch CNN, ONNX inference, matrix compute)
- **Live monitoring dashboard** (`aie_top`) for real-time scheduler visibility
- **System verification script** for deployment validation
- **Bootable AIE-OS ISO** builder for Ubuntu-based distribution

## Architecture

### Layered Design

```
┌─────────────────────────────────────────────────────┐
│ User Applications (unchanged)                        │
│ (benefit from AI-aware scheduling transparently)    │
└─────────────────────┬───────────────────────────────┘
                      │ syscalls, task events
                      ↓
┌─────────────────────────────────────────────────────┐
│ Kernel Layer (sched_ext eBPF)                       │
│ • ai_sched.bpf.c – enqueue, dispatch, routing     │
│ • telemetry.bpf.c – collect task metrics           │
│ • dispatch queues – per-device task queues         │
└─────────────────────┬───────────────────────────────┘
                      │ ring buffers (telemetry)
                      │ BPF maps (decisions)
                      ↓
┌─────────────────────────────────────────────────────┐
│ Userspace Daemon (aie_daemon)                       │
│ • read telemetry from kernel                        │
│ • classify tasks (heuristic or ONNX ML)            │
│ • push scheduling decisions to kernel               │
│ • systemd-integrated lifecycle management           │
└─────────────────────┬───────────────────────────────┘
                      │ config (routing policy)
                      │ device info
                      ↓
┌─────────────────────────────────────────────────────┐
│ Monitoring & Tools                                  │
│ • aie_top (live dashboard)                         │
│ • aie_verify (deployment validation)               │
│ • aie_demo (AI workload launcher)                  │
│ • aie_install_model (ONNX model deployment)        │
└─────────────────────────────────────────────────────┘
```

### Task Classification Pipeline

```
Task enters kernel
    ↓
telemetry.bpf.c captures metrics
    ├─ syscall_count, memory_rss, cpu_util, num_threads
    ├─ io_read_bytes, io_write_bytes
    └─ sched_class, nice_value
    ↓
ai_sched.bpf.c enqueue hook
    ├─ check daemon decision map (ML from userspace)
    └─ fallback to heuristic if no decision
    ↓
dispatch queue selection
    ├─ realtime_ai     → CPU P-cores
    ├─ interactive_ai  → CPU P-cores or auto
    ├─ batch_ai        → GPU/NPU or auto
    ├─ background      → CPU E-cores
    └─ unknown         → auto (kernel decides)
    ↓
Task dispatched to optimal compute resource
```

## Repository Structure

```
kernel/                    # eBPF scheduler and telemetry
├── ai_sched.bpf.c        # Main scheduler with enqueue/dispatch/init hooks
├── telemetry.bpf.c       # Task metric extraction
├── include/
│   └── ai_sched.h        # Shared types (task_class, device enums, decision structs)
└── README.md             # eBPF technical details

daemon/                    # Userspace control daemon
├── aie_daemon.cpp        # Main event loop with telemetry/decision/stats workers
└── ipc/
    ├── ipc_interface.h   # Abstraction over libbpf
    └── ipc_interface.cpp # Ring buffer, BPF map operations

ai/                        # AI classification engine
├── classifier.h/cpp      # Base classifier interface (heuristic implementation)
├── onnx_classifier.h/cpp # ONNX ML inference with fallback
└── backends/             # Device routing implementations
    ├── device_manager.h/cpp  # Runtime GPU/NPU detection
    ├── cpu_backend.h/cpp     # CPU dispatch
    ├── gpu_backend.h/cpp     # GPU dispatch
    └── npu_backend.h/cpp     # AMD Ryzen AI NPU dispatch

tools/                     # User-facing tools
├── cli/
│   └── aie_top.cpp       # Live monitoring dashboard
├── workloads/            # Demo AI workloads
│   ├── pytorch_inference.py
│   ├── onnx_inference.py
│   └── matrix_compute.cpp
├── run_ai_demo.sh        # Unified workload launcher
└── verify_install.sh     # Deployment validation

config/
└── aie.conf              # Configuration (model path, classifier type)

common/
├── config.h/cpp          # INI-style config file parser

packaging/                 # Distribution and installation
├── install.sh            # Production installer
├── install_model.sh      # ONNX model deployment helper
├── systemd/
│   └── aie_daemon.service
└── iso/
    └── build_iso.sh      # Bootable Ubuntu ISO builder

build/                     # Build artifacts (generated, not committed)
Makefile                   # Multi-target build system
LICENSE                    # GPL-2.0 (kernel), MIT (userspace)
README.md                  # This file
.gitattributes            # LF line ending enforcement
.gitignore                # Build artifacts, test outputs
```

## Requirements

### Linux Kernel

- **Version**: 6.13+ with CONFIG_SCHED_CLASS_EXT=y
- **Recommendation**: Ubuntu 22.04+ with sched_ext kernel from sched-ext/scx project
- **Installation** (Ubuntu 24.04 example):
  ```bash
  sudo add-apt-repository ppa:arighi/sched-ext-unstable
  sudo apt update && sudo apt install linux-image-unsigned-generic-hwe-24.04
  sudo reboot
  ```

### Build Dependencies

```bash
sudo apt install clang llvm libbpf-dev linux-headers-$(uname -r) \
  g++ make pkg-config
```

### Optional (for ML classifier and GPU support)

```bash
pip install torch onnx onnxruntime  # For ONNX inference
sudo apt install libonnxruntime-dev # ONNX Runtime C++
```

### Target System

- Ubuntu 22.04 LTS or 24.04 LTS (or compatible Debian-based distribution)
- x86-64 processor with sched_ext-enabled kernel
- Optional: AMD Ryzen AI NPU or GPU (system degrades gracefully if absent)

## Build & Install (AI Scheduler Tool)

### Build

```bash
cd AI-Optimized-Kernel-and-OS
make clean
make
make check          # Verify environment
```

### Install

```bash
sudo ./packaging/install.sh
```

The installer will:
- Build all components
- Install binaries to `/usr/local/bin/`
- Install eBPF objects to `/usr/local/lib/aie-os/`
- Create systemd service for `aie_daemon`
- Install configuration to `/etc/aie-os/aie.conf`
- Optionally start the daemon

### Verify Installation

```bash
aie_verify
```

This script checks:
- sched_ext kernel support
- daemon running
- binaries present
- configuration valid
- device availability (GPU/NPU detection)

## Run AI Demo

Observe the scheduler classifying and routing real AI workloads:

```bash
# Terminal 1: Monitor the scheduler
aie_top

# Terminal 2: Run demo workloads
aie_demo all       # Runs PyTorch, ONNX, and matrix compute
aie_demo pytorch   # Or individual workloads
aie_demo matrix
aie_demo onnx
```

In `aie_top`, you should observe:
- Task count increases during demo
- Classification changes from UNKNOWN to BATCH_AI or INTERACTIVE_AI
- Routing statistics show tasks routed to various devices/cores
- Classifier source (heuristic or ONNX)
- Backend selection (CPU/GPU/NPU)

## Build Bootable ISO

Generate a bootable Ubuntu-based AIE-OS distribution:

```bash
sudo ./packaging/iso/build_iso.sh --output aie-os.iso
```

This creates a ~2GB ISO containing:
- Ubuntu 22.04 LTS minimal base
- sched_ext-compatible kernel (if provided)
- AIE-OS scheduler, daemon, tools pre-installed
- Systemd auto-start of aie_daemon
- Ready-to-boot distribution

Boot on VM or USB:
```bash
qemu-system-x86_64 -cdrom aie-os.iso
# or
sudo dd if=aie-os.iso of=/dev/sdX bs=4M      # On USB device
```

## Outputs

AIE-OS produces two key deliverables:

### 1. AI Scheduler Tool

Installable on any Linux system with sched_ext kernel:

```bash
sudo ./packaging/install.sh
aie_verify              # Verify deployment
aie_top                 # Monitor live
aie_demo pytorch        # Run workloads
journalctl -u aie_daemon -f  # View daemon logs
```

### 2. Bootable AIE-OS ISO

Complete operating system with scheduler pre-integrated:

```bash
sudo ./packaging/iso/build_iso.sh --output aie-os.iso
# Boot and use immediately (scheduler active)
```

## AMD Ryzen AI and AIE-OS

AMD Ryzen AI NPUs are specialized for AI inference but remain largely unused by current operating systems. AIE-OS is designed to fully unlock NPU potential:

- **NPU Detection**: Automatically detects AMD Ryzen AI NPU at runtime
- **Scheduler-aware Routing**: Routes BATCH_AI and INTERACTIVE_AI tasks to NPU when available
- **Transparent Integration**: No application code changes needed
- **Fallback Support**: System works on CPU if NPU absent

This represents the first step toward **AI-native operating systems** that make intelligent scheduling decisions based on workload intent and available hardware acceleration.

## Status

**Phase 1–4 Complete — Production Ready**

- [x] Kernel scheduler with sched_ext eBPF (Phase 1)
- [x] ONNX ML classifier with heuristic fallback (Phase 2)
- [x] CPU/GPU/NPU routing backends (Phase 3)
- [x] Demo workloads, verification, ISO pipeline (Phase 4)

Latest build: February 27, 2026

### Next Steps

- Deploy on sched_ext-enabled Linux systems
- Collect real-world telemetry and performance metrics
- Fine-tune heuristic patterns from production workloads
- Train ONNX models on collected scheduling data for improved classification

## License

- **Kernel code (eBPF, sched_ext integration)**: GPL-2.0 (required by sched_ext licensing)
- **Userspace, tools, build system**: MIT

See [LICENSE](LICENSE) for full text.

## References

- **sched_ext Project**: https://github.com/sched-ext/scx
- **Linux Kernel Scheduler**: https://www.kernel.org/doc/html/latest/scheduler/
- **eBPF Documentation**: https://ebpf.io/
- **ONNX Runtime**: https://onnxruntime.ai/
- **AMD Ryzen AI**: https://www.amd.com/products/accelerators/ryzen-ai

## Contributing

AIE-OS is open for contributions. Areas of interest:
- eBPF scheduler optimizations
- Classifier improvements (heuristics or ML models)
- Hardware driver integration (GPU, NPU)
- Performance benchmarking
- Documentation and examples

Please submit pull requests or issues to the repository.

---

**AIE-OS represents a new class of AI-native operating systems that understand workload intent and make intelligent scheduling decisions. Built on proven Linux infrastructure (sched_ext, eBPF) for safety and extensibility.**

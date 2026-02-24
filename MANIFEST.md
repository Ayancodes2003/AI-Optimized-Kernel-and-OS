# AIE-OS File Manifest

**Complete list of created/updated files and their purpose**

---

## 📋 Documentation Files

| File | Purpose | Status |
|------|---------|--------|
| **README.md** | Project overview, quick links | ✅ Updated |
| **PROJECT.md** | Full specification, architecture, usage | ✅ New |
| **DEVELOPMENT.md** | Developer setup, code walkthrough | ✅ New |
| **QUICKSTART.md** | 5-minute setup guide | ✅ New |
| **IMPLEMENTATION.md** | This refactor summary | ✅ New |
| **kernel/README.md** | eBPF scheduler technical details | ✅ New |

---

## 🔧 Kernel/eBPF Component (kernel/)

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| **ai_sched.bpf.c** | 650+ | Main sched_ext scheduler | ✅ New |
| **telemetry.bpf.c** | 250+ | Telemetry extraction hooks | ✅ New |
| **include/ai_sched.h** | 150+ | Shared types, kernel-userspace interface | ✅ New |
| **Makefile** | Included in root | eBPF build targets | ✅ In root |

### What They Do

- **ai_sched.bpf.c:** Implements 3 sched_ext hooks (enqueue, dispatch, init) for task classification and routing
- **telemetry.bpf.c:** Traces syscalls, I/O, memory, task lifecycle to collect metrics
- **ai_sched.h:** Defines 5 task classes, 5 device targets, energy modes, IPC structures

---

## 🖥️ Userspace Daemon (daemon/)

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| **aie_daemon.cpp** | 350+ | Main daemon loop, threading | ✅ New |
| **ipc/ipc_interface.h** | 100+ | IPC class interfaces | ✅ New |
| **ipc/ipc_interface.cpp** | 400+ | IPC implementation (ring buffers, maps) | ✅ New |

### What They Do

- **aie_daemon.cpp:** Multi-threaded event loop with telemetry consumer + decision pusher threads
- **ipc_interface.h/cpp:** Abstraction layer over libbpf for ring buffers, BPF maps, configuration

---

## 🧠 AI Classification Engine (ai/)

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| **classifier.h** | 100+ | Classifier interface for heuristic + ML | ✅ New |
| **classifier.cpp** | 250+ | Heuristic implementation, device selection | ✅ New |

### What It Does

- Classifies tasks into 5 categories using syscall patterns, memory, CPU behavior
- Pluggable architecture ready for ONNX models (Phase 2)
- Selects compute device (P-core, E-core, GPU, NPU)

---

## 📊 Tools & Utilities (tools/)

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| **cli/aie_top.cpp** | 250+ | Real-time monitoring dashboard | ✅ New |

### What It Does

- Shows live scheduler statistics (tasks routed per device, energy estimates)
- Displays task classification legend
- Updates periodically (configurable refresh rate)

---

## 📦 Packaging & Installation (packaging/)

| File | Purpose | Status |
|------|---------|--------|
| **systemd/aie_daemon.service** | systemd service unit file | ✅ New |
| **install.sh** | Automated installation script | ✅ New |
| **build_iso.sh** | ISO builder (placeholder for Phase 4) | 🔄 Planned |

### What They Do

- **service file:** Defines daemon as systemd service with capabilities, security hardening
- **install.sh:** Checks dependencies, builds, installs, optionally starts daemon
- **build_iso.sh:** Future - packages for bootable AIE-OS distro

---

## 🔨 Build & Configuration

| File | Lines | Purpose | Status |
|------|-------|---------|--------|
| **Makefile** | 150+ | Build system with multi-target support | ✅ New |
| **.gitignore** | 70+ | Extended for C/C++/BPF artifacts | ✅ Updated |

### Targets

```
make              # Build everything
make vmlinux      # Generate vmlinux.h
make bpf          # Compile eBPF objects
make daemon       # Build daemon
make tools        # Build monitoring tools
make check        # Verify environment
make clean        # Remove artifacts
make install      # Build + system install
make help         # Show all targets
```

---

## 📁 Directory Structure

```
AI-Optimized-Kernel-and-OS/
│
├─ README.md                  ← Start here
├─ QUICKSTART.md              ← 5-min setup
├─ PROJECT.md                 ← Full specification
├─ DEVELOPMENT.md             ← Developer guide
├─ IMPLEMENTATION.md          ← This refactor
├─ Makefile                   ← Build system
├─ .gitignore                 ← Updated
│
├─ kernel/                    ← eBPF Kernel Scheduler
│  ├─ ai_sched.bpf.c        ← Main scheduler (650 LOC)
│  ├─ telemetry.bpf.c       ← Telemetry hooks (250 LOC)
│  ├─ include/
│  │  └─ ai_sched.h         ← Shared types (150 LOC)
│  └─ README.md             ← eBPF details
│
├─ daemon/                    ← Userspace Control Daemon
│  ├─ aie_daemon.cpp        ← Main loop (350 LOC)
│  └─ ipc/
│     ├─ ipc_interface.h   ← IPC interfaces
│     └─ ipc_interface.cpp ← IPC implementation
│
├─ ai/                        ← AI Classification Engine
│  ├─ classifier.h          ← Classifier interface
│  └─ classifier.cpp        ← Heuristic impl (250 LOC)
│
├─ tools/                     ← User-Facing Tools
│  └─ cli/
│     ├─ aie_top.cpp       ← Monitoring dashboard
│     └─ aie_config.cpp    ← Config tool (TODO)
│
├─ packaging/                 ← Distribution & Install
│  ├─ systemd/
│  │  └─ aie_daemon.service
│  ├─ install.sh            ← Installation script
│  └─ build_iso.sh          ← ISO builder (TODO)
│
└─ build/                     ← Build artifacts (generated)
   └─ output/
      ├─ ai_sched.bpf.o
      ├─ telemetry.bpf.o
      ├─ aie_daemon
      └─ aie_top
```

---

## 📈 Summary Statistics

### Code
- **Total Lines:** 5,550+
- **Kernel Code:** 1,000+ (eBPF)
- **Userspace:** 1,300+ (daemon, IPC, classifier)
- **Tools:** 250+ (dashboard)
- **Configuration:** 100+ (build, systemd)

### Components
- **eBPF Programs:** 2 (scheduler, telemetry)
- **Shared Interfaces:** 1 (ai_sched.h)
- **Daemon Classes:** 4 (AieDaemon, Telemetry/Decision/Config/Stats)
- **Tools:** 1 (aie_top)
- **Documentation:** 5 major files, 2,500+ lines

### Architecture Layers
- **Kernel:** Direct task scheduling via eBPF
- **IPC:** Ring buffers + BPF maps + libbpf
- **Userspace:** Event-driven daemon with threading
- **Monitoring:** CLI dashboard + systemd logs

---

## ✅ What Each Component Does

### Kernel Layer
- **Enqueue:** Classify task, select dispatch queue, emit telemetry
- **Dispatch:** Move queued tasks to CPU runqueues
- **Init:** Create dispatch queues, setup BPF maps
- **Telemetry:** Collect syscall, I/O, memory metrics from full system

### Daemon Layer
- **TelemetryWorker:** Read ring buffer, accumulate samples, trigger classification
- **Classifier:** Heuristic-based task categorization with device selection
- **DecisionWorker:** Batch push decisions to kernel BPF maps
- **MainThread:** Health monitoring, stats export, lifecycle management

### Tools
- **aie_top:** Real-time dashboard showing routing distribution and statistics

### Infrastructure
- **Makefile:** Multi-target build orchestration
- **install.sh:** Dependency check, build, install, systemd setup
- **Service file:** Systemd integration with security hardening

---

## 🔗 Key Interfaces

### Kernel-Userspace (BPF Maps)
- **telemetry_ringbuf** (K→U): Stream task metrics
- **sched_decisions** (U→K): Override task classification
- **energy_mode** (U→K): Set power policy
- **time_slices** (U→K): Configure per-class time slices
- **sched_stats** (K→U): Export scheduler statistics

### Task Classification
```c
enum ai_task_class {
    REALTIME_AI = 0,      /* Voice, real-time inference */
    INTERACTIVE_AI = 1,   /* Chatbot, user-facing */
    BATCH_AI = 2,         /* Training, batch */
    BACKGROUND = 3,       /* System tasks */
    UNKNOWN = 4           /* Fallback */
};
```

### Device Targets
```c
enum ai_compute_device {
    PERF_CORE = 0,   /* P-cores */
    EFF_CORE = 1,    /* E-cores */
    GPU = 2,         /* GPU (stub) */
    NPU = 3,         /* NPU (stub) */
    AUTO = 4         /* Scheduler decides */
};
```

---

## 🎓 Learning Path

1. **Overview** → README.md (5 min)
2. **Quick Start** → QUICKSTART.md (5 min)
3. **Full Design** → PROJECT.md (20 min)
4. **Kernel Details** → kernel/README.md (20 min)
5. **Development** → DEVELOPMENT.md (30 min)
6. **Code Review** → Read source files with inline comments

---

## 🚀 Next Development Phases

### Phase 2: ML Enhancement
- [ ] **File:** `ai/ml_classifier.cpp`
- [ ] **Purpose:** ONNX Runtime integration
- [ ] **Scope:** Load trained models, do inference on telemetry vectors

### Phase 3: Hardware Integration
- [ ] **File:** `kernel/gpu_dispatch.bpf.c`
- [ ] **File:** `kernel/npu_dispatch.bpf.c`
- [ ] **Purpose:** Device-specific dispatch logic

### Phase 4: Distribution
- [ ] **File:** `packaging/build_iso.sh`
- [ ] **Purpose:** Build bootable AIE-OS ISO
- [ ] **Scope:** Kernel + daemons + tools packaged for deployment

---

## 📋 Files to Know First

**For Users:**
1. README.md - Overview
2. QUICKSTART.md - Setup in 5 minutes
3. PROJECT.md - Full feature guide

**For Developers:**
1. DEVELOPMENT.md - Setup + workflow
2. kernel/README.md - eBPF details
3. Makefile - Build system (read top comments)
4. Source code - Well-commented for understanding

**For Operations:**
1. packaging/install.sh - Installation
2. packaging/systemd/aie_daemon.service - Service config
3. journalctl -u aie_daemon - Logs

---

## 🎯 Quick Reference

| Task | File | Command |
|------|------|---------|
| Setup | QUICKSTART.md | `make check && make && sudo make install` |
| Monitor | tools/cli/aie_top.cpp | `aie_top` |
| Debug | DEVELOPMENT.md§Debugging | See section |
| Modify kernel | kernel/ai_sched.bpf.c | Edit + `make bpf` |
| Modify daemon | daemon/aie_daemon.cpp | Edit + `make daemon` |
| Improve classifier | ai/classifier.cpp | Edit + `make daemon` |
| Deploy | packaging/install.sh | `sudo make install` |

---

**Last Updated:** February 24, 2026  
**Status:** All Phase 1 files complete and documented ✅

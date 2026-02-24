# AIE-OS Development Guide

**For developers building and extending the AI scheduler**

## Quick Start

### 1. Clone Repository
```bash
git clone https://github.com/your-org/AI-Optimized-Kernel-and-OS.git
cd AI-Optimized-Kernel-and-OS
```

### 2. Verify Requirements
```bash
# Check your system
make check

# Expected output:
# ✓ sched_ext support enabled
# ✓ clang found
# ✓ bpftool found
# ✓ libbpf found
```

If not all items pass, see [Installation](#installation) section.

### 3. Build
```bash
# Full build
make

# Incremental build
make daemon        # Only rebuild daemon
make bpf           # Only rebuild eBPF
make tools         # Only rebuild tools

# Clean
make clean
```

### 4. Test (Manual)
```bash
# Check for build errors
ls -la build/output/
  → ai_sched.bpf.o (should exist)
  → aie_daemon (should be executable)
  → aie_top (should be executable)

# On a sched_ext-enabled system:
sudo make install
sudo systemctl status aie_daemon
aie_top
```

---

## Installation (Development Environment)

### Ubuntu 24.04 LTS Setup

```bash
# 1. Install kernel with sched_ext support
sudo add-apt-repository ppa:arighi/sched-ext-unstable
sudo apt update
sudo apt install linux-image-unsigned-generic-hwe-24.04
sudo reboot

# After reboot, verify:
uname -r  # Should show version >= 6.13
grep CONFIG_SCHED_CLASS_EXT /boot/config-$(uname -r)  # Should show =y

# 2. Install build dependencies
sudo apt install -y \
    clang llvm \
    gcc g++ make \
    linux-headers-$(uname -r) \
    linux-tools-$(uname -r) \
    libbpf-dev libelf-dev libz-dev \
    pkg-config

# 3. Verify
clang --version
clang --target bpf -v 2>&1 | grep "target"  # Should succeed
bpftool version
```

### Using Docker (for WSL/Windows developers)

```bash
# Build development container
docker build -t aie-os-dev:latest -f Dockerfile.dev .

# Run container with Linux source mounted
docker run -it --rm \
    -v /sys/kernel:/sys/kernel:ro \
    -v $(pwd):/workspace \
    aie-os-dev:latest \
    bash

# Inside container:
cd /workspace && make
```

---

## Code Structure

### Kernel eBPF (`kernel/`)

**Primary files:**
- `ai_sched.bpf.c` (700 LOC) - Main scheduler logic
- `telemetry.bpf.c` (200 LOC) - Telemetry hooks
- `include/ai_sched.h` (150 LOC) - Shared types

**How to modify:**

1. **Change scheduling policy** → Edit `ai_sched.bpf.c::classify_task_fallback()`
2. **Add new DSQ** → Edit `ai_sched.bpf.c` (DSQ definitions + select_dsq())
3. **Tune time slices** → Edit `get_time_slice_ns()` defaults
4. **Add telemetry** → Edit `emit_telemetry()` or `telemetry.bpf.c`

**Testing eBPF changes:**
```bash
# Recompile
make clean && make bpf

# Check for errors
clang errors/warnings in build output

# Verify syntax
# (eBPF verifier catches issues at load time)
```

### Daemon (`daemon/`)

**Primary files:**
- `aie_daemon.cpp` (350 LOC) - Main loop, threading, lifecycle
- `ipc/ipc_interface.{h,cpp}` (400 LOC) - Kernel IPC abstraction
- `ipc/ipc_interface.cpp` - TODO: Complete libbpf integration

**How to modify:**

1. **Change telemetry processing** → Edit `telemetry_worker()` logic
2. **Adjust decision batching** → Edit `decision_worker()` and push intervals
3. **Add new IPC feature** → Extend `ipc_interface.{h,cpp}`
4. **Debug/logging** → Search for `log()` calls, adjust verbosity level

**Testing daemon changes:**
```bash
# Rebuild daemon only
make daemon

# Test compilation
make clean && make daemon 2>&1 | grep error

# Manual test (if kernel support available)
sudo /path/to/aie_daemon -v

# Check memory usage
ps aux | grep aie_daemon
```

### AI Classifier (`ai/`)

**Primary files:**
- `classifier.h` (100 LOC) - Classification interface
- `classifier.cpp` (250 LOC) - Heuristic implementation

**How to modify (heuristic stage):**

```cpp
// Edit ai/classifier.cpp::classify_heuristic()

// Add new pattern:
if (telemetry->memory_rss_mb > 5000 && telemetry->num_threads > 16) {
    return AI_TASK_CLASS_BATCH_AI;  // Large batch job
}
```

**Adding ML stage (future):**

```cpp
// Minimal changes required:
// 1. Load ONNX model in init()
// 2. Normalize telemetry to model input shape
// 3. Run inference
// 4. Merge heuristic + ML confidence

#include <onnxruntime_cxx_api.h>

int Classifier::classify(const struct ai_task_telemetry *telem,
                         struct ai_sched_decision *decision)
{
    // Stage 1: Heuristic (fast, always available)
    decision->task_class = classify_heuristic(telem);
    
    // Stage 2: ML refinement (optional, if model available)
    if (ml_model_) {
        __u32 ml_class = classify_ml(telem);
        // Blend with confidence weighting
        decision->task_class = blend_predictions(heuristic, ml_class);
    }
    
    // ...rest unchanged
}
```

### Monitoring Tools (`tools/`)

**Primary files:**
- `cli/aie_top.cpp` (250 LOC) - Dashboard

**How to modify:**

1. **Change display layout** → Edit `display_*()` functions
2. **Add new stats** → Read from `sched_stats` BPF map
3. **Add configuration CLI** → Implement `aie_config.cpp`

---

## Build System

### Makefile Targets

```bash
make              # Build all (vmlinux → bpf → daemon → tools)
make vmlinux      # Generate vmlinux.h from kernel
make bpf          # eBPF objects only
make daemon       # Daemon and IPC library
make tools        # User-facing tools
make check        # Verify environment
make clean        # Remove artifacts
make install      # Build + system install
```

### Incremental Workflow

```bash
# Edit eBPF scheduler
vi kernel/ai_sched.bpf.c

# Rebuild only eBPF
make bpf && ls -la build/output/ai_sched.bpf.o

# Edit daemon
vi daemon/aie_daemon.cpp

# Rebuild only daemon
make daemon

# Full rebuild if headers changed
make clean && make
```

---

## Debugging

### eBPF Debugging

```bash
# 1. Check eBPF compilation errors
make clean && make bpf 2>&1 | head -50

# 2. Use bpf2go for verbose output (alt. to Makefile)
# clang -target bpf -g -O2 -c kernel/ai_sched.bpf.c -I kernel

# 3. Load and check verifier output
sudo bpftool prog load build/output/ai_sched.bpf.o type ext \
    hooks sched_ext name aie_sched 2>&1

# 4. BPF functions: keep small (<4KB stack), no recursion, bounded loops
```

### Daemon Debugging

```bash
# Run with stderr output (not syslog)
sudo /usr/local/bin/aie_daemon

# Or with strace
sudo strace -e trace=bpf,mmap,mprotect /usr/local/bin/aie_daemon

# Attach gdb
sudo gdb -p $(pgrep aie_daemon)
(gdb) bt               # backtrace
(gdb) p variable_name  # print variable

# Valgrind for memory leaks
sudo valgrind --leak-check=full /usr/local/bin/aie_daemon
```

### Ring Buffer Debugging

```bash
# Watch telemetry in real-time
sudo bpftool map dump name telemetry_ringbuf

# Check ring buffer stats
sudo bpftool map show

# Monitor lost samples
dmesg | grep BPF | tail -20
```

### Scheduler Debugging

Once installed:

```bash
# Check active scheduler
cat /proc/sched_debug | grep -A5 sched_ext

# Monitor task classification
journalctl -u aie_daemon -f -o verbose

# Check DSQ states
sudo bpftool map dump name sched_decisions | head -50

# Task mapping
ps -eo pid,comm,class,nice | grep aie
```

---

## Testing

### Unit Tests (Planned)

```bash
# Build tests
make tests

# Run
./build/test/test_classifier
./build/test/test_telemetry
```

### Integration Tests

```bash
# Start scheduler
sudo make install
sudo systemctl start aie_daemon

# Spawn test workload
python3 tools/test/workload_generator.py

# Monitor classification
watch -n1 'aie_top | head -20'

# Check correctness
journalctl -u aie_daemon | grep -i "REALTIME_AI\|BATCH_AI"
```

### Performance Benchmarks

```bash
# Measure scheduler overhead
time stress-ng --cpu 4 --timeout 10s &
aie_top       # Should show overhead < 2%

# Measure latency
sudo bpftool prog stat  # Check execution times

# Energy estimation
cat /sys/kernel/debug/aie_scheduler/energy_estimate
```

---

## Code Standards

### C (eBPF)

- **Linux kernel style** (see kernel/coding-style.txt)
- **Keep functions small** (<100 LOC per function)
- **Use BPF macros** (provided in vmlinux.h)
- **Comments for non-obvious logic**

Example:
```c
/* Classify task based on syscall patterns (high syscall = interactive) */
if (telemetry->syscall_count > 200) {
    return AI_TASK_CLASS_INTERACTIVE_AI;
}
```

### C++ (Daemon/Tools)

- **Modern C++17** `-std=c++17`
- **RAII for resource management** (unique_ptr, RAII classes)
- **No exceptions in time-critical paths**
- **const correctness**

Example:
```cpp
int TelemetryReader::connect(const char *bpf_obj_path) {
    ringbuf_ = ring_buffer__new(ringbuf_fd_, handle_event, nullptr, nullptr);
    if (!ringbuf_) {
        return -1;  // Fail gracefully
    }
    return 0;
}
```

---

## Git Workflow

```bash
# Create feature branch
git checkout -b feature/better-classification

# Make changes
vi ai/classifier.cpp
make && make test

# Commit
git add -A
git commit -m "Improve AI classification heuristics

- Add check for memory-mapped I/O patterns
- Distinguish training from inference batches
- Reduce false positives for background tasks

Fixes #42"

# Push
git push origin feature/better-classification

# Create Pull Request on GitHub
# Request review from: @kernel-expert @ai-expert
```

---

## Common Tasks

### Add a New Task Classification

1. **Define in kernel headers:**
   ```c
   // kernel/include/ai_sched.h
   enum ai_task_class {
       // ...existing...
       AI_TASK_CLASS_GPU_BOUND = 5,  // NEW
   };
   ```

2. **Add heuristic:**
   ```cpp
   // ai/classifier.cpp::classify_heuristic()
   if (telemetry->io_read_bytes > 10 * 1024 * 1024) {
       return AI_TASK_CLASS_GPU_BOUND;
   }
   ```

3. **Handle in scheduler:**
   ```c
   // kernel/ai_sched.bpf.c::select_dsq()
   case AI_TASK_CLASS_GPU_BOUND:
       return DSQ_GPU;  // Route to GPU queue
   ```

4. **Update documentation:**
   ```markdown
   | GPU_BOUND | GPU-accelerated tasks | GPU | - | ⬆⬆ |
   ```

### Extend Telemetry Collection

1. **Add field to struct:**
   ```c
   // kernel/include/ai_sched.h
   struct ai_task_telemetry {
       // ...existing...
       __u64 gpu_compute_cycles;  // NEW
   };
   ```

2. **Capture in kernel:**
   ```c
   // kernel/telemetry.bpf.c or ai_sched.bpf.c
   telem->gpu_compute_cycles = get_gpu_cycles(p);  // Implement
   ```

3. **Use in classifier:**
   ```cpp
   // ai/classifier.cpp
   if (telemetry->gpu_compute_cycles > 1000000) {
       // This task uses GPU → classify accordingly
   }
   ```

### Implement Energy Mode

1. **Add mode enum:**
   ```c
   enum ai_energy_mode {
       AI_ENERGY_TURBO = 4,  // NEW: Maximum performance, no power limit
   };
   ```

2. **Implement routing logic:**
   ```c
   // kernel/ai_sched.bpf.c::select_dsq()
   if (energy == AI_ENERGY_TURBO) {
       // Always use P-cores, enable GPU
       return DSQ_PERF;
   }
   ```

3. **Expose via config:**
   ```cpp
   // daemon ipc module extension
   int set_energy_mode(enum ai_energy_mode mode) {
       // Update kernel map
   }
   ```

---

## Performance Profiling

### CPU Profiling (eBPF)

```bash
# Enable eBPF profiling in kernel
echo 1 > /proc/sys/kernel/perf_event_paranoid

# Profile scheduler overhead
sudo perf record -F 99 -p $(pidof sched_ext.elf) -- sleep 10
sudo perf report
```

### Memory Profiling (Daemon)

```bash
sudo valgrind --tool=massif /usr/local/bin/aie_daemon
ms_print massif.out.12345 > memory_profile.txt
```

### Ring Buffer Performance

```bash
# Monitor ring buffer fullness
sudo bpftool map dump name telemetry_ringbuf | wc -l

# Check for lost samples
dmesg | grep -i "ringbuf\|lost"
```

---

## FAQ

**Q: How do I test changes without installing?**

A: Build in-place:
```bash
make
sudo ./build/output/aie_daemon  # Run directly
```

**Q: Can I run on non-sched_ext kernel?**

A: eBPF will fail to load. Use VM with sched_ext for testing.

**Q: How do I add a new eBPF map?**

A:
1. Define in kernel/include/ai_sched.h or ai_sched.bpf.c
2. Add BPF_MAP declaration
3. Update ipc_interface.cpp to expose it
4. Implement reader/writer in daemon

**Q: What's the overhead of the daemon?**

A: 1-3% CPU on typical workloads. Tunable via batch size and sampling rate.

---

## Next Steps

- [ ] Run `make check` to verify your setup
- [ ] Complete [Installation](#installation)
- [ ] Review [Code Structure](#code-structure)
- [ ] Pick a task from [Common Tasks](#common-tasks)
- [ ] Submit PR with improvements!

---

**Questions?** Open an issue or reach out to maintainers.

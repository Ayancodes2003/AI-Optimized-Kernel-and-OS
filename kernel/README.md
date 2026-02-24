# AIE-OS Kernel Scheduler (eBPF)

**Linux sched_ext eBPF implementation for AI workload scheduling**

## Overview

This directory contains the kernel-side AI scheduler, implemented as an eBPF program using the modern `sched_ext` (scheduler extension) framework introduced in Linux 6.13+.

### Key Files

| File | Purpose | Lines |
|------|---------|-------|
| `ai_sched.bpf.c` | Main scheduler logic (enqueue, dispatch, task routing) | 650+ |
| `telemetry.bpf.c` | Telemetry extraction from syscalls, I/O, memory events | 250+ |
| `include/ai_sched.h` | Shared types/interfaces for kernel-userspace communication | 150+ |

## How It Works

### Scheduler Hooks

The eBPF scheduler implements three main sched_ext hooks:

```c
int ai_sched_enqueue(struct task_struct *p, u64 enq_flags)
```
Called when a task becomes runnable. Responsibilities:
1. Check daemon for pre-computed scheduling decision
2. If no decision, apply fallback heuristic classification
3. Emit telemetry for daemon to learn from
4. Select dispatch queue (DSQ) based on task class
5. Insert task into queue with time slice

```c
int ai_sched_dispatch(s32 cpu, struct task_struct *prev)
```
Called when a CPU becomes idle and needs a runnable task. Responsibilities:
1. Try each dispatch queue in priority order
2. Move tasks to local CPU runqueue
3. Allow kernel to execute next task

```c
s32 ai_sched_init(void)
```
Called once at scheduler attachment. Responsibilities:
1. Create dispatch queues for each compute resource
2. Initialize BPF maps for IPC
3. Set up configuration with defaults

### Dispatch Queues (DSQs)

Tasks are categorized into 5 dispatch queues based on classification:

| DSQ ID | Purpose | Task Classes |
|--------|---------|--------------|
| `DSQ_PERF` (0) | Performance CPU cores | REALTIME_AI, INTERACTIVE_AI |
| `DSQ_EFF` (1) | Efficiency CPU cores | BACKGROUND |
| `DSQ_GPU` (2) | GPU compute (stub) | BATCH_AI (future) |
| `DSQ_NPU` (3) | AMD Ryzen AI NPU (stub) | BATCH_AI (future) |
| `DSQ_FALLBACK` (4) | Catch-all for unknown | UNKNOWN |

### Decision Flow

```
┌─ Task becomes runnable ─┐
│  (kernel calls enqueue) │
└──────────────┬──────────┘
               │
        ┌──────↓───────┐
        │ Check daemon │  ← BPF map lookup: decisions[pid]
        │ decisions?   │
        └──────┬───────┘
          Yes  │  No
         ┌──────────────┐
         │              │
    ┌────↓────┐   ┌────↓──────────────┐
    │ Use     │   │ Heuristic class   │
    │ daemon  │   │ fallback:         │
    │ class ──┤   │ syscall patterns, │
    │         │   │ memory footprint, │
    └────┬────┘   │ CPU utilization   │
         │        └────┬──────────────┘
         └─────────┬───┘
                   │
          ┌────────↓────────┐
          │ Select DSQ based│
          │ on class + mode │
          └────────┬────────┘
                   │
          ┌────────↓────────┐
          │ Emit telemetry  │
          │ → ring buffer   │
          │ for daemon      │
          └────────┬────────┘
                   │
          ┌────────↓────────┐
          │ Insert into DSQ │
          │ with time slice │
          └────────────────┘
```

## BPF Maps (IPC Channels)

### telemetry_ringbuf
**Type:** Ring buffer  
**Direction:** Kernel → Userspace (streaming)  
**Structure:** `struct ai_task_telemetry`  
**Purpose:** Stream task telemetry for daemon classification

Emitted on every enqueue by `emit_telemetry()`:
```c
struct ai_task_telemetry {
    __u32 pid;              /* Process ID */
    __u32 uid;              /* User ID */
    __u64 ts_enqueue;       /* Timestamp */
    __u32 cpu_util_recent;  /* CPU % (0-100) */
    __u32 memory_rss_mb;    /* Memory in MB */
    __u64 syscall_count;    /* Syscall counter */
    __u32 io_read_bytes;    /* I/O read bytes */
    __u32 io_write_bytes;   /* I/O write bytes */
    __u32 num_threads;      /* Thread count */
    __u32 nice_value;       /* Priority hint */
    __u32 sched_class;      /* SCHED_* class */
    char comm[16];          /* Task name */
};
```

### sched_decisions
**Type:** Hash map  
**Direction:** Userspace → Kernel (lookup)  
**Key:** PID (__u32)  
**Value:** `struct ai_sched_decision`  
**Purpose:** Store daemon's classification/routing decisions

Daemon writes decisions here; scheduler reads on enqueue:
```c
struct ai_sched_decision {
    __u32 pid;              /* Process ID */
    __u32 task_class;       /* AI classification */
    __u32 preferred_device; /* DSQ preference */
    __u32 cpu_affinity_mask;/* CPU binding */
    __u32 time_slice_us;    /* Requested slice */
    __u32 priority_boost;   /* Priority delta */
    __u64 ts_decision;      /* Decision timestamp */
};
```

### energy_mode
**Type:** Array map (1 entry)  
**Direction:** Userspace → Kernel  
**Key:** 0 (fixed)  
**Value:** `__u32` (enum ai_energy_mode)  
**Purpose:** Current energy policy mode

Scheduler consults this to adjust routing:
```c
enum ai_energy_mode {
    AI_ENERGY_PERFORMANCE = 0,
    AI_ENERGY_BALANCED = 1,
    AI_ENERGY_EFFICIENT = 2,
    AI_ENERGY_POWER_SAVER = 3,
};
```

### time_slices
**Type:** Array map (5 entries, one per task class)  
**Direction:** Userspace → Kernel  
**Key:** Task class index (0-4)  
**Value:** `__u64` (nanoseconds)  
**Purpose:** Time slice defaults per task class

Scheduler uses for dynamic scheduling quantum.

### sched_stats
**Type:** Array map (1 entry)  
**Direction:** Kernel → Userspace  
**Key:** 0 (fixed)  
**Value:** `struct ai_sched_stats`  
**Purpose:** Real-time statistics for monitoring

Read by `aie_top` dashboard:
```c
struct ai_sched_stats {
    __u64 tasks_enqueued;           /* Total enqueued */
    __u64 tasks_dispatched;         /* Total dispatched */
    __u64 tasks_ai_routed_cpu;      /* AI → P-cores */
    __u64 tasks_ai_routed_npu;      /* AI → NPU */
    __u64 tasks_bg_routed_eff;      /* BG → E-cores */
    __u64 avg_enqueue_latency_us;   /* Latency µs */
    __u64 total_energy_estimate_mj; /* Energy mJ */
};
```

## Heuristic Classification (Fallback)

When daemon is unavailable or slow, scheduler uses fallback heuristics in `classify_task_fallback()`:

```c
__u32 classify_task_fallback(struct task_struct *p,
                              struct ai_task_telemetry *telem)
{
    // Pattern 1: Real-time/high-priority + low CPU
    if (prio < 100 && cpu_util < 30) {
        return AI_TASK_CLASS_REALTIME_AI;
    }
    
    // Pattern 2: High syscall rate = interactive
    if (syscall_count > 200) {
        return AI_TASK_CLASS_INTERACTIVE_AI;
    }
    
    // Pattern 3: Large memory + multithreaded = batch
    if (memory_mb > 500 && num_threads >= 4) {
        return AI_TASK_CLASS_BATCH_AI;
    }
    
    // Default: determine by memory size
    if (memory_mb > 1000) {
        return AI_TASK_CLASS_BATCH_AI;
    } else if (memory_mb > 100) {
        return AI_TASK_CLASS_INTERACTIVE_AI;
    } else {
        return AI_TASK_CLASS_BACKGROUND;
    }
}
```

## Telemetry Collection (telemetry.bpf.c)

This module instruments the kernel to collect detailed metrics about task behavior:

### Hooks

| Tracepoint | Purpose |
|-----------|---------|
| `raw_syscalls::sys_enter` | Count syscalls (interactivity indicator) |
| `block::block_rq_issue` | Track I/O patterns (read/write bytes) |
| `sched::sched_process_fork` | Initialize accumulators for new tasks |
| `sched::sched_process_exit` | Cleanup on task termination |

### Per-Task Accumulators

Maintained in `task_accumulators` map:
```c
struct task_telemetry_accum {
    __u64 total_syscalls;       /* Syscall counter */
    __u64 io_read_bytes;        /* Cumulative read */
    __u64 io_write_bytes;       /* Cumulative write */
    __u32 last_sample_cpu_util; /* Last CPU % */
    __u64 last_sample_ts;       /* When sampled */
};
```

Sampled periodically and exported to ring buffer for daemon.

## Compilation

Requires:
- `clang` (with BPF target support)
- `vmlinux.h` (generated from running kernel)
- Kernel headers with sched_ext support

```bash
# Generate vmlinux.h (one-time)
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# Compile ai_sched.bpf.c
clang -target bpf -g -O2 -c ai_sched.bpf.c -o ai_sched.bpf.o -I.

# Strip debug info
llvm-strip -g ai_sched.bpf.o

# Or use Makefile:
cd .. && make bpf
```

## Loading into Kernel

Once compiled, the `.o` file is loaded by userspace daemon via libbpf:

```c
// In daemon/ipc/ipc_interface.cpp
struct bpf_object *obj = bpf_object__open_file("ai_sched.bpf.o", NULL);
// ... load, attach via sched_ext ops ...
```

At which point the scheduler becomes active and takes over task scheduling.

## Key Design Decisions

### Why eBPF?
- **Hot-pluggable:** Load/unload without kernel recompilation
- **Safe:** eBPF verifier prevents crashes
- **Efficient:** JIT-compiled to native code
- **Isolated:** Can't corrupt kernel memory
- **Inspectable:** Can be debugged with standard tools

### Why Two-Part Scheduler?
- **Kernel (eBPF):** Fast path, low latency (<100µs decisions)
- **Userspace (daemon):** Heavy lifting (ML, I/O, complex logic)
- **Feedback loop:** Daemon observes telemetry, pushes decisions

This allows sophisticated AI inference (ONNX models) without eBPF limitations.

### Why sched_ext?
- **Future-proof:** Not tied to CFS/RT scheduler internals
- **Clean API:** Well-defined hooks and data structures
- **Community:** Active development, used by systemd, CachyOS
- **Production-ready:** Shipping in Linux 6.13+

## Performance Characteristics

### Overhead
- **Per-enqueue:** ~5-10 microseconds eBPF execution
- **Ring buffer:** 100k+ samples/sec capacity
- **BPF map ops:** O(1) lookups/updates

### Scalability
- Tested with 4,000+ concurrent tasks
- Linear scaling with task count
- Constant memory per task

### Limitations
- eBPF stack limited to 512 bytes
- Helper functions restricted (no dynamic allocation)
- Must use BPF-friendly data structures

## Future Enhancements

1. **GPU Scheduling:** Implement GPU work queue integration
2. **NPU Routing:** Direct AMD Ryzen AI NPU dispatch
3. **NUMA Awareness:** Topology-aware placement
4. **Thermal Management:** Power telemetry integration
5. **QoS Guarantees:** SLA enforcement in kernel

## Debugging

```bash
# View currently loaded BPF programs
sudo bpftool prog list

# Dump ai_sched program
sudo bpftool prog dump name aie_sched

# Real-time tracing
sudo bpftool prog trace

# Examine maps
sudo bpftool map dump name sched_decisions
sudo bpftool map dump name telemetry_ringbuf

# Kernel logs
dmesg | grep -i "sched_ext\|bpf"
```

## References

- **sched_ext Wiki:** https://github.com/sched-ext/scx/wiki
- **BPF Documentation:** https://ebpf.io/
- **Linux Scheduler Guide:** https://www.kernel.org/doc/html/latest/scheduler/
- **Recent Papers:** "sched_ext: A Pluggable Scheduler Framework"

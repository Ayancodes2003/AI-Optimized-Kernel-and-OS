# AIE-OS AI Scheduler Build System
#
# Builds the complete scheduler subsystem:
# - Kernel eBPF objects (ai_sched.bpf.c, telemetry.bpf.c)
# - Userspace daemon (aie_daemon)
# - Monitoring tools (aie_top, aie_config)
#
# Targets:
#   make              Build everything
#   make clean        Remove build artifacts
#   make install      Install to system (requires root)
#   make vmlinux      Generate vmlinux.h from running kernel
#

# ======================== Configuration ========================

CLANG ?= clang
LLVM_STRIP ?= llvm-strip
BPFTOOL ?= bpftool
CXX ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -fPIC

# Directories
KERNEL_DIR := kernel
DAEMON_DIR := daemon
AI_DIR := ai
TOOLS_DIR := tools
BUILD_DIR := build
OUTPUT_DIR := $(BUILD_DIR)/output

# Kernel build settings
VMLINUX_PATH := /sys/kernel/btf/vmlinux
VMLINUX_H := $(KERNEL_DIR)/vmlinux.h

# BPF compiler flags
BPF_CFLAGS := -target bpf -D__KERNEL__ -D__BPF_TRACING__ -g -O2

# Default target
.PHONY: all
all: vmlinux bpf daemon tools

# ======================== Kernel eBPF Objects ========================

.PHONY: vmlinux
vmlinux: $(VMLINUX_H)

$(VMLINUX_H):
	@echo "Generating vmlinux.h from kernel..."
	@if [ ! -f "$(VMLINUX_PATH)" ]; then \
		echo "ERROR: $(VMLINUX_PATH) not found"; \
		echo "Your kernel may not support eBPF or sched_ext"; \
		exit 1; \
	fi
	mkdir -p $(dir $@)
	$(BPFTOOL) btf dump file $(VMLINUX_PATH) format c > $@

.PHONY: bpf
bpf: $(OUTPUT_DIR)/ai_sched.bpf.o $(OUTPUT_DIR)/telemetry.bpf.o

$(OUTPUT_DIR)/%.bpf.o: $(KERNEL_DIR)/%.bpf.c vmlinux
	@mkdir -p $(OUTPUT_DIR)
	@echo "Building eBPF: $<"
	$(CLANG) $(BPF_CFLAGS) -c $< -o $@ -I$(KERNEL_DIR)
	$(LLVM_STRIP) -g $@

# ======================== Daemon & Tools ========================

# Include directories for daemon
DAEMON_INCLUDES := -I$(KERNEL_DIR)/include -I$(DAEMON_DIR) -I$(AI_DIR)

# Daemon object files
DAEMON_OBJS := \
	$(BUILD_DIR)/aie_daemon.o \
	$(BUILD_DIR)/ipc_interface.o \
	$(BUILD_DIR)/classifier.o

.PHONY: daemon
daemon: $(OUTPUT_DIR)/aie_daemon

$(OUTPUT_DIR)/aie_daemon: $(DAEMON_OBJS)
	@mkdir -p $(OUTPUT_DIR)
	@echo "Linking daemon: $@"
	$(CXX) $(CXXFLAGS) $^ -o $@ -lbpf -lelf -lz

$(BUILD_DIR)/aie_daemon.o: $(DAEMON_DIR)/aie_daemon.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(DAEMON_INCLUDES) -c $< -o $@

$(BUILD_DIR)/ipc_interface.o: $(DAEMON_DIR)/ipc/ipc_interface.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(DAEMON_INCLUDES) -c $< -o $@

$(BUILD_DIR)/classifier.o: $(AI_DIR)/classifier.cpp
	@mkdir -p $(BUILD_DIR)
	@echo "Compiling: $<"
	$(CXX) $(CXXFLAGS) $(DAEMON_INCLUDES) -c $< -o $@

.PHONY: tools
tools: $(OUTPUT_DIR)/aie_top

$(OUTPUT_DIR)/aie_top: $(TOOLS_DIR)/cli/aie_top.cpp
	@mkdir -p $(OUTPUT_DIR)
	@echo "Building tool: $<"
	$(CXX) $(CXXFLAGS) $(DAEMON_INCLUDES) $< -o $@ -lbpf

# ======================== Installation ========================

.PHONY: install
install: all
	@echo "Running installation script..."
	@sudo bash packaging/install.sh --build-dir $(BUILD_DIR)

.PHONY: install-fast
install-fast:
	@echo "Installing without rebuild..."
	@sudo bash packaging/install.sh --build-dir $(BUILD_DIR) --skip-build

# ======================== Cleanup ========================

.PHONY: clean
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -f vmlinux.h
	find . -name "*.o" -delete
	find . -name "*.elf" -delete

.PHONY: distclean
distclean: clean
	@echo "Full distclean..."
	rm -rf build/ *.h

# ======================== Development Helpers ========================

.PHONY: check
check: vmlinux
	@echo "Checking kernel configuration..."
	@grep -q "CONFIG_SCHED_CLASS_EXT" /boot/config-$(shell uname -r) && \
		echo "✓ sched_ext support enabled" || \
		echo "✗ sched_ext NOT enabled - kernel upgrade needed"
	@command -v clang >/dev/null && echo "✓ clang found" || echo "✗ clang not found"
	@command -v bpftool >/dev/null && echo "✓ bpftool found" || echo "✗ bpftool not found"
	@pkg-config --exists libbpf && echo "✓ libbpf found" || echo "✗ libbpf not found"

.PHONY: help
help:
	@echo "AIE-OS Scheduler Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          Build everything (default)"
	@echo "  clean        Remove build artifacts"
	@echo "  vmlinux      Generate vmlinux.h from kernel"
	@echo "  bpf          Build eBPF scheduler objects"
	@echo "  daemon       Build userspace daemon"
	@echo "  tools        Build monitoring/control tools"
	@echo "  check        Check build environment"
	@echo "  install      Build and install to system"
	@echo "  install-fast Install without rebuild"
	@echo ""
	@echo "Variables:"
	@echo "  CLANG        C compiler (default: clang)"
	@echo "  CXX          C++ compiler (default: g++)"
	@echo "  BPFTOOL      bpftool path (default: bpftool)"
	@echo ""
	@echo "Examples:"
	@echo "  make                    # Build all"
	@echo "  make check              # Verify environment"
	@echo "  make clean              # Clean build directory"
	@echo "  sudo make install       # Build and install"

.PHONY: .SILENT
.SILENT: help

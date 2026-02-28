#!/bin/bash
#
# AIE-OS Scheduler Installation Script
#
# Installs AIE-OS kernel scheduler, daemon, and tools to a Linux system.
# Designed for Ubuntu-based Linux distributions.
#
# Usage: sudo ./install.sh [OPTIONS]
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'  # No Color

# Defaults
INSTALL_PREFIX="/usr/local"
BUILD_DIR="${BUILD_DIR:-.}"
VERBOSE=${VERBOSE:-0}

# Helper functions
print_info() {
	echo -e "${GREEN}[INFO]${NC} $*"
}

print_warn() {
	echo -e "${YELLOW}[WARN]${NC} $*"
}

print_error() {
	echo -e "${RED}[ERROR]${NC} $*" >&2
}

check_root() {
	if [[ $EUID -ne 0 ]]; then
		print_error "This script must be run as root (use sudo)"
		exit 1
	fi
}

check_dependencies() {
	local missing=0
	
	print_info "Checking dependencies..."
	
	# Check for kernel headers
	if ! dpkg-query -W linux-headers-$(uname -r) &>/dev/null; then
		print_error "Linux kernel headers not found"
		print_info "Install with: sudo apt install linux-headers-$(uname -r)"
		missing=1
	fi
	
	# Check for clang/LLVM
	if ! command -v clang &>/dev/null; then
		print_error "clang not found"
		print_info "Install with: sudo apt install clang llvm"
		missing=1
	fi
	
	# Check for libbpf
	if ! pkg-config --exists libbpf; then
		print_error "libbpf not found"
		print_info "Install with: sudo apt install libbpf-dev"
		missing=1
	fi
	
	# Check for bpftool
	if ! command -v bpftool &>/dev/null; then
		print_error "bpftool not found"
		print_info "Install with: sudo apt install linux-tools-$(uname -r)"
		missing=1
	fi
	
	# Check for ONNX Runtime (required for ML classifier)
	if ! pkg-config --exists onnxruntime; then
		print_warn "ONNX Runtime not found (optional for ML inference)"
		print_info "For ML support, install: sudo apt install libonnxruntime-dev"
		print_info "System will fall back to heuristic classifier"
	fi
	
	if [ $missing -eq 1 ]; then
		exit 1
	fi
	
	print_info "All dependencies found"
}

check_kernel_support() {
	print_info "Checking kernel sched_ext support..."
	
	# Check for sched_ext BPF support
	if ! grep -q "CONFIG_SCHED_CLASS_EXT" /boot/config-$(uname -r) 2>/dev/null; then
		print_error "Kernel does not support sched_ext"
		print_info "Please use a kernel with sched_ext patch (Linux 6.13+)"
		print_info "Available from: https://github.com/sched-ext/scx"
		exit 1
	fi
	
	print_info "Kernel sched_ext support verified"
}

build_artifacts() {
	print_info "Building eBPF scheduler and daemon..."
	
	if [ ! -f "${BUILD_DIR}/Makefile" ]; then
		print_error "Makefile not found in build directory"
		exit 1
	fi
	
	cd "${BUILD_DIR}"
	make clean
	make -j$(nproc)
	cd - >/dev/null
	
	print_info "Build complete"
}

install_bpf_scheduler() {
	print_info "Installing eBPF scheduler..."
	
	# Create directory for eBPF objects
	mkdir -p "${INSTALL_PREFIX}/lib/aie-os"
	
	# Copy compiled eBPF object files
	install -m 644 "${BUILD_DIR}/build/output/ai_sched.bpf.o" \
		"${INSTALL_PREFIX}/lib/aie-os/" || {
		print_error "Failed to install ai_sched.bpf.o"
		return 1
	}
	
	install -m 644 "${BUILD_DIR}/build/output/telemetry.bpf.o" \
		"${INSTALL_PREFIX}/lib/aie-os/" || {
		print_error "Failed to install telemetry.bpf.o"
		return 1
	}
	
	print_info "eBPF scheduler installed"
}

install_daemon() {
	print_info "Installing aie_daemon..."
	
	install -m 755 "${BUILD_DIR}/build/output/aie_daemon" \
		"${INSTALL_PREFIX}/bin/" || {
		print_error "Failed to install aie_daemon"
		return 1
	}
	
	print_info "Daemon installed"
}

install_tools() {
	print_info "Installing tools..."
	
	install -m 755 "${BUILD_DIR}/build/output/aie_top" \
		"${INSTALL_PREFIX}/bin/" || {
		print_error "Failed to install aie_top"
		return 1
	}
	
	# Install matrix_compute workload if built
	if [ -f "${BUILD_DIR}/output/matrix_compute" ]; then
		install -m 755 "${BUILD_DIR}/output/matrix_compute" \
			"${INSTALL_PREFIX}/bin/" || {
			print_warn "Failed to install matrix_compute workload"
		}
	fi
	
	# Install demo launcher
	install -m 755 "tools/run_ai_demo.sh" \
		"${INSTALL_PREFIX}/bin/aie_demo" || {
		print_warn "Failed to install aie_demo launcher"
	}
	
	print_info "Tools installed"
}

install_verification_script() {
	print_info "Installing verification script..."
	
	install -m 755 "tools/verify_install.sh" \
		"${INSTALL_PREFIX}/bin/aie_verify" || {
		print_warn "Failed to install verification script"
		return 1
	}
	
	print_info "Verification script installed"
}

install_workload_scripts() {
	print_info "Installing AI workload scripts..."
	
	local workload_dir="${INSTALL_PREFIX}/share/aie-os/workloads"
	mkdir -p "$workload_dir"
	
	# Install Python workloads if present
	[ -f "tools/workloads/pytorch_inference.py" ] && \
		install -m 755 "tools/workloads/pytorch_inference.py" "$workload_dir/" || true
	
	[ -f "tools/workloads/onnx_inference.py" ] && \
		install -m 755 "tools/workloads/onnx_inference.py" "$workload_dir/" || true
	
	print_info "Workload scripts installed"
}

install_systemd_service() {
	print_info "Installing systemd service..."
	
	# Install service file
	install -m 644 "packaging/systemd/aie_daemon.service" \
		"/etc/systemd/system/" || {
		print_error "Failed to install systemd service"
		return 1
	}
	
	# Reload systemd daemon
	systemctl daemon-reload
	
	print_info "Systemd service installed"
}

install_headers() {
	print_info "Installing public headers..."
	
	mkdir -p "${INSTALL_PREFIX}/include/aie-os"
	
	install -m 644 "kernel/include/ai_sched.h" \
		"${INSTALL_PREFIX}/include/aie-os/" || {
		print_error "Failed to install headers"
		return 1
	}
	
	print_info "Headers installed"
}

install_config() {
	print_info "Installing configuration..."
	
	mkdir -p "/etc/aie-os"
	
	install -m 644 "config/aie.conf" \
		"/etc/aie-os/" || {
		print_error "Failed to install config"
		return 1
	}
	
	mkdir -p "${INSTALL_PREFIX}/share/aie-os"
	
	print_info "Configuration installed"
}

install_model_helper() {
	print_info "Installing model installation helper..."
	
	install -m 755 "packaging/install_model.sh" \
		"${INSTALL_PREFIX}/bin/aie_install_model" || {
		print_error "Failed to install model helper"
		return 1
	}
	
	print_info "Model helper installed"
}

configure_permissions() {
	print_info "Configuring permissions..."
	
	# Ensure proper permissions for eBPF operations
	# Note: ideally daemon runs with CAP_BPF + CAP_PERFMON capabilities
	# (handled via systemd service file)
	
	print_info "Permissions configured"
}

start_service() {
	print_info "Starting aie_daemon service..."
	
	systemctl enable aie_daemon
	systemctl start aie_daemon
	
	# Check that service started
	sleep 1
	if systemctl is-active --quiet aie_daemon; then
		print_info "Service started successfully"
		return 0
	else
		print_error "Service failed to start"
		systemctl status aie_daemon
		return 1
	fi
}

usage() {
	cat <<EOF
Usage: $0 [OPTIONS]

Install AIE-OS AI Scheduler subsystem.

OPTIONS:
  --prefix PATH         Installation prefix (default: /usr/local)
  --build-dir PATH      Build directory (default: .)
  --skip-build          Skip compilation step
  --skip-service        Skip systemd service setup
  --verbose             Enable verbose output
  -h, --help            Show this help message

REQUIREMENTS:
  - Linux kernel 6.13+ with sched_ext support
  - Ubuntu 24.04+ (or similar)
  - Root privileges (sudo)

EXAMPLES:
  # Standard installation
  sudo ./install.sh

  # Custom prefix
  sudo ./install.sh --prefix=/opt/aie-os

  # Skip rebuild if already compiled
  sudo ./install.sh --skip-build

EOF
}

# ======================== Main ========================

main() {
	# Parse arguments
	while [[ $# -gt 0 ]]; do
		case "$1" in
			--prefix)
				INSTALL_PREFIX="$2"
				shift 2
				;;
			--build-dir)
				BUILD_DIR="$2"
				shift 2
				;;
			--skip-build)
				SKIP_BUILD=1
				shift
				;;
			--skip-service)
				SKIP_SERVICE=1
				shift
				;;
			--verbose)
				VERBOSE=1
				shift
				;;
			-h|--help)
				usage
				exit 0
				;;
			*)
				print_error "Unknown option: $1"
				usage
				exit 1
				;;
		esac
	done
	
	print_info "Starting AIE-OS Scheduler Installation"
	print_info "Installation prefix: ${INSTALL_PREFIX}"
	
	# Verify preconditions
	check_root
	check_dependencies
	check_kernel_support
	
	# Build if not skipped
	if [ -z "$SKIP_BUILD" ]; then
		build_artifacts
	fi
	
	# Install components
	install_headers
	install_config
	install_bpf_scheduler
	install_daemon
	install_tools
	install_workload_scripts
	install_verification_script
	install_model_helper
	
	if [ -z "$SKIP_SERVICE" ]; then
		install_systemd_service
		configure_permissions
		
		# Optionally start service
		read -p "Start aie_daemon now? [y/N] " -n 1 -r
		echo
		if [[ $REPLY =~ ^[Yy]$ ]]; then
			start_service
		fi
	fi
	
	print_info "Installation complete!"
	print_info "AIE-OS Scheduler is ready to use"
	print_info ""
	print_info "Next steps:"
	print_info "  1. Verify installation: ${INSTALL_PREFIX}/bin/aie_verify"
	print_info "  2. Check service status: systemctl status aie_daemon"
	print_info "  3. Monitor scheduler: ${INSTALL_PREFIX}/bin/aie_top"
	print_info "  4. Run AI demo: ${INSTALL_PREFIX}/bin/aie_demo pytorch"
	print_info "  5. View logs: journalctl -u aie_daemon -f"
}

main "$@"

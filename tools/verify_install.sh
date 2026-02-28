#!/bin/bash
#
# AIE-OS Installation Verification Script
#
# Checks that AIE-OS scheduler is installed and operational.
# Useful for validating deployment on target systems.
#
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0

check_pass() {
    echo -e "${GREEN}[PASS]${NC} $*"
    ((PASS_COUNT++))
}

check_fail() {
    echo -e "${RED}[FAIL]${NC} $*"
    ((FAIL_COUNT++))
}

check_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

print_header() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║  AIE-OS Installation Verification                 ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""
}

check_kernel() {
    echo "═══ Kernel Configuration ═══"
    
    if grep -q "CONFIG_SCHED_CLASS_EXT" /boot/config-$(uname -r) 2>/dev/null; then
        check_pass "sched_ext support detected in kernel"
    else
        check_fail "sched_ext not enabled in kernel"
        return
    fi
}

check_daemon() {
    echo ""
    echo "═══ Daemon Service ═══"
    
    if systemctl list-unit-files | grep -q aie_daemon.service; then
        check_pass "aie_daemon.service installed"
        
        if systemctl is-active --quiet aie_daemon; then
            check_pass "aie_daemon is running"
        else
            check_warn "aie_daemon installed but not running (use: sudo systemctl start aie_daemon)"
        fi
    else
        check_fail "aie_daemon.service not found"
    fi
}

check_binaries() {
    echo ""
    echo "═══ Installed Binaries ═══"
    
    local prefix="${1:-/usr/local}"
    
    if [ -f "$prefix/bin/aie_daemon" ]; then
        check_pass "aie_daemon binary found at $prefix/bin/aie_daemon"
    else
        check_fail "aie_daemon binary not found"
    fi
    
    if [ -f "$prefix/bin/aie_top" ]; then
        check_pass "aie_top tool found at $prefix/bin/aie_top"
    else
        check_fail "aie_top tool not found"
    fi
}

check_config() {
    echo ""
    echo "═══ Configuration ═══"
    
    if [ -f /etc/aie-os/aie.conf ]; then
        check_pass "Configuration file found at /etc/aie-os/aie.conf"
        
        local classifier=$(grep "^classifier=" /etc/aie-os/aie.conf | cut -d= -f2)
        if [ -n "$classifier" ]; then
            check_pass "Classifier set to: $classifier"
        fi
        
        local model_path=$(grep "^model_path=" /etc/aie-os/aie.conf | cut -d= -f2)
        if [ -n "$model_path" ]; then
            if [ -f "$model_path" ]; then
                check_pass "ONNX model found at: $model_path"
            else
                check_warn "ONNX model not found at: $model_path (using heuristic fallback)"
            fi
        fi
    else
        check_fail "Configuration not found at /etc/aie-os/aie.conf"
    fi
}

check_ebpf_objects() {
    echo ""
    echo "═══ eBPF Scheduler ═══"
    
    local prefix="${1:-/usr/local}"
    
    if [ -f "$prefix/lib/aie-os/ai_sched.bpf.o" ]; then
        check_pass "ai_sched.bpf.o found"
    else
        check_fail "ai_sched.bpf.o not found"
    fi
    
    if [ -f "$prefix/lib/aie-os/telemetry.bpf.o" ]; then
        check_pass "telemetry.bpf.o found"
    else
        check_fail "telemetry.bpf.o not found"
    fi
}

check_demo_workloads() {
    echo ""
    echo "═══ Demo Workloads ═══"
    
    if [ -f "$(dirname $0)/run_ai_demo.sh" ]; then
        check_pass "run_ai_demo.sh launcher found"
    else
        check_warn "run_ai_demo.sh not found in tools directory"
    fi
    
    if command -v python3 &>/dev/null; then
        if command -v pip3 &>/dev/null; then
            if pip3 show torch &>/dev/null; then
                check_pass "PyTorch is installed"
            else
                check_warn "PyTorch not installed (optional for pytorch_inference.py)"
            fi
        fi
    fi
}

check_dependencies() {
    echo ""
    echo "═══ Dependencies ═══"
    
    if pkg-config --exists libbpf; then
        check_pass "libbpf installed"
    else
        check_warn "libbpf not found"
    fi
    
    if pkg-config --exists onnxruntime; then
        check_pass "ONNX Runtime installed"
    else
        check_warn "ONNX Runtime not installed (optional, uses heuristic fallback)"
    fi
}

check_permissions() {
    echo ""
    echo "═══ Capabilities & Permissions ═══"
    
    if [ -S /var/run/systemd/notify ]; then
        check_pass "systemd socket accessible"
    fi
    
    if [ -r /proc/self/cgroup ]; then
        check_pass "cgroup interface readable"
    fi
}

print_summary() {
    echo ""
    echo "╔════════════════════════════════════════════════════╗"
    echo "║  Summary                                           ║"
    echo "╚════════════════════════════════════════════════════╝"
    echo ""
    echo -e "${GREEN}Passed:${NC}  $PASS_COUNT"
    echo -e "${RED}Failed:${NC}  $FAIL_COUNT"
    echo ""
    
    if [ $FAIL_COUNT -eq 0 ]; then
        echo -e "${GREEN}✓ All critical checks passed!${NC}"
        echo ""
        echo "Next steps:"
        echo "  1. Start daemon: sudo systemctl start aie_daemon"
        echo "  2. Monitor: aie_top"
        echo "  3. Run demo: $(dirname $0)/run_ai_demo.sh pytorch"
        return 0
    else
        echo -e "${RED}✗ Some checks failed. See above for details.${NC}"
        echo ""
        echo "For help, check /etc/aie-os/aie.conf and systemctl status aie_daemon"
        return 1
    fi
}

main() {
    print_header
    
    local install_prefix="${1:-/usr/local}"
    
    check_kernel
    check_daemon
    check_binaries "$install_prefix"
    check_config
    check_ebpf_objects "$install_prefix"
    check_demo_workloads
    check_dependencies
    check_permissions
    
    print_summary
}

main "$@"

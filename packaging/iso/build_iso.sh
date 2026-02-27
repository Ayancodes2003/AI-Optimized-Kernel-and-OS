#!/bin/bash
#
# AIE-OS ISO Build Script
#
# Creates a bootable Ubuntu-based ISO with AI scheduler installed.
# Requires: debootstrap, grub-mkimage, xorriso (or mkisofs)
#
# Usage:
#   sudo ./packaging/iso/build_iso.sh [OPTIONS]
#
# With pre-built kernel:
#   sudo ./packaging/iso/build_iso.sh --kernel /path/to/vmlinuz --initrd /path/to/initrd.img
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_info() {
    echo -e "${GREEN}[ISO Build]${NC} $*"
}

print_error() {
    echo -e "${RED}[ISO Build]${NC} ERROR: $*" >&2
}

print_warn() {
    echo -e "${YELLOW}[ISO Build]${NC} Warning: $*"
}

# Configuration
WORK_DIR="/tmp/aie-os-iso-$(date +%s)"
ISO_OUTPUT="${ISO_OUTPUT:-./aie-os.iso}"
UBUNTU_RELEASE="${UBUNTU_RELEASE:-jammy}"  # Ubuntu 22.04 LTS
INSTALL_PREFIX="/usr/local"
KERNEL_PATH="${KERNEL_PATH:-}"
INITRD_PATH="${INITRD_PATH:-}"

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Build bootable AIE-OS ISO.

OPTIONS:
  --kernel PATH      Path to Linux kernel vmlinuz (if not provided, uses system kernel)
  --initrd PATH      Path to initramfs/initrd.img
  --release CODENAME Ubuntu release (default: jammy)
  --output PATH      Output ISO path (default: ./aie-os.iso)
  --work-dir PATH    Work directory (default: /tmp/aie-os-iso-*)
  --help             Show this help

REQUIREMENTS:
  - Root privileges (sudo)
  - debootstrap
  - grub-mkimage
  - xorriso or mkisofs
  - Internet connection (for debootstrap)

EXAMPLE:
  sudo ./build_iso.sh \\
    --kernel ~/sched_ext_kernel/vmlinuz-6.13 \\
    --initrd ~/sched_ext_kernel/initrd.img \\
    --output ~/aie-os-6.13.iso

EOF
}

check_root() {
    if [[ $EUID -ne 0 ]]; then
        print_error "This script must be run as root (use sudo)"
        exit 1
    fi
}

check_tools() {
    print_info "Checking required tools..."
    
    local missing=0
    for cmd in debootstrap grub-mkimage xorriso; do
        if ! command -v $cmd &>/dev/null; then
            if [ "$cmd" = "xorriso" ] && command -v mkisofs &>/dev/null; then
                continue
            fi
            print_error "$cmd not found"
            missing=1
        fi
    done
    
    if [ $missing -eq 1 ]; then
        print_error "Install missing tools and try again"
        exit 1
    fi
    
    print_info "All tools found"
}

setup_root() {
    print_info "Setting up chroot environment at $WORK_DIR..."
    mkdir -p "$WORK_DIR"
    
    # Debootstrap minimal Ubuntu
    debootstrap --variant=minbase "$UBUNTU_RELEASE" "$WORK_DIR/chroot" &>/dev/null || {
        print_error "debootstrap failed"
        return 1
    }
    
    print_info "Chroot environment created"
}

install_aie_components() {
    print_info "Installing AIE-OS components into chroot..."
    
    local chroot="$WORK_DIR/chroot"
    
    # Copy built binaries and objects
    mkdir -p "$chroot/usr/local/bin"
    mkdir -p "$chroot/usr/local/lib/aie-os"
    mkdir -p "$chroot/etc/aie-os"
    mkdir -p "$chroot/etc/systemd/system"
    
    # Copy daemon binary (if built)
    if [ -f "build/output/aie_daemon" ]; then
        cp build/output/aie_daemon "$chroot/usr/local/bin/" || {
            print_warn "aie_daemon binary not found; will need to build in chroot"
        }
    fi
    
    # Copy tools
    if [ -f "build/output/aie_top" ]; then
        cp build/output/aie_top "$chroot/usr/local/bin/"
    fi
    
    if [ -f "build/output/matrix_compute" ]; then
        cp build/output/matrix_compute "$chroot/usr/local/bin/"
    fi
    
    # Copy eBPF objects
    if [ -f "build/output/ai_sched.bpf.o" ]; then
        cp build/output/ai_sched.bpf.o "$chroot/usr/local/lib/aie-os/"
    fi
    
    if [ -f "build/output/telemetry.bpf.o" ]; then
        cp build/output/telemetry.bpf.o "$chroot/usr/local/lib/aie-os/"
    fi
    
    # Copy configuration
    cp config/aie.conf "$chroot/etc/aie-os/" || {
        mkdir -p "$chroot/etc/aie-os"
        cat > "$chroot/etc/aie-os/aie.conf" <<'CONF'
model_path=/usr/local/share/aie-os/model.onnx
classifier=heuristic
CONF
    }
    
    # Copy systemd service
    if [ -f "packaging/systemd/aie_daemon.service" ]; then
        cp packaging/systemd/aie_daemon.service "$chroot/etc/systemd/system/"
    fi
    
    print_info "AIE-OS components installed"
}

install_dependencies() {
    print_info "Installing runtime dependencies in chroot..."
    
    local chroot="$WORK_DIR/chroot"
    
    # Minimal packages needed at boot
    chroot "$chroot" apt-get update &>/dev/null
    chroot "$chroot" apt-get install -y \
        linux-image-generic \
        grub-pc \
        systemd \
        openssh-server \
        vim \
        net-tools \
        curl \
        bc \
        &>/dev/null || {
        print_warn "Some packages failed to install"
    }
    
    print_info "Dependencies installed"
}

setup_boot() {
    print_info "Configuring boot system..."
    
    local chroot="$WORK_DIR/chroot"
    
    # Create GRUB configuration
    mkdir -p "$chroot/boot/grub"
    cat > "$chroot/boot/grub/grub.cfg" <<'GRUB'
set default=0
set timeout=5

menuentry 'AIE-OS (AI Scheduler Enabled)' {
    insmod gzio
    insmod part_msdos
    insmod ext2
    search --no-floppy --label aie-os --set root
    linux   /boot/vmlinuz-* root=LABEL=aie-os ro quiet
    initrd  /boot/initrd.img-*
}

menuentry 'AIE-OS (Recovery Mode)' {
    insmod gzio
    insmod part_msdos
    insmod ext2
    search --no-floppy --label aie-os --set root
    linux   /boot/vmlinuz-* root=LABEL=aie-os ro single
    initrd  /boot/initrd.img-*
}
GRUB
    
    # Enable autostart of aie_daemon
    chroot "$chroot" systemctl enable aie_daemon.service 2>/dev/null || {
        print_warn "Could not enable aie_daemon.service"
    }
    
    print_info "Boot system configured"
}

create_iso_layout() {
    print_info "Creating ISO layout..."
    
    local iso_tmp="$WORK_DIR/iso_layout"
    mkdir -p "$iso_tmp/boot/grub"
    
    # Copy chroot to ISO layout
    cp -r "$WORK_DIR/chroot"/* "$iso_tmp/" 2>/dev/null || {
        print_error "Failed to copy chroot to ISO layout"
        return 1
    }
    
    # Create GRUB boot image
    grub-mkimage -O i386-pc -o "$iso_tmp/boot/grub/core.img" \
        biosdisk part_msdos ext2 &>/dev/null || {
        print_warn "GRUB boot image creation may have partial success"
    }
    
    print_info "ISO layout created"
}

build_iso() {
    print_info "Building ISO image..."
    
    local iso_tmp="$WORK_DIR/iso_layout"
    
    # Use xorriso if available, otherwise mkisofs
    if command -v xorriso &>/dev/null; then
        xorriso -as mkisofs -o "$ISO_OUTPUT" \
            -b boot/grub/core.img \
            -no-emul-boot \
            -boot-load-size 4 \
            -boot-info-table \
            -J -R -V "aie-os" \
            "$iso_tmp" &>/dev/null || {
            print_error "xorriso failed"
            return 1
        }
    else
        mkisofs -o "$ISO_OUTPUT" \
            -b boot/grub/core.img \
            -no-emul-boot \
            -boot-load-size 4 \
            -boot-info-table \
            -J -R -V "aie-os" \
            "$iso_tmp" &>/dev/null || {
            print_error "mkisofs failed"
            return 1
        }
    fi
    
    print_info "ISO image created: $ISO_OUTPUT"
    ls -lh "$ISO_OUTPUT"
}

cleanup() {
    print_info "Cleaning up temporary files..."
    if [ -d "$WORK_DIR" ]; then
        rm -rf "$WORK_DIR" || print_warn "Could not fully clean $WORK_DIR"
    fi
}

main() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --kernel) KERNEL_PATH="$2"; shift 2 ;;
            --initrd) INITRD_PATH="$2"; shift 2 ;;
            --output) ISO_OUTPUT="$2"; shift 2 ;;
            --release) UBUNTU_RELEASE="$2"; shift 2 ;;
            --work-dir) WORK_DIR="$2"; shift 2 ;;
            --help) usage; exit 0 ;;
            *) print_error "Unknown option: $1"; usage; exit 1 ;;
        esac
    done
    
    check_root
    check_tools
    
    trap cleanup EXIT
    
    setup_root
    install_aie_components
    install_dependencies
    setup_boot
    create_iso_layout
    build_iso
    
    print_info "ISO build complete!"
    print_info "Output: $ISO_OUTPUT"
    echo ""
    echo "Next steps:"
    echo "  1. Boot ISO on target machine: qemu-system-x86_64 -cdrom $ISO_OUTPUT"
    echo "  2. Or burn to USB: sudo dd if=$ISO_OUTPUT of=/dev/sdX bs=4M"
    echo "  3. On boot, verify: sudo $(dirname $0)/../verify_install.sh"
}

main "$@"

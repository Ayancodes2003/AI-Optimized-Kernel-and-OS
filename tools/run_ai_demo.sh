#!/bin/bash
#
# AIE-OS AI Workload Demo Launcher
#
# Demonstration script that runs various AI workloads to trigger scheduler classification.
# Observe with: aie_top (in another terminal)
#
set -e

WORKLOAD_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/workloads" && pwd)"

print_info() {
    echo "[aie_demo] $*"
}

print_error() {
    echo "[aie_demo] ERROR: $*" >&2
}

usage() {
    cat <<EOF
Usage: $0 [WORKLOAD] [OPTIONS]

Run AI workloads for scheduler demonstration.

WORKLOADS:
  pytorch   - PyTorch inference (requires torch installed)
  onnx      - ONNX model inference
  matrix    - CPU-intensive matrix computation
  all       - Run all workloads sequentially

OPTIONS:
  --help           Show this help
  --batch-size N   Batch size for pytorch (default: 8)
  --batches N      Number of batches (default: 10)
  --rounds N       Rounds for matrix workload (default: 5)

EXAMPLES:
  $0 pytorch            # PyTorch workload
  $0 matrix --rounds 10 # Extended matrix computation
  $0 all                # Run all workloads

OBSERVATION:
  Open another terminal and run: aie_top
  You should see task classification change as workloads run.

EOF
}

run_pytorch() {
    print_info "Starting PyTorch inference workload..."
    local batch_size=8
    local batches=10
    
    # Parse pytorch-specific args
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --batch-size) batch_size="$2"; shift 2 ;;
            --batches) batches="$2"; shift 2 ;;
            *) shift ;;
        esac
    done
    
    if ! command -v python3 &>/dev/null; then
        print_error "python3 not found"
        return 1
    fi
    
    python3 "$WORKLOAD_DIR/pytorch_inference.py" \
        --batch-size "$batch_size" \
        --batches "$batches"
}

run_onnx() {
    print_info "Starting ONNX inference workload..."
    local inferences=10
    
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --inferences) inferences="$2"; shift 2 ;;
            *) shift ;;
        esac
    done
    
    if ! command -v python3 &>/dev/null; then
        print_error "python3 not found"
        return 1
    fi
    
    python3 "$WORKLOAD_DIR/onnx_inference.py" --inferences "$inferences"
}

run_matrix() {
    print_info "Starting matrix compute workload..."
    local rounds=5
    local size=256
    
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --rounds) rounds="$2"; shift 2 ;;
            --size) size="$2"; shift 2 ;;
            *) shift ;;
        esac
    done
    
    local binary="${WORKLOAD_DIR%/*}/matrix_compute"
    if [ ! -f "$binary" ]; then
        print_error "matrix_compute binary not found at $binary"
        print_info "Build with: make -C $(dirname $0)/../.. tools"
        return 1
    fi
    
    "$binary" --size "$size" --rounds "$rounds"
}

main() {
    if [[ $# -eq 0 ]]; then
        usage
        exit 0
    fi
    
    local workload="$1"
    shift
    
    case "$workload" in
        pytorch)
            run_pytorch "$@"
            ;;
        onnx)
            run_onnx "$@"
            ;;
        matrix)
            run_matrix "$@"
            ;;
        all)
            print_info "Running all demonstration workloads..."
            print_info ""
            
            if [ -f "$WORKLOAD_DIR/matrix_compute" ]; then
                run_matrix --rounds 3 || true
                sleep 2
            fi
            
            if command -v python3 &>/dev/null; then
                run_onnx --inferences 5 || true
                sleep 2
            fi
            
            print_info "All workloads completed"
            ;;
        -h|--help|help)
            usage
            exit 0
            ;;
        *)
            print_error "Unknown workload: $workload"
            usage
            exit 1
            ;;
    esac
}

main "$@"

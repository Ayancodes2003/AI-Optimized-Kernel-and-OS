#!/usr/bin/env python3
"""
AIE-OS AI Workload Demo: ONNX Inference
Demonstrates ONNX model inference (compatible with AIE-OS classifier).
"""

import sys
import numpy as np
import argparse
import time
import os

try:
    import onnxruntime as ort
except ImportError:
    print("[onnx_inference] Error: onnxruntime not installed", file=sys.stderr)
    print("[onnx_inference] Install with: pip install onnxruntime", file=sys.stderr)
    sys.exit(1)

def create_dummy_model(path):
    """Create a simple ONNX model for demo if not present"""
    try:
        import onnx
        import onnx.helper as helper
        
        # Build a simple graph: input -> matmul -> relu -> output
        X = helper.make_tensor_value_info('X', onnx.TensorProto.FLOAT, [1, 3, 224, 224])
        Y = helper.make_tensor_value_info('Y', onnx.TensorProto.FLOAT, [1, 1000])
        
        # Weights for matmul (flattened input [150528] to output [1000])
        W = helper.make_tensor(
            'W', onnx.TensorProto.FLOAT,
            [150528, 1000],
            np.random.randn(150528, 1000).astype(np.float32).tobytes(),
            raw=True
        )
        
        # Create simple squash (reshape + linear projection)
        graph = helper.make_graph(
            [
                helper.make_node('Reshape', inputs=['X'], outputs=['X_flat'],
                                 value=helper.make_tensor('shape_const', onnx.TensorProto.INT64, [1], [1, 150528])),
                helper.make_node('MatMul', inputs=['X_flat', 'W'], outputs=['Y']),
            ],
            'DummyModel',
            [X],
            [Y],
            [W]
        )
        
        model = helper.make_model(graph, opset_imports=[helper.make_opsetid('', 11)])
        onnx.checker.check_model(model)
        onnx.save(model, path)
        print(f"[onnx_inference] Created dummy model at {path}")
    except:
        pass

def run_inference(model_path, num_inferences=10):
    """Run ONNX inference repeatedly"""
    if not os.path.exists(model_path):
        print(f"[onnx_inference] Model not found at {model_path}")
        print(f"[onnx_inference] Falling back to CPU-heavy computation demo")
        dummy_inference(num_inferences)
        return
    
    print(f"[onnx_inference] Loading model from {model_path}")
    sess = ort.InferenceSession(model_path)
    
    print(f"[onnx_inference] Starting inference")
    print(f"[onnx_inference] PID: {os.getpid()}")
    
    input_name = sess.get_inputs()[0].name
    output_name = sess.get_outputs()[0].name
    
    for i in range(num_inferences):
        x = np.random.randn(1, 3, 224, 224).astype(np.float32)
        output = sess.run([output_name], {input_name: x})
        print(f"[onnx_inference] Inference {i+1}/{num_inferences} - "
              f"output shape: {output[0].shape}, mean: {output[0].mean():.4f}")
        time.sleep(0.1)

def dummy_inference(num_rounds=10):
    """CPU-heavy computation when model unavailable"""
    print(f"[onnx_inference] Running CPU-heavy computation demo")
    print(f"[onnx_inference] PID: {os.getpid()}")
    
    for i in range(num_rounds):
        # Large matrix multiplication
        A = np.random.randn(512, 512).astype(np.float32)
        B = np.random.randn(512, 512).astype(np.float32)
        C = np.matmul(A, B)
        print(f"[onnx_inference] Round {i+1}/{num_rounds} - "
              f"matrix shape: {C.shape}, mean: {C.mean():.4f}")
        time.sleep(0.1)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='ONNX inference workload for AIE-OS demo')
    parser.add_argument('--model', type=str, default='/tmp/aie_demo.onnx', 
                        help='Path to ONNX model')
    parser.add_argument('--inferences', type=int, default=10, help='Number of inferences')
    args = parser.parse_args()
    
    try:
        run_inference(args.model, args.inferences)
    except Exception as e:
        print(f"[onnx_inference] Error: {e}", file=sys.stderr)
        sys.exit(1)

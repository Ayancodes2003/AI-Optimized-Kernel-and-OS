#!/usr/bin/env python3
"""
AIE-OS AI Workload Demo: PyTorch Inference
Demonstrates real neural network inference to trigger AI task classification.
"""

import sys
import torch
import torch.nn as nn
import argparse
import time

class SimpleNet(nn.Module):
    """Small CNN for demo purposes"""
    def __init__(self):
        super(SimpleNet, self).__init__()
        self.conv1 = nn.Conv2d(3, 32, 3, padding=1)
        self.conv2 = nn.Conv2d(32, 64, 3, padding=1)
        self.pool = nn.MaxPool2d(2, 2)
        self.fc1 = nn.Linear(64 * 56 * 56, 128)
        self.fc2 = nn.Linear(128, 10)
        self.relu = nn.ReLU()

    def forward(self, x):
        x = self.pool(self.relu(self.conv1(x)))
        x = self.pool(self.relu(self.conv2(x)))
        x = x.view(x.size(0), -1)
        x = self.relu(self.fc1(x))
        x = self.fc2(x)
        return x

def run_inference(batch_size=8, num_batches=10, device='cpu'):
    """Run repeated inference to show sustained AI workload"""
    model = SimpleNet().to(device).eval()
    
    print(f"[pytorch_inference] Starting inference on {device}")
    print(f"[pytorch_inference] Batch size: {batch_size}, Num batches: {num_batches}")
    print(f"[pytorch_inference] PID: {os.getpid()}")
    
    with torch.no_grad():
        for batch_idx in range(num_batches):
            x = torch.randn(batch_size, 3, 224, 224, device=device)
            output = model(x)
            print(f"[pytorch_inference] Batch {batch_idx+1}/{num_batches} - "
                  f"output shape: {output.shape}, mean: {output.mean():.4f}")
            time.sleep(0.1)  # brief pause between batches

    print(f"[pytorch_inference] Inference complete")

if __name__ == '__main__':
    import os
    parser = argparse.ArgumentParser(description='PyTorch inference workload for AIE-OS demo')
    parser.add_argument('--batch-size', type=int, default=8, help='Batch size')
    parser.add_argument('--batches', type=int, default=10, help='Number of batches')
    parser.add_argument('--device', type=str, default='cpu', help='Device (cpu or cuda)')
    args = parser.parse_args()
    
    try:
        run_inference(batch_size=args.batch_size, num_batches=args.batches, device=args.device)
    except Exception as e:
        print(f"[pytorch_inference] Error: {e}", file=sys.stderr)
        sys.exit(1)

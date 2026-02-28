#!/bin/bash
echo "Starting AI workload simulation..."
pkill -f "aie_sim_" 2>/dev/null
sleep 0.5

echo "[1] REALTIME_AI tasks (high priority, high syscall rate)..."
for i in 1 2 3; do
    nice -n -15 bash -c 'while true; do dd if=/dev/urandom bs=1 count=1 2>/dev/null > /dev/null; sleep 0.001; done' &
    echo "   PID $!"
done

echo "[2] INTERACTIVE_AI tasks (high syscall rate)..."
for i in 1 2 3; do
    bash -c 'while true; do for j in $(seq 1 200); do ls /proc/$$ > /dev/null 2>&1; done; sleep 0.01; done' &
    echo "   PID $!"
done

echo "[3] BATCH_AI task (multi-threaded + large memory)..."
python3 -c "
import threading, time, os, array
data = array.array('d', range(14000000))
def worker():
    x = 0.0
    for i in range(10000000):
        x += (i * 3.14159) / (i + 1)
threads = [threading.Thread(target=worker, daemon=True) for _ in range(6)]
print(f'   BATCH_AI PID: {os.getpid()}, threads: 6, memory: ~112MB')
[t.start() for t in threads]
while True: time.sleep(1)
" &
echo "   PID $!"

echo "[4] BACKGROUND tasks (low priority)..."
for i in 1 2 3; do
    nice -n 19 bash -c 'while true; do sleep 2; done' &
    echo "   PID $!"
done

echo ""
echo "All workloads running! Open a new terminal and run:"
echo "  sudo ./build/output/aie_top"
echo ""
echo "Press Ctrl+C to stop all workloads..."
trap 'echo Stopping...; kill $(jobs -p) 2>/dev/null; exit' INT
wait

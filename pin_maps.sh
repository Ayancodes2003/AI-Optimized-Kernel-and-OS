#!/bin/bash
BPF_FS="/sys/fs/bpf"
mount | grep -q "$BPF_FS" || mount -t bpf bpf "$BPF_FS"
STATS_ID=$(bpftool map list 2>/dev/null | grep "sched_stats" | awk '{print $1}' | tr -d ':')
ENERGY_ID=$(bpftool map list 2>/dev/null | grep "energy_mode" | awk '{print $1}' | tr -d ':')
if [ -z "$STATS_ID" ]; then echo "[ERROR] sched_stats map not found. Is scheduler loaded?"; exit 1; fi
[ -f "$BPF_FS/aie_sched_stats" ] && rm -f "$BPF_FS/aie_sched_stats"
[ -f "$BPF_FS/aie_energy_mode" ] && rm -f "$BPF_FS/aie_energy_mode"
bpftool map pin id $STATS_ID "$BPF_FS/aie_sched_stats" && echo "[OK] Pinned sched_stats"
[ -n "$ENERGY_ID" ] && bpftool map pin id $ENERGY_ID "$BPF_FS/aie_energy_mode" && echo "[OK] Pinned energy_mode"
echo "[DONE] Now run: ./build/output/aie_top"

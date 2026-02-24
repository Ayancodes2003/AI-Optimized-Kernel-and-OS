# AIE-OS Quick Start Guide

**Get up and running in 5 minutes**

---

## 1️⃣ Verify Your Setup (2 minutes)

```bash
# Check if you have sched_ext kernel (Linux 6.13+)
uname -r
grep CONFIG_SCHED_CLASS_EXT /boot/config-$(uname -r)
# Expected: CONFIG_SCHED_CLASS_EXT=y
```

**Don't have sched_ext?** Install on Ubuntu 24.04:
```bash
sudo add-apt-repository ppa:arighi/sched-ext-unstable
sudo apt update && sudo apt install linux-image-unsigned-generic-hwe-24.04
sudo reboot
```

## 2️⃣ Install Build Dependencies (1 minute)

```bash
sudo apt install -y \
    clang llvm \
    g++ make \
    linux-headers-$(uname -r) \
    linux-tools-$(uname -r) \
    libbpf-dev libelf-dev libz-dev
```

## 3️⃣ Build & Install (2 minutes)

```bash
cd AI-Optimized-Kernel-and-OS

# Verify environment
make check
# Should show: ✓ for all items

# Build everything
make

# Install to system (requires sudo)
sudo make install

# Start scheduler
sudo systemctl start aie_daemon
```

## 4️⃣ Watch It Work

```bash
# Real-time dashboard
aie_top

# Follow logs
journalctl -u aie_daemon -f
```

---

## 📚 Next: Learn More

| Want to... | Read... | Time |
|----------|---------|------|
| Understand architecture | [PROJECT.md](PROJECT.md) | 20 min |
| Modify code | [DEVELOPMENT.md](DEVELOPMENT.md) | 30 min |
| Debug issues | [PROJECT.md](PROJECT.md#troubleshooting) | 10 min |
| Study eBPF | [kernel/README.md](kernel/README.md) | 30 min |

---

## ⚡ Common Commands

```bash
# Check status
systemctl status aie_daemon

# View logs
journalctl -u aie_daemon -n 100        # Last 100 lines
journalctl -u aie_daemon -f            # Follow in real-time

# Rebuild
make clean && make

# Reinstall
sudo make install-fast

# Uninstall (stop service, remove files)
sudo systemctl stop aie_daemon
sudo systemctl disable aie_daemon
sudo rm -f /usr/local/bin/aie_daemon /usr/local/bin/aie_top
```

---

## 🐛 Troubleshooting

### "sched_ext NOT enabled"
→ Install sched_ext kernel (see step 1)

### "Permission denied" on install
→ Use `sudo make install`

### Daemon won't start
→ Check logs: `journalctl -u aie_daemon -n 50`

### High CPU usage
→ Normal for 1-3%. Check with: `systemctl status aie_daemon`

---

## 🎯 What's Working Now

✅ **Kernel Scheduler:** Task classification & routing  
✅ **Daemon:** Real-time telemetry processing  
✅ **Classification:** Smart heuristic-based categorization  
✅ **Dashboard:** Live monitoring with aie_top  
✅ **Installation:** Automated setup & systemd integration  

---

## 🚀 What's Next

Phase 2 (In progress):
- [ ] ONNX Runtime for ML models
- [ ] GPU/NPU routing
- [ ] Advanced energy policies

---

## 💡 How It Works (TL;DR)

```
Your Apps (unchanged)
      ↓
[Kernel eBPF Scheduler]
  • Classify tasks (AI vs non-AI)
  • Route to best CPU/GPU/NPU
  • Stream telemetry
      ↓
[Userspace Daemon]
  • Learn from telemetry
  • Run classifier
  • Push decisions
      ↓
[Smart Routing]
  • Real-time inference → P-cores
  • Batch jobs → NPU/GPU
  • Background → E-cores
```

---

## 📞 Need Help?

1. **Check docs:** [FAQ in DEVELOPMENT.md](DEVELOPMENT.md#faq)
2. **View source:** Code is well-commented
3. **Run tests:** `make check` to verify setup

---

**Ready to dive deeper?** → [Read PROJECT.md](PROJECT.md)

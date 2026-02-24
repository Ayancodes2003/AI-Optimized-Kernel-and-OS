# AIE-OS Documentation Index

**Complete guide to all project documentation**

---

## 🎯 Start Here

### 📖 README.md
**5-minute overview for everyone**
- What is AIE-OS?
- Key features (task classification, routing, energy policies)
- Quick start (prerequisites, build, install)
- Links to detailed documentation
- **Read time:** 5 minutes  
- **Best for:** First-time users, stakeholders

---

## 🚀 Getting Started

### 📋 QUICKSTART.md
**5-minute hands-on setup**
- Check your kernel (sched_ext support)
- Install dependencies (one command)
- Build & install (three commands)
- Watch it work (aie_top)
- Troubleshooting quick fixes
- **Read time:** 5 minutes  
- **Best for:** Users ready to install now

### 📚 PROJECT.md
**Complete project specification and guide**
- Comprehensive feature list
- Architecture deep-dive (kernel + userspace + IPC)
- Prerequisites and detailed installation
- Usage guide with examples
- Performance characteristics
- Troubleshooting section
- Development roadmap
- References and external resources
- **Read time:** 20-30 minutes  
- **Best for:** Complete understanding before development

---

## 👨‍💻 For Developers

### 📖 DEVELOPMENT.md
**Developer setup, workflow, and coding guide**
- Environment setup (Ubuntu, WSL, Docker)
- Dependency installation
- Code structure walkthrough
  - Kernel layer (eBPF)
  - Userspace layer (daemon)
  - Classification layer
  - Monitoring tools
- How to modify each component
- Testing procedures
- Debugging techniques
  - eBPF debugging
  - Daemon debugging
  - Ring buffer inspection
  - Scheduler verification
- Code standards (C kernel-style, C++17 userspace)
- Common development tasks (add classification, extend telemetry, etc.)
- Performance profiling
- FAQ section
- **Read time:** 30-60 minutes  
- **Best for:** Developers adding features or fixing bugs

### 🔧 kernel/README.md
**In-depth eBPF scheduler technical documentation**
- Scheduler hooks explained (enqueue, dispatch, init)
- Dispatch queue architecture (5 queues for different devices)
- Decision flow diagram (task → classification → routing)
- BPF maps detailed documentation
  - telemetry_ringbuf (kernel→userspace)
  - sched_decisions (userspace→kernel)
  - energy_mode (configuration)
  - time_slices (per-class tuning)
  - sched_stats (statistics)
- Heuristic classification algorithm walkthrough
- Telemetry collection (tracepoints and accumulation)
- Compilation and loading process
- Design decisions and trade-offs
- Performance characteristics
- Debugging with bpftool
- Future enhancements
- **Read time:** 30-45 minutes  
- **Best for:** Kernel/eBPF developers, lower-level understanding

---

## 📊 For Project Management

### 📋 IMPLEMENTATION.md
**Refactoring summary and project achievement**
- Executive summary
- Phase 1 completion checklist
- Code statistics (5,550+ LOC breakdown)
- Architecture highlights
- Key features implemented
- Design principles applied
- Code quality overview
- Performance profile
- Development roadmap
- **Read time:** 10-15 minutes  
- **Best for:** Stakeholders, project reviews

### 📦 DELIVERABLES.md
**Complete list of deliverables with status**
- Project completion summary
- Detailed deliverables checklist by component
- Code breakdown by file and component
- Architecture delivered
- Key features implemented
- Documentation quality metrics
- Code quality assessment
- Deployment readiness
- Knowledge transfer guidance
- Success metrics
- Path forward (Phases 2-4)
- **Read time:** 15-20 minutes  
- **Best for:** Stakeholders, deliverable tracking

### 📄 MANIFEST.md
**File-by-file reference guide**
- Complete file listing with purpose
- Directory structure
- Component responsibilities
- Key interfaces
- Learning path for new developers
- Quick reference table
- **Read time:** 10 minutes  
- **Best for:** Finding where things are, understanding structure

---

## 📚 Reference Documents

### This Document (Documentation Index)
**You are here** - Navigation guide for all docs

---

## 🗺️ Navigation by Use Case

### "I want to understand the project"
1. README.md (5 min) ← You are here conceptually
2. PROJECT.md (20 min)
3. IMPLEMENTATION.md (10 min)

### "I want to install and run it"
1. README.md - Quick Start section (5 min)
2. QUICKSTART.md (5 min)
3. Run `make check && make && sudo make install`

### "I want to modify code"
1. DEVELOPMENT.md (30 min)
2. MANIFEST.md (5 min) - Find what you want to modify
3. Read inline comments in source files
4. kernel/README.md (if modifying eBPF)

### "I want to understand the architecture"
1. PROJECT.md - Architecture section (15 min)
2. kernel/README.md - Scheduler details (30 min)
3. Read source files in order:
   - kernel/ai_sched.bpf.c (main logic)
   - daemon/aie_daemon.cpp (control loop)
   - ai/classifier.cpp (classification)

### "I want to extend the classifier"
1. DEVELOPMENT.md - Code structure section
2. ai/classifier.cpp (review current heuristics)
3. Implement new pattern
4. Test via `make daemon && sudo make install-fast`

### "I want to debug an issue"
1. DEVELOPMENT.md - Debugging section
2. journalctl -u aie_daemon -f (view logs)
3. aie_top (check routing statistics)
4. kernel/README.md - Debugging with bpftool

### "I'm deploying to production"
1. PROJECT.md - Installation section
2. DELIVERABLES.md - Deployment Ready section
3. Run installation script
4. Monitor with aie_top

---

## 📖 Documentation Overview

| Document | Type | Length | Audience | Key Content |
|----------|------|--------|----------|-------------|
| README.md | Overview | 150 lines | Everyone | Vision, features, quick links |
| QUICKSTART.md | Guide | 100 lines | Users | 5-min setup |
| PROJECT.md | Spec | 900 lines | Everyone | Complete specification |
| DEVELOPMENT.md | Guide | 600 lines | Developers | Setup, workflow, debugging |
| kernel/README.md | Technical | 500 lines | Developers | eBPF scheduler details |
| IMPLEMENTATION.md | Summary | 500 lines | Stakeholders | Refactor achievements |
| DELIVERABLES.md | Summary | 600 lines | Stakeholders | Deliverables checklist |
| MANIFEST.md | Reference | 400 lines | Maintainers | File reference |
| This Doc | Index | 200 lines | Everyone | Documentation guide |

**Total:** 2,500+ lines of documentation

---

## 🎯 Quick Questions & Answers

### Q: How do I get started?
→ Start with README.md (5 min), then QUICKSTART.md (5 min)

### Q: Where do I find the kernel scheduler?
→ kernel/ai_sched.bpf.c - also read kernel/README.md for explanation

### Q: Where's the main daemon code?
→ daemon/aie_daemon.cpp - multi-threaded event loop

### Q: How do I add a new task classification?
→ See DEVELOPMENT.md § "Add a New Task Classification"

### Q: How do I debug if something's wrong?
→ See DEVELOPMENT.md § "Debugging" section

### Q: What's the architecture?
→ Read PROJECT.md § "Architecture Deep Dive" + kernel/README.md

### Q: Can I run this on Windows?
→ Development on Windows is fine, testing on Linux/WSL required (sched_ext only on Linux)

### Q: Is this production ready?
→ Yes, Phase 1 complete. See DELIVERABLES.md for status.

### Q: What's the roadmap?
→ See PROJECT.md or DELIVERABLES.md § "Path Forward"

### Q: How do I contribute?
→ See DEVELOPMENT.md § "Code Standards" and "Git Workflow"

---

## 🔗 Cross-References

### From README.md
→ PROJECT.md (full spec)  
→ DEVELOPMENT.md (dev guide)  
→ QUICKSTART.md (setup)

### From QUICKSTART.md
→ README.md (overview)  
→ PROJECT.md (detailed guide)  
→ DEVELOPMENT.md § Troubleshooting (if issues)

### From PROJECT.md
→ kernel/README.md (eBPF details)  
→ DEVELOPMENT.md (build & debug)  
→ DELIVERABLES.md (status)

### From DEVELOPMENT.md
→ kernel/README.md (eBPF specifics)  
→ MANIFEST.md (file locations)  
→ SOURCE CODE (commented inline)

### From kernel/README.md
→ PROJECT.md § Architecture (System overview)  
→ SOURCE (kernel/ai_sched.bpf.c, kernel/telemetry.bpf.c)

---

## 📊 Content Coverage by Topic

### Installation & Setup
- README.md - Quick start section
- QUICKSTART.md - Full guide
- DEVELOPMENT.md - Detailed setup
- PROJECT.md - Prerequisites section

### Architecture & Design
- PROJECT.md - Architecture Deep Dive
- kernel/README.md - eBPF scheduler
- IMPLEMENTATION.md - System overview
- Inline comments in source code

### Feature Specification
- PROJECT.md - Core Product Requirements
- kernel/README.md - Scheduler hooks
- IMPLEMENTATION.md - Features implemented

### Development Workflow
- DEVELOPMENT.md - Complete guide
- Makefile - Build targets (self-documenting)
- MANIFEST.md - File structure

### Debugging & Troubleshooting
- DEVELOPMENT.md - Debugging section
- PROJECT.md - Troubleshooting section
- journalctl (runtime logs)
- aie_top (monitoring tool)

### Deployment
- PROJECT.md - Installation & Usage
- packaging/install.sh - Automated setup
- packaging/systemd/aie_daemon.service - Service config
- DEVELOPEMENT.md - Deployment section

---

## ✅ Documentation Completeness

- [x] User overview (README.md)
- [x] Quick start guide (QUICKSTART.md)
- [x] Complete specification (PROJECT.md)
- [x] Developer guide (DEVELOPMENT.md)
- [x] Kernel technical details (kernel/README.md)
- [x] Project summary (IMPLEMENTATION.md)
- [x] Deliverables (DELIVERABLES.md)
- [x] File reference (MANIFEST.md)
- [x] Documentation index (this file)
- [x] Inline code comments (in source)
- [x] Build system help (Makefile with comments)
- [x] Installation script comments (packaging/install.sh)
- [x] Systemd service comments (packaging/systemd/)

---

## 🎓 Suggested Reading Order

### For System Administrators (30 min)
1. README.md (5 min)
2. QUICKSTART.md (5 min)
3. PROJECT.md § Usage (10 min)
4. PROJECT.md § Troubleshooting (10 min)

### For Developers (2-3 hours)
1. README.md (5 min)
2. DEVELOPMENT.md (45 min)
3. kernel/README.md (45 min)
4. MANIFEST.md (10 min)
5. Review source code (30 min)

### For Researchers (1-2 hours)
1. README.md (5 min)
2. PROJECT.md (30 min)
3. kernel/README.md (30 min)
4. IMPLEMENTATION.md (15 min)
5. Review classifier algorithms (20 min)

### For Stakeholders (45 min)
1. README.md (5 min)
2. IMPLEMENTATION.md (15 min)
3. DELIVERABLES.md (15 min)
4. PROJECT.md § Roadmap (10 min)

---

## 🔄 How Documentation is Maintained

All documentation is:
- ✅ Version controlled in git
- ✅ Kept in sync with code
- ✅ Updated on major changes
- ✅ Cross-referenced for consistency
- ✅ Organized for discoverability

**Update Checklist** (for future changes):
- [ ] Update relevant source file
- [ ] Update relevant documentation
- [ ] Update MANIFEST.md if file structure changes
- [ ] Update links in other docs if sections move
- [ ] Test build and verify no broken references

---

## 📞 Finding Help

### Installation issues?
→ QUICKSTART.md § Troubleshooting

### Understanding the code?
→ DEVELOPMENT.md § Code Structure

### eBPF-specific questions?
→ kernel/README.md

### Debugging problems?
→ DEVELOPMENT.md § Debugging

### Want to contribute?
→ DEVELOPMENT.md § Code Standards + Git Workflow

### Need full specification?
→ PROJECT.md

### Just deployed?
→ PROJECT.md § Usage

---

## 🎉 You're All Set!

This index provides a complete roadmap through all documentation. Pick your starting point above and dive in!

**Recommended:** Start with README.md, then jump to your specific use case above.

---

**Last Updated:** February 24, 2026  
**Status:** Complete documentation set ✅

# Documentation Index

Complete documentation for the Kernel Event Queue IPC project.

## 📚 Documentation Files

### Start Here
- **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - One-page command reference
  - Common commands
  - Event structure
  - Quick troubleshooting

### Main Documentation
- **[PROJECT_DOCUMENTATION.md](PROJECT_DOCUMENTATION.md)** - Comprehensive project documentation
  - Project overview
  - Architecture diagrams
  - How it works (detailed)
  - Demo guide
  - Technical deep dive

### For Teammates
- **[CONSUMER_APP_GUIDE.md](CONSUMER_APP_GUIDE.md)** - User-space app implementation guide
  - Event format specification
  - Template code
  - Build instructions
  - Test scenarios

### Development History
- **[troubleshooting/development-issues.md](troubleshooting/development-issues.md)** - Bug tracker & solutions
  - Issues encountered
  - Root cause analysis
  - Final solutions

## 🚀 Quick Start

```bash
# 1. Build on RPi5
cd ~/os-project && make

# 2. Load module
sudo insmod event_driver.ko timer_enabled=1

# 3. Test
echo "TEST" > /dev/pi5_event
cat /dev/pi5_event
```

## 📋 Project Files

```
OS/
├── event_driver.c          # Kernel module source
├── Makefile                # Build configuration
├── consumer_app.cpp        # User-space consumer template
├── docs/                   # This folder
│   ├── README.md           # This file
│   ├── QUICK_REFERENCE.md
│   ├── PROJECT_DOCUMENTATION.md
│   ├── CONSUMER_APP_GUIDE.md
│   └── troubleshooting/
│       └── development-issues.md
└── agents/                 # Project blueprint (original)
    └── project_blueprint.md
```

## 🎯 Learning Objectives

After studying this project, you will understand:

1. **Kernel Programming**
   - Character device drivers
   - File operations (open, read, write, poll)
   - Kernel memory management

2. **Synchronization**
   - Spinlocks vs mutexes
   - Atomic operations
   - Memory barriers

3. **Process Management**
   - Context switching
   - Wait queues
   - Process states (running, interruptible, uninterruptible)

4. **Interrupt Handling**
   - Softirq context
   - Timer callbacks
   - Interrupt safety

5. **User-Kernel Interface**
   - System calls
   - Data copying (copy_to_user, copy_from_user)
   - Device files

## 🎓 For Presentations

### Demo Script
1. Show architecture diagram
2. Load module with timer: `sudo insmod event_driver.ko timer_enabled=1`
3. Show device created: `ls -l /dev/pi5_event`
4. Run consumer: `./consumer_app`
5. Show CPU usage: `top` (should be 0%)
6. Send manual event: `echo "DEMO" > /dev/pi5_event`
7. Show consumer receives event
8. Unload cleanly: `sudo rmmod event_driver`

### Key Talking Points
- **Producer-Consumer across OS boundary**
- **0% CPU while waiting** (epoll efficiency)
- **Softirq timer context** (can't sleep, uses spinlock)
- **Bounded queue** (32 events, drops when full)
- **Clean shutdown** (del_timer_sync, wake all)

## 🔧 Troubleshooting

| Problem | Solution |
|---------|----------|
| Module won't load | Check kernel version: `uname -r` vs `modinfo` |
| Permission denied | Device should be 0666 |
| System crash | Wait 5s for auto-reboot |
| No events | Check `dmesg`, verify timer enabled |
| Wrong data size | Must read 40 bytes |

## 📞 Support

For issues or questions:
1. Check [development-issues.md](troubleshooting/development-issues.md)
2. Review [PROJECT_DOCUMENTATION.md](PROJECT_DOCUMENTATION.md)
3. Check kernel logs: `sudo dmesg | grep pi5_event`

## 📄 License

SPDX-License-Identifier: GPL-2.0

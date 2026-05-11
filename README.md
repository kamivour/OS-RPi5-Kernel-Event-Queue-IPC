# Kernel Event Queue IPC

A kernel-level event queue IPC mechanism demonstrating fundamental Operating System concepts: interrupt handling, synchronization, blocking I/O, and inter-process communication.

## Overview

This project implements a **producer-consumer event queue** that bridges kernel-space and user-space on Raspberry Pi 5 (ARM64). It demonstrates how real operating systems handle asynchronous events, process synchronization, and IPC.

### Academic Concepts Demonstrated
1. **Hardware Interrupt Handling** - SoftIRQ context via kernel timers
2. **Kernel Synchronization** - Spinlocks for data race prevention
3. **Process Scheduling & Blocking I/O** - Wait queues and context switching
4. **Asynchronous I/O Multiplexing** - User-space `epoll` integration

## Quick Start

### Prerequisites
- Raspberry Pi 5 (8GB, ARM64)
- Ubuntu Server 24.04 LTS
- Kernel headers: `sudo apt install linux-headers-$(uname -r)`
- Build tools: `sudo apt install build-essential`

### Build
```bash
cd rpi5-target
make
```

---

## RPi5 Deployment Guide

### 1. Initial RPi5 Setup

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install kernel headers and build tools
sudo apt install -y linux-headers-$(uname -r) build-essential

# Enable auto-reboot on kernel panic (for safer development)
echo 5 | sudo tee /proc/sys/kernel/panic
echo kernel.panic=5 | sudo tee -a /etc/sysctl.conf
```

### 2. SSH Setup (Passwordless)

```bash
# On your laptop - generate SSH key
ssh-keygen -t ed25519 -C "your_email@example.com"

# Copy public key to RPi5
cat ~/.ssh/id_ed25519.pub | ssh user@rpi5-ip "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys"

# Test connection
ssh user@rpi5-ip "uname -a"
```

### 3. Deploy Files to RPi5

```bash
# From your laptop - sync source files
rsync -av rpi5-target/ user@rpi5-ip:~/os-project/

# Or use scp
scp rpi5-target/* user@rpi5-ip:~/os-project/
```

### 4. Build on RPi5

```bash
# SSH into RPi5
ssh user@rpi5-ip

# Navigate to project
cd ~/os-project

# Build kernel module
make

# Verify build
ls -lh event_driver.ko
```

### 5. Load and Test

```bash
# Load module (no timer)
sudo insmod event_driver.ko

# Verify device node created
ls -l /dev/pi5_event

# Check kernel logs
sudo dmesg | grep pi5_event

# Test write
echo "TEST_EVENT" > /dev/pi5_event

# Test read (should show the event)
cat /dev/pi5_event | od -c

# Unload
sudo rmmod event_driver
```

### 6. Full Demo (Timer Enabled)

```bash
# Terminal 1: Start consumer
cat /dev/pi5_event | od -c

# Terminal 2: Load with timer
sudo insmod event_driver.ko timer_enabled=1

# Terminal 3: Send manual event
echo "BUTTON_PRESS" > /dev/pi5_event

# Expected: Terminal 1 shows both TIMER_EVENT and BUTTON_PRESS
```

### 7. Troubleshooting

| Issue | Solution |
|-------|----------|
| Permission denied | `sudo chmod 666 /dev/pi5_event` |
| Module won't load | `dmesg \| tail -20` - check kernel logs |
| Build fails | Check kernel headers: `ls /lib/modules/\$(uname -r)/build` |
| System crash | Wait 5s for auto-reboot (panic=5 setting) |
| Connection timeout | Check RPi5 IP: `ping rpi5-ip` |

---

### Load Module
```bash
# Basic (no timer)
sudo insmod event_driver.ko

# With automatic timer events
sudo insmod event_driver.ko timer_enabled=1
```

### Test
```bash
# Terminal 1: Consumer
cat /dev/pi5_event

# Terminal 2: Producer
echo "Hello World" > /dev/pi5_event
```

### Unload
```bash
sudo rmmod event_driver
```

## Project Structure

```
.
├── README.md                      # This file
├── docs/                          # Full documentation
│   ├── PROJECT_DOCUMENTATION.md   # Technical deep dive
│   ├── CONSUMER_APP_GUIDE.md      # User-space app guide
│   ├── QUICK_REFERENCE.md         # Command reference
│   └── troubleshooting/
│       └── development-issues.md  # Bug history
├── scripts/                       # Demo scripts
│   ├── demo_proof.sh              # Full presentation demo
│   ├── test_blocking.sh           # Blocking queue test
│   ├── test_stress.sh             # Multiple producer stress test
│   └── quick_proof.sh             # Quick Q&A proof
├── rpi5-target/                   # Files for RPi5 deployment
│   ├── event_driver.c             # Kernel module source
│   ├── Makefile                   # Build configuration
│   └── consumer_app.cpp           # User-space consumer template
└── old_project/                   # Original userspace simulation
```

## Documentation

- **[Full Documentation](docs/PROJECT_DOCUMENTATION.md)** - Architecture, how it works, demo guide
- **[Consumer App Guide](docs/CONSUMER_APP_GUIDE.md)** - For teammates implementing user-space app
- **[Quick Reference](docs/QUICK_REFERENCE.md)** - One-page command reference
- **[Troubleshooting](docs/troubleshooting/development-issues.md)** - Development issues and solutions

## Key Features

- **Character Device Driver** - Creates `/dev/pi5_event` for user-kernel communication
- **Circular Buffer Queue** - 32-event kernel-side buffer with spinlock protection
- **Dual Producer Modes**:
  - Automatic: Kernel timer fires every 3 seconds (softirq context, drops when full)
  - Manual: User writes via `echo "msg" > /dev/pi5_event` (blocks when full)
- **Blocking Consumer** - `read()` blocks until data available (0% CPU while waiting)
- **Blocking Producer** - Manual writes block when queue is full, resume when space available
- **epoll Support** - Efficient I/O multiplexing for user-space applications

## Technical Details

### Event Format
Each event is exactly 40 bytes:
```
Offset  Size  Field       Type
0-7     8     timestamp   ktime_t (nanoseconds)
8-39    32    data        char[] (message)
Total   40    -           -
```

### Synchronization
- **Spinlock** (`spin_lock_irqsave`) - Protects queue in softirq context
- **Wait Queues** (`wait_queue_head_t`):
  - `read_wait` - Blocks consumer when queue empty
  - `write_wait` - Blocks producer when queue full (manual writes only)
- **Cannot use mutex** - Timer runs in softirq context where sleeping is forbidden

### Blocking Behavior
| Context | Queue Full | Queue Empty |
|---------|------------|-------------|
| Timer (softirq) | Drops (cannot block) | Wakes consumers |
| Manual write | Blocks (or returns -EAGAIN if O_NONBLOCK) | - |
| Manual read | - | Blocks until data available |

### Contexts
- **Timer callback** → SoftIRQ (cannot sleep, must use spinlock)
- **Write syscall** → Process context (can sleep, but we use spinlock for consistency)
- **Read syscall** → Process context, blocks in wait queue

## Demo Scripts

### Proof This Is Real Kernel Code
```bash
./scripts/demo_proof.sh
```

Shows:
- Module loaded in kernel (`lsmod`)
- Device node created (`/dev/pi5_event`)
- Kernel log messages (`dmesg`)
- If it crashes → whole system freezes (not just app)

## Platform

- **Hardware:** Raspberry Pi 5 (8GB RAM)
- **Architecture:** ARM64 (aarch64)
- **OS:** Ubuntu Server 24.04 LTS
- **Kernel:** 6.8.0-1047-raspi

## License

SPDX-License-Identifier: GPL-2.0

## Authors

OS Project - Kernel Event Queue IPC

## Acknowledgments

- Linux Device Drivers (LDD3) - Reference for char driver implementation
- Raspberry Pi 5 RP1 peripheral controller documentation

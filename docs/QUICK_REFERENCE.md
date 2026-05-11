# Quick Reference Card

## One-Line Commands

```bash
# Build
make

# Load (no timer)
sudo insmod event_driver.ko

# Load (with timer)
sudo insmod event_driver.ko timer_enabled=1

# Check device
ls -l /dev/pi5_event

# Check module
lsmod | grep event

# Send event
echo "TEST" > /dev/pi5_event

# Read events (blocking)
cat /dev/pi5_event

# Read events (hexdump)
hexdump -C /dev/pi5_event

# Unload
sudo rmmod event_driver

# Check logs
sudo dmesg | tail -20
```

## Event Structure

```
Offset  Size  Field       Type      Description
0-7     8     timestamp   ktime_t   Nanoseconds since boot
8-39    32    data        char[]    Message payload
Total   40    -           -         -
```

## File Locations

| File | Purpose |
|------|---------|
| `event_driver.c` | Kernel module source |
| `Makefile` | Build configuration |
| `consumer_app.cpp` | User-space consumer template |
| `/dev/pi5_event` | Device node (created on load) |

## Module Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `timer_enabled` | 0 | Auto-generate events every 3s |

```bash
# Enable at load
sudo insmod event_driver.ko timer_enabled=1

# Runtime toggle
echo 1 | sudo tee /sys/module/event_driver/parameters/timer_enabled
```

## Queue Specs

- **Depth:** 32 events
- **Size:** 1280 bytes (32 × 40)
- **Behavior (Timer):** Drop new events when full (softirq constraint)
- **Behavior (Manual):** Block when full, resume when space available
- **Thread-safe:** Yes (spinlock protected)

## Blocking Queue Behavior

| Producer Type | Queue Full Behavior | Reason |
|---------------|---------------------|--------|
| Timer (softirq) | **Drops** event | Cannot block in softirq context |
| Manual write (O_NONBLOCK) | Returns `-EAGAIN` | Non-blocking mode |
| Manual write (blocking) | **Blocks** until space | Process context allows blocking |
| Manual write (signal interrupt) | Returns `-ERESTARTSYS` | Interrupted by signal |

## Demo Scripts

### Basic Test
```bash
sudo insmod event_driver.ko && \
echo "Hello" > /dev/pi5_event && \
cat /dev/pi5_event && \
sudo rmmod event_driver
```

### Blocking Queue Test
```bash
cd scripts
./test_blocking.sh
# Shows: Fill queue → Write blocks → Consumer frees space → Write completes
```

### Stress Test (Multiple Producers)
```bash
cd scripts
./test_stress.sh
# Shows: 5 producers blocked → 3 wake up when space freed
```

### Timer Test
```bash
sudo insmod event_driver.ko timer_enabled=1 && \
timeout 10 cat /dev/pi5_event | od -c && \
sudo rmmod event_driver
```

### Non-blocking Write Test
```bash
sudo insmod event_driver.ko && \
for i in {1..40}; do echo "MSG_$i" > /dev/pi5_event; done && \
# First 32 succeed, remaining fail with "Resource temporarily unavailable"
sudo rmmod event_driver
```

## Error Codes

| Code | Meaning |
|------|---------|
| `EAGAIN` | Non-blocking read: no data; Non-blocking write: queue full |
| `ENOSPC` | (Legacy) Queue full - now blocks instead |
| `EINVAL` | Invalid read size (must be 40) |
| `EFAULT` | Bad user pointer |
| `ERESTARTSYS` | Blocking write interrupted by signal |

## CPU Usage

| State | CPU |
|-------|-----|
| Idle (empty queue) | 0% |
| Reading event | <1% |
| Writing event | <1% |
| Timer callback | <0.1% per 3s |

## Critical Concepts

| Concept | Kernel | User-Space |
|---------|--------|------------|
| Sync | Spinlock | Mutex |
| Blocking | Wait queue | epoll/select |
| Context | SoftIRQ | Process |
| Memory | kmalloc | malloc |

## Troubleshooting Quick Fixes

```bash
# Permission denied
sudo chmod 666 /dev/pi5_event  # or rebuild with mode=0666

# Device doesn't exist
sudo insmod event_driver.ko  # module not loaded

# Module won't load
sudo dmesg \| tail            # check error

# System crash
# Wait 5s for auto-reboot, then:
sudo dmesg \| grep -i panic
```

## Presentation Checklist

- [ ] Timer enabled: `timer_enabled=1`
- [ ] Consumer running: `./consumer_app`
- [ ] Manual trigger ready: `echo "TEST" > /dev/pi5_event`
- [ ] Show dmesg: `sudo dmesg | grep pi5_event`
- [ ] Show epoll blocking: `top` (0% CPU)
- [ ] Show cleanup: `sudo rmmod event_driver`

## Key Files for Presentation

```
docs/
├── PROJECT_DOCUMENTATION.md    (full docs)
├── CONSUMER_APP_GUIDE.md       (teammate guide)
├── QUICK_REFERENCE.md          (this file)
└── troubleshooting/
    └── development-issues.md   (bug history)
```

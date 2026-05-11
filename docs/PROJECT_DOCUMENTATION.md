# Kernel Event Queue IPC - Project Documentation

## Table of Contents
1. [Project Overview](#project-overview)
2. [Architecture](#architecture)
3. [Technology Stack](#technology-stack)
4. [How It Works](#how-it-works)
5. [Setup & Installation](#setup--installation)
6. [Demo Guide](#demo-guide)
7. [Expected Outputs](#expected-outputs)
8. [Technical Deep Dive](#technical-deep-dive)

---

## Project Overview

### Purpose
This project demonstrates fundamental Operating System concepts by implementing a **kernel-level event queue** that bridges kernel-space and user-space. It showcases how real operating systems handle asynchronous events, process synchronization, and inter-process communication (IPC).

### Academic Concepts Demonstrated
1. **Hardware Interrupt Handling** - SoftIRQ context via kernel timers
2. **Kernel Synchronization** - Spinlocks for data race prevention
3. **Process Scheduling & Blocking I/O** - Wait queues and context switching
4. **Asynchronous I/O Multiplexing** - User-space `epoll` integration
5. **Producer-Consumer Pattern** - Classic concurrency pattern across OS boundaries

### Target Platform
- **Hardware:** Raspberry Pi 5 (8GB, ARM64)
- **OS:** Ubuntu Server 24.04 LTS (64-bit)
- **Kernel:** 6.8.0-1047-raspi
- **Architecture:** ARM64 (aarch64)

---

## Architecture

### High-Level Data Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                     KERNEL SPACE (Ring 0)                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐      ┌─────────────┐      ┌──────────────┐   │
│  │   PRODUCER   │─────▶ │   QUEUE     │─────▶ │  CONSUMER    │   │
│  │              │      │             │      │              │   │
│  │ • Timer (3s) │      │ • Circular  │      │ • Blocking   │   │
│  │ • Manual     │      │   Buffer    │      │   read()     │   │
│  │   write()    │      │ • 32 events │      │ • poll()     │   │
│  └──────────────┘      │ • Spinlock  │      │              │   │
│         │             └─────────────┘             │          │
│         │                    │                    │          │
│         │            ┌───────┴───────┐            │          │
│         │            │  Wait Queue   │◀───────────┘          │
│         │            │  (sleep/wake) │                       │
│         │            └───────────────┘                       │
│         └────────────────────────────────────────────────────┘
│                           │
└───────────────────────────┼─────────────────────────────────────┘
                            │
                    ┌───────┴────────┐
                    │  OS BOUNDARY   │
                    │ /dev/pi5_event │
                    └───────┬────────┘
                            │
┌───────────────────────────┼─────────────────────────────────────┐
│                   USER SPACE (Ring 3)                          │
├───────────────────────────┼─────────────────────────────────────┤
│                           │                                     │
│  ┌──────────────┐         │         ┌──────────────┐           │
│  │  Producer    │─────────┼────────▶│  Consumer    │           │
│  │              │         │         │              │           │
│  │ echo "..."   │─────────┼────────▶│ epoll_wait() │           │
│  │   > device   │         │         │   cat device │           │
│  └──────────────┘         │         └──────────────┘           │
│                           │                                     │
└───────────────────────────┼─────────────────────────────────────┘
```

### Component Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                          User Application                       │
│  ┌────────────┐    ┌────────────┐    ┌────────────────────┐    │
│  │   epoll    │◀───│    read    │◀───│   select/poll      │    │
│  │   loop     │    │   (40B)    │    │   monitoring       │    │
│  └────────────┘    └────────────┘    └────────────────────┘    │
└──────────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                       VFS Layer                                  │
│  ┌────────────────────────────────────────────────────────┐     │
│  │              file_operations structure                  │     │
│  │  • .open    • .release  • .read   • .write  • .poll    │     │
│  └────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────┘
                           │
                           ▼
┌──────────────────────────────────────────────────────────────────┐
│                    event_driver.ko                               │
│  ┌─────────────┐    ┌──────────────┐    ┌─────────────────┐    │
│  │   Timer     │    │   Queue      │    │  Sync Objects   │    │
│  │  (softirq)  │───▶│  Management  │◀───│  • Spinlock     │    │
│  │             │    │              │    │  • Wait Queue   │    │
│  │ mod_timer() │    │ Circular Buf │    │                 │    │
│  └─────────────┘    │ 32 × 40B     │    └─────────────────┘    │
│                     └──────────────┘                            │
└──────────────────────────────────────────────────────────────────┘
```

---

## Technology Stack

### Kernel Components
| Component | Header File | Purpose |
|-----------|-------------|---------|
| Character Device | `<linux/miscdevice.h>` | Device node registration |
| Spinlock | `<linux/spinlock.h>` | Atomic synchronization |
| Wait Queue | `<linux/wait.h>` | Blocking/sleeping |
| Timer | `<linux/timer.h>` | SoftIRQ callbacks |
| Poll | `<linux/poll.h>` | epoll support |

### Data Structures
```c
// Event payload (40 bytes total)
struct event_msg {
    ktime_t timestamp;    // 8 bytes - kernel timestamp
    char data[32];        // 32 bytes - message payload
};

// Circular queue (32 events = 1280 bytes)
struct circular_queue {
    struct event_msg buffer[32];
    int head;             // Write position
    int tail;             // Read position
    int count;            // Current event count
};
```

### User-Space APIs
- **epoll(7)** - Efficient I/O event notification
- **open(2)/read(2)/write(2)** - Standard file operations
- **poll(2)** - I/O multiplexing

---

## How It Works

### 1. Producer Path (Timer - Automatic Events)

```
Timer fires every 3 seconds
         │
         ▼
┌─────────────────────────────────────────┐
│ timer_callback() [SOFTIRQ CONTEXT]      │
│ • Runs in softirq (deferred interrupt)  │
│ • Cannot sleep - must use spinlock      │
│ • Creates event with timestamp          │
└─────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│ Critical Section (spin_lock_irqsave)    │
│ • Check if queue full                   │
│ • Add event to circular buffer          │
│ • Update head pointer                   │
└─────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│ Wake Up Consumers                       │
│ • wake_up_interruptible()               │
│ • Marks sleeping tasks as runnable      │
│ • Scheduler will wake them soon         │
└─────────────────────────────────────────┘
```

### 2. Producer Path (Manual - User Triggered)

```
User writes to /dev/pi5_event
         │
         ▼
┌─────────────────────────────────────────┐
│ event_write() [PROCESS CONTEXT]         │
│ • copy_from_user() - safe data copy     │
│ • Create event with timestamp           │
└─────────────────────────────────────────┘
         │
         ▼
    (Same as above - queue + wake)
```

### 3. Consumer Path (Blocking Read)

```
User calls read() on /dev/pi5_event
         │
         ▼
┌─────────────────────────────────────────┐
│ event_read()                            │
│ • Check if queue has events             │
│   ├─ YES → Read and return              │
│   └─ NO  → Go to sleep                  │
└─────────────────────────────────────────┘
         │
         ▼ (if empty)
┌─────────────────────────────────────────┐
│ wait_event_interruptible()              │
│ • Adds process to wait queue            │
│ • Marks process as TASK_INTERRUPTIBLE    │
│ • Calls scheduler() → CPU switched      │
│ • Process sleeps, 0% CPU usage          │
└─────────────────────────────────────────┘
         │
         ▼ (when producer wakes)
┌─────────────────────────────────────────┐
│ Scheduler Wakes Process                 │
│ • Process marked runnable               │
│ • Resumes from wait_event_interruptible │
│ • Reads event from queue                │
│ • copy_to_user() - returns to userspace │
└─────────────────────────────────────────┘
```

### 4. Consumer Path (epoll - Efficient Monitoring)

```
User sets up epoll
         │
         ▼
┌─────────────────────────────────────────┐
│ epoll_create1() + epoll_ctl()           │
│ • Creates epoll instance                │
│ • Adds /dev/pi5_event with EPOLLIN      │
└─────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────┐
│ epoll_wait()                            │
│ • Sleeps until device ready             │
│ • Kernel tracks multiple FDs efficiently│
│ • O(1) complexity regardless of FDs     │
└─────────────────────────────────────────┘
         │
         ▼ (when event available)
┌─────────────────────────────────────────┐
│ Event Ready                             │
│ • epoll_wait returns                    │
│ • User calls read() to get data         │
└─────────────────────────────────────────┘
```

---

## Setup & Installation

### Prerequisites
```bash
# On RPi 5
sudo apt update
sudo apt install linux-headers-$(uname -r)
sudo apt install build-essential
```

### Build
```bash
# Transfer files to RPi5
rsync -av event_driver.c Makefile user@rpi5:~/os-project/

# On RPi5
cd ~/os-project
make
```

### Load Module
```bash
# Basic load (no timer)
sudo insmod event_driver.ko

# Load with timer enabled
sudo insmod event_driver.ko timer_enabled=1

# Verify
ls -l /dev/pi5_event
lsmod | grep event
dmesg | tail
```

### Unload Module
```bash
sudo rmmod event_driver
```

---

## Demo Guide

### Demo 1: Basic Blocking Read

**Objective:** Demonstrate blocking I/O - process sleeps waiting for data

```bash
# Terminal 1: Start consumer (will block)
cat /dev/pi5_event

# Terminal 2: Send data (Terminal 1 wakes up)
echo "Hello World" > /dev/pi5_event
```

**What happens:**
1. Terminal 1: `cat` opens device and calls `read()`
2. Queue is empty → process goes to sleep (0% CPU)
3. Terminal 2: `echo` writes data → kernel wakes sleeping process
4. Terminal 1: `cat` receives data and displays it

**Expected output (Terminal 1):**
```
Hello World
```

### Demo 2: Automatic Timer Events

**Objective:** Demonstrate kernel timer (softirq context) producing events

```bash
# Terminal 1: Monitor events
hexdump -C /dev/pi5_event

# Terminal 2: Enable timer
sudo insmod event_driver.ko timer_enabled=1
```

**What happens:**
1. Kernel timer fires every 3 seconds (softirq context)
2. Each timer creates an event with "TIMER_EVENT" message
3. Events queued and consumer wakes up to read them

**Expected output (Terminal 1):**
```
00000000  e4 7c 6d 5f 0d 00 00 00  54 49 4d 45 52 5f 45 56  |.|m_...TIMER_EV|
00000010  45 4e 54 00 00 00 00 00  00 00 00 00 00 00 00 00  |ENT............|
00000020  00 00 00 00 00 00 00 00  00                       |.........|
```

### Demo 3: Manual + Timer Mixed

**Objective:** Show both producers working together

```bash
# Terminal 1: Consumer
while true; do timeout 5 dd if=/dev/pi5_event bs=40 count=1 2>/dev/null | od -c; sleep 0.5; done

# Terminal 2: Send manual events
echo "BUTTON_A" > /dev/pi5_event
sleep 2
echo "BUTTON_B" > /dev/pi5_event
```

**Expected output (Terminal 1):**
```
# Timer event
0000000  3 1 7   0 0 0 0 0   T   I   M   E   R   _   E   V
0000020   E   N   T  \0  \0  \0  \0  \0  \0  \0  \0  \0  \0

# Manual event
0000000    2 0 0   \f 031  \0  \0  \0   B   U   T   T   O   N
0000020   _   A  \0  \0  \0  \0  \0  \0  \0  \0  \0  \0  \0

# Another timer event
0000000  260 025 233   \f 031  \0  \0  \0   T   I   M   E   R
0000020   _   E   V   E   N   T  \0  \0  \0  \0  \0  \0  \0
```

### Demo 4: Consumer App (Teammate's C++)

**Objective:** Demonstrate epoll-based efficient monitoring

```cpp
// consumer_app.cpp
int epfd = epoll_create1(0);
struct epoll_event ev = {.events = EPOLLIN, .data.fd = fd};
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

while (1) {
    epoll_wait(epfd, &ev, 1, -1);  // Sleep until event
    read(fd, &event, sizeof(event));
    printf("[EVENT] %s\n", event.data);
}
```

```bash
# Terminal 1
./consumer_app

# Terminal 2
echo "ACTION_1" > /dev/pi5_event
echo "ACTION_2" > /dev/pi5_event
```

**Expected output (Terminal 1):**
```
Consumer started. Waiting for events...
[TIP: Use 'echo TEST > /dev/pi5_event' to trigger manually]
[EVENT] ACTION_1
[EVENT] ACTION_2
```

### Demo 5: Queue Full Behavior

**Objective:** Demonstrate bounded queue behavior

```bash
# Terminal 1: Don't read (simulate slow consumer)
# Terminal 2: Spam events
for i in {1..40}; do echo "MSG_$i" > /dev/pi5_event; done
```

**Expected:**
- First 32 messages accepted
- Messages 33-40 rejected with "No space left on device"
- dmesg shows warnings if timer tries to write to full queue

---

## Expected Outputs

### Device Node
```bash
$ ls -l /dev/pi5_event
crw-rw-rw- 1 root root 10, 123 May 12 01:42 /dev/pi5_event
# ^ Character device (c), major 10, minor 123
```

### Kernel Messages
```bash
$ sudo dmesg | grep pi5_event
[  53.078] pi5_event: driver loaded, /dev/pi5_event created
[  98.539] pi5_event: timer enabled
[ 123.270] pi5_event: driver unloaded safely
```

### Event Format (40 bytes)
```
Offset  Size    Field        Description
0-7     8 bytes ktime_t      Kernel timestamp (nanoseconds)
8-39    32 bytes char[32]     Message payload (null-terminated)
```

### Example Raw Output
```bash
$ echo "TEST" > /dev/pi5_event
$ dd if=/dev/pi5_event bs=40 count=1 2>/dev/null | hexdump -C
00000000  b4 9e 3d 66 0d 00 00 00  54 45 53 54 00 00 00 00  |..=f....TEST....|
00000010  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
00000028
```

---

## Technical Deep Dive

### Why Spinlock Not Mutex?

**Mutex:**
- Can sleep if lock is contended
- **FORBIDDEN** in interrupt/softirq context
- Only usable in process context

**Spinlock:**
- Busy-waits (spins) if lock is contended
- Safe in interrupt/softirq context
- Required for timer callback (softirq)

**Our timer runs in softirq context:**
```c
static void timer_callback(struct timer_list *t) {
    // In softirq context - CANNOT sleep!
    spin_lock_irqsave(&queue_lock, flags);  // OK
    // mutex_lock(&mutex);  // ❌ WOULD CRASH
    // ...
    spin_unlock_irqrestore(&queue_lock, flags);
}
```

### Why `spin_lock_irqsave` not `spin_lock`?

```c
spin_lock(&lock);           // Disables preemption only
spin_lock_irqsave(&lock, flags);  // Disables preemption + interrupts
```

**In softirq context:**
- Local interrupts must be disabled
- Prevents deadlock if IRQ handler tries to acquire same lock
- `irqsave` saves current interrupt state, `irqrestore` restores it

### Wait Queue Mechanics

**Process states:**
```
TASK_RUNNING      → Executable or running
TASK_INTERRUPTIBLE → Sleeping, waiting for event
TASK_UNINTERRUPTIBLE → Sleeping, NOT interruptible by signals
```

**Blocking read flow:**
```c
// 1. Process in TASK_RUNNING state
if (event_queue.count == 0) {
    // 2. Transition to TASK_INTERRUPTIBLE
    // 3. Add to wait queue
    // 4. Call schedule() → CPU switches to another process
    wait_event_interruptible(read_wait, event_queue.count > 0);
    // ... process sleeping here, 0% CPU ...
    // 5. Producer calls wake_up_interruptible()
    // 6. Process marked TASK_RUNNING, scheduled again
    // 7. Resumes here when condition is true
}
```

### epoll Efficiency

**Traditional poll vs epoll:**
```
poll():
  O(n) - must scan all file descriptors every time

epoll():
  O(1) - kernel maintains ready list
       - Only returns ready descriptors
       - Scales to thousands of connections
```

**epoll internals:**
1. **Red-black tree** - stores all monitored FDs
2. **Ready list** - FDs with pending events
3. **epoll_wait** - just checks ready list (O(1))

### Context Switch Cost

**When user reads from empty queue:**
```
1. User process calls read()
2. Kernel → TASK_INTERRUPTIBLE
3. Context switch → another process runs
4. Timer fires → event queued
5. Kernel wakes our process → TASK_RUNNING
6. Context switch → our process resumes
7. Data copied to user → read() returns
```

**Why this is efficient:**
- Sleeping process uses 0% CPU
- Scheduler handles hundreds of sleeping processes
- Wakeup is O(1) via wait queue

---

## Troubleshooting

### Module Won't Load
```bash
# Check kernel version match
modinfo event_driver.ko
uname -r

# Check dmesg for errors
sudo dmesg | tail -20
```

### Device Not Created
```bash
# Check if module loaded
lsmod | grep event

# Check misc devices
ls -l /dev/misc/
cat /proc/misc
```

### Permission Denied
```bash
# Device mode is 0666 (rw-rw-rw-)
# If permission issues, check:
ls -l /dev/pi5_event

# Or run with sudo
sudo cat /dev/pi5_event
```

### Timer Not Firing
```bash
# Check if timer enabled
cat /sys/module/event_driver/parameters/timer_enabled

# Enable timer
sudo insmod event_driver.ko timer_enabled=1
```

### System Crash/Panic
```bash
# Auto-reboot should trigger (5 second timeout)
# Check crash logs after reboot:
sudo dmesg | grep -i "panic\|oops\|event"

# Common causes:
# - kfifo issues (fixed with circular buffer)
# - Sleep in softirq (ensure no mutex/msleep in timer)
# - Missing spinlock (data races)
```

---

## Performance Characteristics

### Memory Usage
- Module code: ~15KB
- Queue buffer: 1280 bytes (32 × 40 bytes)
- Per-open overhead: Minimal (file struct)

### CPU Usage
- **Idle (no events):** 0% - processes sleep
- **Event processing:** <1% per event
- **Timer overhead:** Minimal (one callback per 3 seconds)

### Throughput
- **Max queue depth:** 32 events
- **Event size:** 40 bytes
- **Latency:** <1ms (wake up to read)

### Scalability
- **Multiple readers:** Safe (but each gets different events)
- **Multiple writers:** Safe (protected by spinlock)
- **Queue contention:** Minimal (simple circular buffer)

---

## Extension Ideas

1. **Priority Queue** - Add priority field to events
2. **Multiple Queues** - Different channels for different event types
3. **Event Filtering** - Kernel-side filtering based on user criteria
4. **Statistics** - Export metrics via sysfs or debugfs
5. **GPIO Integration** - Real hardware interrupts on RPi5
6. **Network IPC** - Export events via socket

---

## References

- **Linux Device Drivers (LDD3)** - Chapter 6: Advanced Char Driver Operations
- **Understanding the Linux Kernel** - Wait queues, softirqs
- **RPi5 Peripheral Docs** - RP1 controller, GPIO constraints
- **epoll(7) man page** - I/O event notification

---

## License

SPDX-License-Identifier: GPL-2.0

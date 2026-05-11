# 🚀 PROJECT CONTEXT & BLUEPRINT: Kernel-Level Event Queue IPC

## 1. PROJECT OVERVIEW & GOALS
- **Domain:** Linux Kernel Development / Character Device Driver (Loadable Kernel Module - LKM).
- **Core Objective:** Implement a hardware-interrupt-driven, asynchronous IPC (Inter-Process Communication) mechanism bridging Kernel-space and User-space.
- **Academic OS Concepts to Demonstrate:**
  1. Hardware Interrupt Handling (ISR).
  2. Kernel Synchronization (`Spinlock`) to prevent Data Races.
  3. Process Scheduling & Blocking I/O (`Wait Queue` & Context Switching).
  4. Asynchronous I/O Multiplexing in User-space (`epoll`).

## 2. HARDWARE & ENVIRONMENT SETUP
- **Target Hardware:** Raspberry Pi 5 8GB (ARM64 architecture).
  - *⚠️ CRITICAL CONSTRAINT:* RPi 5 uses the **RP1 peripheral controller** via PCIe. GPIOs are NOT directly connected to the CPU. Legacy integer-based GPIO APIs (`<linux/gpio.h>`) will cause Kernel Panics. You MUST use the GPIO Descriptor API (`<linux/gpio/consumer.h>`, `gpiod`).
- **Target OS:** Ubuntu Server 24.04 LTS (64-bit, Headless CLI) - chosen for fast recovery (~15s) during Kernel Panics.
- **Host Machine:** User's Laptop (Where this AI Agent is running).
- **Workflow Strategy ("Host-Target Isolation"):**
  1. **Brain (Host):** All code generation, editing, and AI Agent context reside strictly on the Laptop.
  2. **Sync:** Code is pushed to the Target via `rsync` or SFTP script.
  3. **Muscle (Target):** We compile natively on the RPi 5 (`make`). No cross-compilation toolchains on the Host. Testing (`insmod`/`rmmod`) happens on the Target. If it crashes (Kernel Panic), we reboot the Target, and the Host AI context remains perfectly safe.

## 3. ARCHITECTURE BLUEPRINT (DATA FLOW)
We are building a Producer-Consumer model across the OS boundary.

```text
[ KERNEL-SPACE: event_driver.ko ]
  (1) PRODUCER: Mock Timer (Phases 1-3) -> Hardware IRQ (Phase 4)
         |  - Generates event payload (timestamp). Runs in Interrupt Context (Ring 0).
         v
  (2) SYNCHRONIZATION: `spin_lock_irqsave`
         |  - Protects the queue. Mutexes are STRICTLY FORBIDDEN here.
         v
  (3) BLOCKING QUEUE: `kfifo` (`<linux/kfifo.h>`)
         |  - Stores the event payload securely.
         v
  (4) SCHEDULING: `wait_queue_head_t`
         |  - Calls `wake_up_interruptible()` to unblock sleeping User-space apps.

==================== OS BOUNDARY (/dev/pi5_event) ====================

[ USER-SPACE: consumer_app.cpp (Teammate's task) ]
  (5) EVENT LOOP: `epoll_wait()`
         |  - App sleeps (0.0% CPU) until the OS wakes it up.
         v
  (6) CONSUMER: `read()`
         |  - Awakened via Context Switch, reads KFIFO data, logs it, and sleeps again.

## 4. DEVELOPMENT ROADMAP (NO HARDWARE REQUIRED)
*Since physical GPIO hardware is unavailable, we will demonstrate the exact same OS concepts (Interrupt Context, Synchronization, Blocking I/O) using software-driven mock triggers.*

- **[ ] PHASE 1: Base Driver & VFS**
  - Setup `Makefile` for native target build on RPi 5.
  - Create `event_driver.c`. Use `<linux/miscdevice.h>` to auto-create the node `/dev/pi5_event`.
  - Goal: Successful `make`, `insmod`, and `ls /dev/pi5_event` on Target.

- **[ ] PHASE 2: Core Data Structures & Consumer (Read/Poll)**
  - Inject `DECLARE_KFIFO`, `DEFINE_SPINLOCK`, and `init_waitqueue_head`.
  - Implement `.read` using `wait_event_interruptible` (blocks if queue is empty).
  - Implement `.poll` to support User-space `epoll`.
  - Goal: Running `cat /dev/pi5_event` blocks the terminal indefinitely (0% CPU).

- **[ ] PHASE 3: The "Auto" Producer (Kernel Timer Mock)**
  - Implement a Kernel Timer (`mod_timer`) firing every 3 seconds.
  - *Context Rule:* The timer callback runs in SoftIRQ/Interrupt context. It MUST use `spin_lock_irqsave()`, write a dummy timestamp payload to `kfifo`, release the lock, and call `wake_up_interruptible()`.
  - Goal: `cat /dev/pi5_event` automatically wakes up and prints a new event every 3 seconds. Demonstrates Interrupt Context constraints and Spinlocks.

- **[ ] PHASE 4: The "Interactive" Producer (Virtual Button)**
  - Disable the Timer to stop automatic event generation.
  - Implement the `.write` file operation.
  - When the user types `echo "HELLO" > /dev/pi5_event` from another terminal, the `.write` function acts as our manual trigger: it acquires the spinlock, pushes the string into `kfifo`, and calls `wake_up_interruptible()`.
  - Goal (Final Demo): 
    - Terminal 1 runs C++ Consumer App (`epoll_wait`).
    - Terminal 2 runs `echo "ACTION_1" > /dev/pi5_event`.
    - Terminal 1 instantly wakes up, reads "ACTION_1", and sleeps again.

## 5. STRICT AI DIRECTIVES (SYSTEM PROMPT)
When acting on this project, you (the AI) must obey:
    - Safety First: Functions that can sleep (msleep, mutex_lock, kmalloc without GFP_ATOMIC, copy_to_user) MUST NEVER be called inside the Timer callback or ISR.
    - Boilerplate Reduction: Always use miscdevice for character device registration.
    - Step-by-Step: Follow the Phases. When asked to start, only generate code for the requested Phase.
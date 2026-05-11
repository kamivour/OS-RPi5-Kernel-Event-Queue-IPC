# Development Issues & Solutions

## Issue Log: RPi5 Kernel Module Development

### 1. Missing Header Include
**What:** Build error - `unknown type name 'poll_table'`
```
error: unknown type name 'poll_table'; did you mean 'pmd_table'?
```

**Why:** `poll_table` type defined in `<linux/poll.h>` but header not included.

**Solution:**
```c
#include <linux/poll.h>
```

---

### 2. Build Tools Dependency Hell
**What:** `apt install build-essential` failed with unmet dependencies
```
dpkg-dev : Depends: bzip2 but it is not installable
bzip2 : Depends: libbz2-1.0 (= 1.0.8-5.1) but 1.0.8-5.1build0.1 is to be installed
```

**Why:** Mixed repository states or partial upgrade caused version mismatch between `libbz2-1.0` and `bzip2` package requirements.

**Solution:**
```bash
# Downgrade libbz2 to match bzip2 requirements
sudo apt install -y --allow-downgrades libbz2-1.0=1.0.8-5.1 bzip2=1.0.8-5.1
sudo apt install -y make gcc
```

---

### 3. Kernel Panic on rmmod (Timer Race Condition)
**What:** System freezes completely when unloading module. Requires power cycle. No panic logs.

**Why:** Race condition between timer callback and module cleanup:
1. `timer_callback()` checks `timer_enabled` **outside** spinlock
2. `timer_enabled` is `true`, so `mod_timer()` is called
3. Simultaneously, `event_exit()` calls `del_timer_sync()`
4. Timer gets rearmed after being deleted → corruption/freeze

**Solution:**
```c
// Move mod_timer inside spinlock, check timer_enabled under lock
static void timer_callback(struct timer_list *t) {
    // ...
    spin_lock_irqsave(&queue_lock, flags);

    // ... queue operations ...

    if (timer_enabled)
        mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));

    spin_unlock_irqrestore(&queue_lock, flags);
}

// Set timer_enabled = false BEFORE del_timer_sync
static void __exit event_exit(void) {
    timer_enabled = false;  // Prevent re-arm
    del_timer_sync(&event_timer);
    // ... rest of cleanup
}
```

**Status:** Partially fixed. Timer still causes issues - investigation ongoing.

---

### 4. Timer Still Causes System Freeze (ONGOING)
**What:** Even with race condition fix, enabling timer causes hard freeze within ~10 seconds.

**Why:** Unknown. Possible causes:
- Timer callback doing something illegal in softirq context
- `wake_up_interruptible()` issue from softirq
- kfifo operation problem
- ARM64-specific issue

**Current Workaround:** Set `timer_enabled = false` by default:
```c
static bool timer_enabled = false;  // Timer disabled until fix found
```

**Next Steps:**
- Add printk debugging to timer callback
- Check if issue is `wake_up_interruptible()` from softirq context
- Consider using `wake_up()` instead (interrupt-safe)
- Test with simpler timer callback (just printk, no queue operations)

---

## Environment Details
- **Target:** Raspberry Pi 5 (8GB, ARM64)
- **OS:** Ubuntu Server 24.04 LTS
- **Kernel:** 6.8.0-1047-raspi
- **Compiler:** gcc-13 (Ubuntu 13.3.0-6ubuntu2.24.04.1)
- **Kernel Built With:** aarch64-linux-gnu-gcc-13

## SSH Automation Setup
```bash
# One-time SSH key setup for passwordless automation
ssh-keygen -t rsa -f ~/.ssh/id_rsa -N ""
cat ~/.ssh/id_rsa.pub | ssh user@host "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys"
```

---

### 5. kfifo Struct Crash on ARM64 (ONGOING)
**What:** System crashes when using kfifo with struct type on ARM64. Crash in `__kfifo_in` → `__memcpy`.

**Why:** kfifo on ARM64 with complex structs has issues. Possible causes:
- Struct padding/alignment issues on 64-bit ARM
- kfifo memcpy operations not handling struct boundaries correctly
- DECLARE_KFIFO + kfifo_alloc API mismatch

**Attempted Fixes:**
1. ✗ DECLARE_KFIFO + kfifo_alloc - API mismatch, failed with EINVAL
2. ✗ DEFINE_KFIFO with struct - crashes in memcpy
3. ✗ Dynamic kfifo_alloc with power-of-2 size - still crashes
4. ✗ Byte buffer approach - crashes on read

**Current Status:** UNRESOLVED. Both write and read paths cause kernel panics.

**Workaround Needed:**
- Consider simpler data structure (just char arrays)
- Use different synchronization primitive
- Investigate ARM64-specific kfifo usage patterns

**Error Trace:**
```
__memcpy+0x15c/0x240
__kfifo_in+0x38/0x58
event_write+0x184/0x368 [event_driver]
```

---

## Next Steps

1. **Manual reboot required** - system not recovering from panic
2. **Simplify driver** - remove kfifo, use simple circular buffer
3. **Consider using existing patterns** - look at working ARM64 drivers
4. **Maybe defer timer/debug** - get basic read/write working first

---

### 6. SOLUTION: Simple Circular Buffer (FIXED)

**What finally worked:** Custom circular buffer implementation without kfifo.

**Why it works:**
- No dependency on kfifo's complex memcpy operations
- Direct struct assignment (no byte-level copying)
- Simple head/tail pointer arithmetic
- Full control over memory layout

**Implementation:**
```c
struct circular_queue {
    struct event_msg buffer[QUEUE_DEPTH];
    int head;  // Write position
    int tail;  // Read position
    int count; // Current event count
};

static int queue_put(struct event_msg *msg) {
    if (event_queue.count >= QUEUE_DEPTH)
        return -ENOSPC;
    event_queue.buffer[event_queue.head] = *msg;  // Direct assignment
    event_queue.head = (event_queue.head + 1) % QUEUE_DEPTH;
    event_queue.count++;
    return 0;
}
```

**All phases working:**
- ✅ Phase 1: Device creation
- ✅ Phase 2: Blocking read
- ✅ Phase 3: Timer producer
- ✅ Phase 4: Manual write

**Key lesson:** On ARM64, kfifo with structs has alignment/memcpy issues. Simple circular buffer is safer for educational code.

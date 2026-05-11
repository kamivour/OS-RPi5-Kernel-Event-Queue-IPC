# Giải thích Project: Kernel Event Queue IPC (Tiếng Việt)

## TỔNG QUAN

Đây là một **kernel module** (driver) chạy trên Raspberry Pi 5, mục đích là demo các khái niệm Operating System quan trọng:
- Interrupt handling (xử lý ngắt)
- Synchronization (đồng bộ hóa)
- Blocking I/O (I/O chặn)
- IPC (Inter-Process Communication - giao tiếp giữa processes)

---

## 1. ARCHITECTURE - KIẾN TRÚC HỆ THỐNG

```
┌─────────────────────────────────────────────────────────────┐
│                   KERNEL SPACE (Ring 0)                      │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐              │
│  │ Producer │───▶│  Queue   │───▶│ Consumer │              │
│  │  (Timer) │    │ (Buffer) │    │  (read)  │              │
│  └──────────┘    └──────────┘    └──────────┘              │
└───────────────────────────┬────────────────────────────────┘
                            │
                    ┌───────┴────────┐
                    │ /dev/pi5_event │  ← Device node
                    └───────┬────────┘
                            │
┌───────────────────────────┴────────────────────────────────┐
│                   USER SPACE (Ring 3)                       │
│  ┌──────────┐                  ┌──────────┐                │
│  │ Producer │                  │ Consumer │                │
│  │ (echo)   │                  │ (cat)    │                │
│  └──────────┘                  └──────────┘                │
└─────────────────────────────────────────────────────────────┘
```

**Ý nghĩa:**
- **Ring 0 (Kernel)** = Chế độ đặc quyền, toàn quyền control hardware
- **Ring 3 (User)** = Chế độ người dùng, bị giới hạn
- Device node = Cầu nối giữa User và Kernel

---

## 2. COMPONENTS - CÁC THÀNH PHẦN CHÍNH

### 2.1 Character Device Driver

**File:** `event_driver.c`

**Mục đích:** Tạo một file device `/dev/pi5_event` để user-space có thể read/write.

**Vì sao là "Character Device"?**
- Block device: disk, đọc theo block (512 bytes, 4KB...)
- Character device: đọc từng byte, stream data (keyboard, serial, our device)

**Code quan trọng:**
```c
static const struct file_operations event_fops = {
    .owner   = THIS_MODULE,
    .open    = event_open,
    .release = event_release,
    .read    = event_read,      // User gọi read() → hàm này chạy
    .write   = event_write,     // User gọi write() → hàm này chạy
    .poll    = event_poll,      // User gọi epoll() → hàm này chạy
};

// Đăng ký với kernel
static struct miscdevice event_device = {
    .name = "pi5_event",        // Tên device node
    .fops = &event_fops,        // Con trỏ đến hàm functions
    .mode = 0666,              // Permission: rw-rw-rw-
};
```

### 2.2 Circular Queue - Hàng đợi vòng

**Mục đích:** Lưu trữ events trong kernel, 32 events max.

**Data structure:**
```c
struct event_msg {
    ktime_t timestamp;    // 8 bytes - thời gian kernel tạo event
    char data[32];        // 32 bytes - nội dung message
}; // Total = 40 bytes

struct circular_queue {
    struct event_msg buffer[32];  // Mảng 32 events
    int head;             // Vị trí ghi tiếp theo
    int tail;             // Vị trí đọc tiếp theo
    int count;            // Số events hiện có
};
```

**Vì sao Circular?**
```
[0][1][2][3]...[30][31]
 ↑              ↑
tail           head

Khi head = 31, event tiếp theo ghi vào [0] (quay vòng)
```

**Lợi ích:** Không cần realloc, fixed size, hiệu quả cao.

### 2.3 Spinlock - Khóa xoay

**Mục đích:** Bảo vệ queue khỏi data race khi 2 thread truy cập đồng thời.

**Code:**
```c
static DEFINE_SPINLOCK(queue_lock);

// Trong hàm write:
unsigned long flags;
spin_lock_irqsave(&queue_lock, flags);  // Lock
// ... critical section ...
spin_unlock_irqrestore(&queue_lock, flags);  // Unlock
```

**Tại sao gọi "xoay"?**
- Mutex: Thread ngủ nếu lock busy → thread khác có thể chạy
- Spinlock: Thread "xoay" chờ (check liên tục) → không ngủ, giữ CPU

**Tại sao phải dùng Spinlock?**
```
Timer callback chạy trong SOFTIRQ CONTEXT:
- Không thể ngủ (cannot sleep)
- Mutex = có thể sleep → ❌ CRASH!
- Spinlock = không sleep → ✅ OK
```

**`irqsave` là gì?**
```c
spin_lock(&lock);           // Chỉ disable preemption
spin_lock_irqsave(&lock, flags);  // Disable preemption + interrupts
```

Trong softirq context, PHẢI disable interrupts để tránh deadlock.

### 2.4 Wait Queue - Hàng đợi chờ

**Mục đích:** Khi queue rỗng, reader "ngủ" chờ event (0% CPU).

**Code:**
```c
static DECLARE_WAIT_QUEUE_HEAD(read_wait);

// Trong read():
if (queue_empty) {
    // Ngủ đây cho đến khi có event
    wait_event_interruptible(read_wait, !queue_empty);
}
// Tỉnh dậy, có event để đọc
```

**Cơ chế hoạt động:**
```
Thread gọi read():
1. Queue rỗng → thread chuyển sang TASK_INTERRUPTIBLE
2. Thread add vào wait queue
3. Gọi schedule() → CPU chuyển sang thread khác
4. Thread hiện tại "ngủ", 0% CPU

Producer viết event:
1. Thêm event vào queue
2. Gọi wake_up_interruptible(&read_wait)
3. Kernel đánh dấu reader = TASK_RUNNING
4. Lần schedule tới, reader tỉnh dậy
5. Reader đọc event và return về user-space
```

### 2.5 Timer - Bộ đếm thời gian

**Mục đích:** Tự động tạo events mỗi 3 giây (demo interrupt handling).

**Code:**
```c
static struct timer_list event_timer;

// Callback function
static void timer_callback(struct timer_list *t) {
    // Tạo event
    struct event_msg msg = {
        .timestamp = ktime_get(),
        .data = "TIMER_EVENT"
    };
    
    // Ghi vào queue
    spin_lock_irqsave(&queue_lock, flags);
    queue_put(&msg);
    spin_unlock_irqrestore(&queue_lock, flags);
    
    // Báo thức consumer
    wake_up_interruptible(&read_wait);
    
    // Schedule lần tiếp theo (3s nữa)
    if (timer_enabled)
        mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));
}
```

**Timer chạy trong context gì?**
```
Process Context: Thread có thể sleep
SoftIRQ Context: Thread KHÔNG THỂ sleep (timer callback ở đây)
Hard IRQ Context: Xử lý ngắt phần cứng ngay lập tức
```

Timer của chúng ta chạy trong **SoftIRQ Context** → không được sleep → phải dùng spinlock.

---

## 3. DATA FLOW - DÒNG DATA

### 3.1 Automatic Producer (Timer)

```
Mỗi 3 giây:
    │
    ▼
┌─────────────────────────────────────────┐
│ Timer fires (kernel)                     │
│ ↓                                        │
│ timer_callback() [SOFTIRQ CONTEXT]      │
│ • Không được sleep                       │
│ • Phải dùng spinlock                     │
└─────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────┐
│ Critical Section (spin_lock_irqsave)    │
│ • Tạo event_msg với timestamp           │
│ • Ghi vào circular queue                │
│ • Cập nhật head pointer                 │
└─────────────────────────────────────────┘
    │
    ▼
┌─────────────────────────────────────────┐
│ wake_up_interruptible(&read_wait)       │
│ • Đánh dấu reader = TASK_RUNNING         │
│ • Scheduler sẽ tỉnh reader               │
└─────────────────────────────────────────┘
```

### 3.2 Manual Producer (User write)

```
User chạy: echo "HELLO" > /dev/pi5_event
    │
    ▼
┌─────────────────────────────────────────┐
│ System call: write()                    │
│ ↓                                        │
│ event_write() [PROCESS CONTEXT]         │
│ • Copy data từ user-space (copy_from_user)│
│ • Tạo event_msg                         │
└─────────────────────────────────────────┘
    │
    ▼
    (Giống timer: queue → wake_up)
```

### 3.3 Consumer (User read)

```
User chạy: cat /dev/pi5_event
    │
    ▼
┌─────────────────────────────────────────┐
│ System call: read()                     │
│ ↓                                        │
│ event_read() [PROCESS CONTEXT]          │
│ • Kiểm tra queue có event không?        │
│   ├─ CÓ → đọc và return                 │
│   └─ KHÔNG → đi ngủ                     │
└─────────────────────────────────────────┘
    │
    ▼ (nếu queue rỗng)
┌─────────────────────────────────────────┐
│ wait_event_interruptible()              │
│ 1. Thread = TASK_INTERRUPTIBLE          │
│ 2. Add vào wait_queue                   │
│ 3. schedule() → CPU context switch      │
│ 4. Thread NGỦ, 0% CPU                  │
└─────────────────────────────────────────┘
    │
    ▼ (khi producer wake)
┌─────────────────────────────────────────┐
│ Thread TỈNH DẤY                         │
│ • Scheduler chọn thread chạy            │
│ • Copy event sang user-space (copy_to_user)│
│ • Return về user-space                  │
└─────────────────────────────────────────┘
```

---

## 4. OS CONCEPTS - CÁC KHÁI NIỆM OS QUAN TRỌNG

### 4.1 Context Switch - Chuyển ngữ cảnh

**Định nghĩa:** CPU chuyển từ thread này sang thread khác.

**Trong project này:**
```
Thread A (reader):
1. Gọi read()
2. Queue rỗng → gọi wait_event_interruptible()
3. Kernel mark A = TASK_INTERRUPTIBLE
4. Gọi schedule() → CONTEXT SWITCH
5. CPU chuyển sang Thread B

... Thread B chạy (có thể là producer) ...

Thread B viết event:
1. Gì vào queue
2. Gọi wake_up_interruptible()
3. Kernel mark A = TASK_RUNNING

... Lúc nào đó scheduler chọn A ...

Thread A:
1. CONTEXT SWITCH (trở lại)
2. A tỉnh dậy, đọc event
3. Return về user-space
```

**Chi phí context switch:**
- Phải save/restore registers
- Flush/refresh cache
- Tốn khoảng 1-10 microsecond

### 4.2 Interrupt Context - Ngữ cảnh ngắt

**Hard IRQ (ngắt phần cứng):**
```
Hardware interrupt (keyboard, network, timer...)
    ↓
CPU dừng công việc hiện tại
    ↓
Nhảy đến ISR (Interrupt Service Routine)
    ↓
Xử lý NGAY LẬP (không thể sleep!)
    ↓
Return
```

**SoftIRQ (ngắt mềm):**
```
Hard IRQ finished
    ↓
Schedule softirq
    ↓ (sau này)
Softirq runs (timer callback của chúng ta)
    ↓
Vẫn không thể sleep!
    ↓
Return
```

**Project demo:**
- Timer callback chạy trong softirq context
- Không thể gọi mutex, kmalloc(GFP_KERNEL), sleep
- Phải dùng spinlock, kmalloc(GFP_ATOMIC)

### 4.3 Synchronization - Đồng bộ hóa

**Problem: Data Race**
```
Thread A (Timer):          Thread B (User write):
queue.head = 5;             queue.head = 6;
queue.count++;              queue.count++;
```

Nếu chạy song song → corrupted data!

**Solution: Spinlock**
```c
spin_lock_irqsave(&queue_lock, flags);
// Chỉ MỘT thread được chạy code ở đây
queue.head = new_head;
queue.count++;
spin_unlock_irqrestore(&queue_lock, flags);
```

**Tại sao không dùng Mutex?**
```
Mutex:
- Thread A: mutex_lock()
- Nếu lock busy → Thread A NGỦ
- Thread B (timer) try lock → CRASH! (không sleep được trong softirq)

Spinlock:
- Thread A: spin_lock()
- Nếu lock busy → Thread A "xoay" chờ (không sleep)
- Thread B (timer) try lock → OK! (vẫn không sleep)
```

### 4.4 Blocking vs Non-blocking I/O

**Blocking I/O:**
```c
// read() block cho đến khi có data
cat /dev/pi5_event
... NGỦ chờ event ...
... Event đến → tỉnh dậy, in ra ...
```

**Non-blocking I/O:**
```c
// read() return ngay lập tức
fd = open("/dev/pi5_event", O_NONBLOCK);
n = read(fd, buf, size);  // Return -EAGAIN nếu không có data
```

**epoll - Blocking I/O multiplexing:**
```c
// Monitor nhiều file descriptors cùng lúc
int epfd = epoll_create1();

// Add /dev/pi5_event vào epoll
epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);

// Chờ BẤT KỲ fd nào có data
epoll_wait(epfd, events, MAX, -1);  // Block ở đây

// Khi có event → epoll_wait return
// Làm sao biết được fd nào có data? → events[i].data.fd
```

**Lợi ích epoll:**
- Monitor hàng nghìn connections
- O(1) complexity (không phụ thuộc số fd)
- 0% CPU khi không có event

---

## 5. KERNEL API - CÁC HÀM KERNEL QUAN TRỌNG

### 5.1 Memory Allocation

```c
// Trong process context (có thể sleep)
ptr = kmalloc(size, GFP_KERNEL);

// Trong interrupt context (KHÔNG sleep)
ptr = kmalloc(size, GFP_ATOMIC);
```

**Project này:** Không dùng kmalloc vì dùng static buffer.

### 5.2 User-Kernel Data Transfer

```c
// Copy từ user-space sang kernel
if (copy_from_user(kernel_buf, user_ptr, size))
    return -EFAULT;

// Copy từ kernel sang user-space
if (copy_to_user(user_ptr, kernel_buf, size))
    return -EFAULT;
```

**Tại sao không dùng memcpy?**
- User pointer có thể invalid → crash kernel
- copy_from_user/check: validate pointer trước khi copy

### 5.3 Device Registration

```c
// Cách đơn giản nhất: miscdevice
misc_register(&event_device);
// Tự động tạo /dev/pi5_event

// Cách phức tạp hơn: cdev_add
// Phải tự alloc device number, create node, v.v.
```

### 5.4 Module Management

```c
module_init(event_init);    // Function chạy khi insmod
module_exit(event_exit);    // Function chạy khi rmmod

MODULE_LICENSE("GPL");       // Bắt buộc cho kernel module
```

---

## 6. BUILD & DEPLOY - BIÊN DỊCH VÀ TRIỂN KHAI

### 6.1 Makefile

```makefile
obj-m += event_driver.o          # Object file cần build

KDIR := /lib/modules/$(shell uname -r)/build  # Kernel source

all:
    $(MAKE) -C $(KDIR) M=$(PWD) modules  # Gọi kernel build system
```

**Compile:**
```bash
make
# Tạo ra event_driver.ko (kernel object)
```

### 6.2 Module Operations

```bash
# Load module vào kernel
sudo insmod event_driver.ko

# Kiểm tra module đã loaded
lsmod | grep event

# Xem kernel logs
dmesg | tail

# Unload module
sudo rmmod event_driver
```

### 6.3 Testing

```bash
# Terminal 1: Consumer
cat /dev/pi5_event
# Sẽ block chờ event

# Terminal 2: Producer
echo "TEST" > /dev/pi5_event
# Terminal 1 sẽ in ra "TEST"
```

---

## 7. TÓM TẮT - KẾT LUẬN

### Project này demo gì?

| OS Concept | Cách demo |
|------------|-----------|
| Interrupt handling | Timer callback trong softirq context |
| Synchronization | Spinlock bảo vệ queue |
| Blocking I/O | Wait queue cho consumer |
| IPC | /dev/pi5_event bridge user-kernel |
| Producer-consumer | Kernel timer + user write |

### Key takeaways:

1. **Kernel code KHÁC user code:**
   - Chạy trong Ring 0 (đặc quyền)
   - Không thể sleep bừa bãi
   - Crash → toàn hệ thống chết

2. **Context switch tốn kém:**
   - Phải save/restore state
   - Nhưng cho phép multiprogramming

3. **Blocking I/O hiệu quả:**
   - Thread ngủ 0% CPU
   - Kernel đánh thức khi có event

4. **Synchronization khó:**
   - Data race → corrupted data
   - Deadlock → system hang
   - Phải chọn đúng primitive (mutex vs spinlock)

### Cho người mới học OS:

Project này = **ví dụ thực tế** của những gì sách dạy:
- Textbook nói "context switch" → project demo nó
- Textbook nói "interrupt context" → project code trong đó
- Textbook nói "blocking I/O" → project implement nó

Học project này → hiểu OS **depth-first** (sâu hơn, practical hơn chỉ đọc sách).

---

## 8. TERMINOLOGY - BẢNG THUẬT NGỮ

| Tiếng Anh | Tiếng Việt |
|-----------|-----------|
| Kernel space | Không gian kernel (Ring 0) |
| User space | Không gian người dùng (Ring 3) |
| System call | Gọi hệ thống |
| Context switch | Chuyển ngữ cảnh |
| Interrupt | Ngắt |
| SoftIRQ | Ngắt mềm |
| Spinlock | Khóa xoay |
| Mutex | Khóa tương hỗ |
| Wait queue | Hàng đợi chờ |
| Blocking I/O | I/O chặn |
| epoll | I/O multiplexing |
| IPC | Giao tiếp giữa processes |
| Producer-consumer | Người sản xuất - người tiêu thụ |
| Data race | Cuộc đua dữ liệu |
| Critical section | Vùng găng |
| Device node | Nút thiết bị |
| Character device | Thiết bị ký tự |
| Kernel module | Mô-đun kernel |
| System panic | Hoảng loạn hệ thống |

---

**Hy vọng giải thích này giúp bạn hiểu project và OS concepts!**

Nếu có thắc mắc, đọc thêm:
- [PROJECT_DOCUMENTATION.md](PROJECT_DOCUMENTATION.md) - Tiếng Anh, chi tiết hơn
- [QUICK_REFERENCE.md](QUICK_REFERENCE.md) - Commands nhanh

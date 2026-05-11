# Flow Hoạt Động Cơ Bản - Đơn Giản Nhất

## PHẦN 1: KERNEL MODULE ĐƯỢC NẠP (INIT)

```
Bạn chạy: sudo insmod event_driver.ko
    ↓
Kernel gọi: event_init()
    ↓
┌──────────────────────────────────────┐
│ 1. Đăng ký device với kernel          │
│    misc_register(&event_device)      │
│    ↓                                 │
│    Kernel tạo: /dev/pi5_event        │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ 2. Khởi tạo queue rỗng               │
│    queue.head = 0                    │
│    queue.tail = 0                    │
│    queue.count = 0                   │
│    (chưa có event nào)               │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ 3. Setup timer (nếu bật)             │
│    timer_enabled = false → timer tắt │
│    timer_enabled = true → timer bật   │
└──────────────────────────────────────┘
    ↓
✅ Module ready! Đợi user input.
```

**Trạng thái ban đầu:**
```
Queue: [EMPTY] (count = 0)
Timer: OFF
Device: /dev/pi5_event tồn tại
```

---

## PHẦN 2: SCENARIO 1 - USER GỬI INPUT (WRITE)

```
Terminal 1: cat /dev/pi5_event (NGỦ chờ data)
Terminal 2: echo "HELLO" > /dev/pi5_event
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 1: System call write()         │
│ echo "HELLO" → bash gọi write()       │
│    ↓                                 │
│ Vào kernel → event_write()           │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 2: Copy data từ user           │
│ copy_from_user(event.data, "HELLO")  │
│ ↓                                    │
│ event_msg = {                        │
│   timestamp: 1234567890 ns           │
│   data: "HELLO"                      │
│ }                                    │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 3: Lock queue                  │
│ spin_lock_irqsave(&queue_lock)      │
│ ↓                                    │
│ Chỉ MỘT thread được truy cập queue  │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 4: Ghi vào queue               │
│ buffer[head] = event_msg             │
│ head = (head + 1) % 32               │
│ count++                              │
│ ↓                                    │
│ Queue: [HELLO] (count = 1)           │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 5: Unlock queue                │
│ spin_unlock_irqrestore(&queue_lock)  │
│ ↓                                    │
│ Thread khác có thể truy cập queue    │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 6: Báo thức consumer           │
│ wake_up_interruptible(&read_wait)    │
│ ↓                                    │
│ Kernel đánh dấu: "Có data rồi!"      │
└──────────────────────────────────────┘
    ↓
✅ Write hoàn tất! Return về user.
```

**Sau write:**
```
Queue: [HELLO] (count = 1)
Consumer: ĐANG NGỦ → kernel sẽ đánh thức
```

---

## PHẦN 3: SCENARIO 2 - CONSUMER ĐỌC DATA (READ)

```
Terminal 1: cat /dev/pi5_event (đang chờ)
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 1: System call read()          │
│ cat gọi read()                       │
│    ↓                                 │
│ Vào kernel → event_read()            │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 2: Kiểm tra queue              │
│ if (queue.count == 0)                │
│    → QUEUE RỖNG!                    │
│    → ĐI NGỦ                          │
│                                      │
│ if (queue.count > 0)                 │
│    → CÓ DATA!                        │
│    → ĐỌC NGAY                        │
└──────────────────────────────────────┘
    ↓
```

### TRƯỜNG HỢP A: QUEUE RỖNG → NGỦ

```
┌──────────────────────────────────────┐
│ BƯỚC 3A: ĐI NGỦ                     │
│ wait_event_interruptible(            │
│   read_wait,                         │
│   queue.count > 0                    │
│ )                                    │
│ ↓                                    │
│ 1. Thread = TASK_INTERRUPTIBLE       │
│ 2. Add thread vào wait_queue         │
│ 3. schedule() → CONTEXT SWITCH       │
│                                      │
│ Thread NGỦ, 0% CPU                   │
│ (chờ producer wake)                  │
└──────────────────────────────────────┘
    ↓
... Chờ ở đây ...
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 4A: Producer GỌI               │
│ ( Sau khi echo "HELLO" > device )    │
│ ↓                                    │
│ Producer gọi:                        │
│   wake_up_interruptible(&read_wait)  │
│ ↓                                    │
│ Kernel đánh dấu:                     │
│   Thread = TASK_RUNNING              │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 5A: Thread TỈNH DẤY            │
│ → Kiểm tra lại: queue.count > 0 ✓    │
│ → Return khỏi wait_event_interruptible│
│ → Tiếp tục BƯỚC 3B                  │
└──────────────────────────────────────┘
    ↓
```

### TRƯỜNG HỢP B: QUEUE CÓ DATA → ĐỌC

```
┌──────────────────────────────────────┐
│ BƯỚC 3B: ĐỌC DATA                   │
│ spin_lock(&queue_lock)               │
│ ↓                                    │
│ event_msg = buffer[tail]             │
│ tail = (tail + 1) % 32               │
│ count--                              │
│ ↓                                    │
│ Queue: [EMPTY] (count = 0)           │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 4B: Copy sang user-space       │
│ copy_to_user(user_buf, &event_msg)   │
│ ↓                                    │
│ User nhận: "HELLO"                   │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 5B: Return về user-space       │
│ cat in ra: "HELLO"                   │
│ ↓                                    │
│ cat lại gọi read() để chờ lần sau    │
└──────────────────────────────────────┘
    ↓
✅ Read hoàn tất!
```

---

## PHẦN 4: SCENARIO 3 - TIMER TỰ ĐỘNG (nếu bật)

```
Module load: sudo insmod event_driver.ko timer_enabled=1
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 1: Setup timer                 │
│ mod_timer(&event_timer, 3 giây)      │
│ ↓                                    │
│ Kernel đếm 3 giây                    │
└──────────────────────────────────────┘
    ↓
... 3 giây trôi qua ...
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 2: Timer FIRED                 │
│ → Kernel gọi timer_callback()        │
│ → Chạy trong SOFTIRQ CONTEXT        │
│   (KHÔNG ĐƯỢC SLEEP!)                │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 3: Tạo event                  │
│ event_msg = {                        │
│   timestamp: now()                   │
│   data: "TIMER_EVENT"                │
│ }                                    │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 4: Ghi vào queue               │
│ (Giống user write)                   │
│ spin_lock_irqsave()                  │
│ queue_put(event_msg)                 │
│ spin_unlock_irqrestore()             │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 5: Báo thức consumer           │
│ wake_up_interruptible(&read_wait)    │
│ ↓                                    │
│ Consumer đang ngủ → sẽ tỉnh          │
└──────────────────────────────────────┘
    ↓
┌──────────────────────────────────────┐
│ BƯỚC 6: Schedule lần tiếp theo       │
│ mod_timer(&event_timer, 3 giây nữa)  │
│ ↓                                    │
│ 3 giây nữa → timer fires again       │
└──────────────────────────────────────┘
    ↓
✅ Lặp lại mãi mãi (cho đến khi unload)
```

---

## PHẦN 5: FULL DEMO FLOW

```
┌─────────────────────────────────────────────────────────────┐
│ BƯỚC 0: INIT                                                │
│ sudo insmod event_driver.ko timer_enabled=1                 │
│ ↓                                                           │
│ - Queue rỗng                                                │
│ - Timer đếm ngược 3 giây                                     │
│ - Device /dev/pi5_event sẵn sàng                             │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ BƯỚC 1: Consumer CHỜA                                      │
│ Terminal 1: cat /dev/pi5_event                              │
│ ↓                                                           │
│ - cat gọi read()                                            │
│ - Queue rỗng → NGỦ (0% CPU)                                │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ BƯỚC 2: Timer TẠO EVENT (tự động sau 3s)                   │
│ ↓                                                           │
│ - Timer callback chạy                                      │
│ - Ghi "TIMER_EVENT" vào queue                               │
│ - Gọi wake_up()                                             │
│ ↓                                                           │
│ Queue: [TIMER_EVENT] (count=1)                              │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ BƯỚC 3: Consumer TỈNH, ĐỌC                                │
│ ↓                                                           │
│ - wait_event_interruptible() return                        │
│ - Đọc "TIMER_EVENT" từ queue                                │
│ - cat in ra "TIMER_EVENT"                                  │
│ ↓                                                           │
│ Queue: [EMPTY] (count=0)                                    │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ BƯỚC 4: Consumer NGỦ LẠI                                   │
│ ↓                                                           │
│ - cat lại gọi read()                                        │
│ - Queue rỗng → NGỦ lại                                      │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ BƯỚC 5: User GỬI MANUAL (song song với timer)              │
│ Terminal 2: echo "BUTTON" > /dev/pi5_event                  │
│ ↓                                                           │
│ - Ghi "BUTTON" vào queue                                    │
│ - Gọi wake_up()                                             │
│ ↓                                                           │
│ Queue: [BUTTON] (count=1)                                   │
└─────────────────────────────────────────────────────────────┘
    ↓
┌─────────────────────────────────────────────────────────────┐
│ BƯỚC 6: Consumer ĐỌC MANUAL                                 │
│ ↓                                                           │
│ - wait_event_interruptible() return                        │
│ - Đọc "BUTTON" từ queue                                     │
│ - cat in ra "BUTTON"                                        │
│ ↓                                                           │
│ Queue: [EMPTY] (count=0)                                    │
└─────────────────────────────────────────────────────────────┘
    ↓
Lặp lại BƯỚC 2-6 mãi mãi...
```

---

## TÓM TẮT VISUAL

```
INIT → Queue rỗng, Timer đếm
  ↓
Consumer NGỦ chờ data
  ↓
Timer FIRED (3s) → Ghi "TIMER_EVENT" → Consumer tỉnh, đọc → NGỦ lại
  ↓
User: echo "HELLO" → Ghi "HELLO" → Consumer tỉnh, đọc → NGỦ lại
  ↓
Timer FIRED (3s) → Ghi "TIMER_EVENT" → Consumer tỉnh, đọc → NGỦ lại
  ↓
... LẶP LẠI ...
```

**KEY POINT:**
- Consumer **LUÔN LUÔN** ngủ khi queue rỗng (0% CPU)
- Producer (timer/user) **ĐÁNH THỨC** consumer khi có data
- Kernel lo tất cả context switch, sleep, wake

---

## DEBUG NHÌN THẤY GÌ?

```bash
# Terminal 1: Consumer
cat /dev/pi5_event
# NGỦ ở đây, chờ data

# Terminal 2: Xem kernel logs
watch -n1 'sudo dmesg | tail -5'
# Thấy:
# - "pi5_event: driver loaded"
# - Khi write: có thể thêm printk debug
```

**Consumer ngủ = Process stuck trong `wait_event_interruptible()`**
**Timer/User write = Gọi `wake_up_interruptible()` → Kernel đánh thức consumer**

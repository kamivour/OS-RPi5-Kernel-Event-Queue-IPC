# DEBUG MODE - Xem Internal Processing

## Vấn đề

Test bằng tay (`echo`) chỉ thấy input/output, **KHÔNG thấy**:
- Queue state (head, tail, count)
- Lock acquired/released
- Thread sleeping/waking
- Timer callback execution

## Giải pháp: Debug Mode

### Build debug version:

```bash
cd rpi5-target
make
# Tạo ra:
# - event_driver.ko (normal version)
# - event_driver_debug.ko (debug version)
```

### Run debug test:

```bash
# Script tự động test
./test_debug.sh

# Hoặc test thủ công:
sudo insmod event_driver_debug.ko

# Ghi log liên tục
watch -n0.5 'sudo dmesg | tail -20'

# Terminal khác: test
echo "TEST" > /dev/pi5_event
cat /dev/pi5_event
```

### Log output sẽ show:

```
[WRITE]: Called with 5 bytes
[WRITE]: Message: 'TEST'
[WRITE]: Acquired lock
[WRITE]: Queue state: head=1 tail=0 count=1  ← Queue state!
[WRITE]: Released lock
[WRITE]: Waking readers
[WRITE]: Write complete

[READ]: Called, count=40
[READ]: Queue count = 1
[READ]: Acquiring lock...
[READ]: Queue state: head=1 tail=1 count=0  ← Sau khi đọc!
[READ]: Released lock
[READ]: Returning 40 bytes to user: 'TEST'

[TIMER]: Callback started                    ← Timer execution!
[TIMER]: Added event, waking readers
[TIMER]: Queue state: head=2 tail=1 count=1
[TIMER]: Callback finished
```

## So sánh

| Version | Log | Dùng khi |
|---------|-----|----------|
| `event_driver.ko` | Minimal | Production, demo |
| `event_driver_debug.ko` | Full printk | Learning, debug |

## Debug Output Meaning

```
Queue state: head=X tail=Y count=Z

head → Vị trí sẽ ghi tiếp
tail → Vị trí sẽ đọc tiếp
count → Số events hiện có
```

**Ví dụ:**
```
head=1 tail=0 count=1  → Có 1 event ở vị trí 0
head=5 tail=3 count=2  → Có 2 events ở vị trí 3,4
head=0 tail=0 count=0  → Queue rỗng
head=5 tail=5 count=32 → Queue đầy (vòng tròn)
```

## When to use debug mode

- **Learning:** Muốn hiểu flow
- **Debugging:** Tìm bug
- **Presentation:** Show internal workings
- **Development:** Test logic mới

Khi OK rồi, dùng normal version cho demo gọn gàng.

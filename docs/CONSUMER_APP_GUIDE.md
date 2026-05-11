# Consumer App Implementation Guide

## Quick Start

Your task: Implement the user-space consumer application that reads events from `/dev/pi5_event` using `epoll`.

## Event Format

```c
// Each event is exactly 40 bytes
struct event_payload {
    long long timestamp;  // 8 bytes - kernel nanosecond timestamp
    char data[32];        // 32 bytes - message payload (null-terminated)
};
```

## Template Code

```cpp
// consumer_app.cpp
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

struct event_payload {
    long long timestamp;
    char data[32];
};

int main() {
    const char* device = "/dev/pi5_event";
    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        std::cerr << "Failed to open " << device << std::endl;
        return 1;
    }

    // Create epoll instance
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        std::cerr << "epoll_create1 failed" << std::endl;
        close(fd);
        return 1;
    }

    // Add device to epoll
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        std::cerr << "epoll_ctl failed" << std::endl;
        close(epfd);
        close(fd);
        return 1;
    }

    std::cout << "Consumer started. Waiting for events..." << std::endl;
    std::cout << "[TIP: Use 'echo TEST > /dev/pi5_event' to trigger]" << std::endl;

    // Event loop - 0% CPU while waiting
    while (1) {
        struct epoll_event events[1];
        int nfds = epoll_wait(epfd, events, 1, -1);  // -1 = block forever

        if (nfds < 0) {
            perror("epoll_wait");
            break;
        }

        if (nfds > 0) {
            struct event_payload event;
            ssize_t n = read(fd, &event, sizeof(event));
            if (n == sizeof(event)) {
                std::cout << "[EVENT] " << event.data << std::endl;
            }
        }
    }

    close(epfd);
    close(fd);
    return 0;
}
```

## Build & Run

```bash
# On RPi5
g++ -o consumer_app consumer_app.cpp
./consumer_app
```

## Test Scenarios

### Scenario 1: Manual Events
```bash
# Terminal 1
./consumer_app

# Terminal 2
echo "BUTTON_A" > /dev/pi5_event
echo "BUTTON_B" > /dev/pi5_event
```

**Expected output:**
```
Consumer started. Waiting for events...
[EVENT] BUTTON_A
[EVENT] BUTTON_B
```

### Scenario 2: Timer Events
```bash
# Terminal 1
./consumer_app

# Terminal 2
sudo insmod event_driver.ko timer_enabled=1
```

**Expected output (every 3 seconds):**
```
Consumer started. Waiting for events...
[EVENT] TIMER_EVENT
[EVENT] TIMER_EVENT
[EVENT] TIMER_EVENT
...
```

### Scenario 3: Mixed
```bash
# Terminal 1
./consumer_app

# Terminal 2
sudo insmod event_driver.ko timer_enabled=1
sleep 5
echo "MANUAL_EVENT" > /dev/pi5_event
```

**Expected output:**
```
[EVENT] TIMER_EVENT
[EVENT] TIMER_EVENT
[EVENT] MANUAL_EVENT
[EVENT] TIMER_EVENT
...
```

## Key Points

1. **Read size must be 40 bytes** - sizeof(event_payload)
2. **epoll_wait blocks** - 0% CPU while waiting
3. **Non-blocking read** - Only read when epoll says ready
4. **Error handling** - Check read return value

## Advanced: Display Timestamp

```cpp
#include <chrono>

// Inside event loop:
auto ts = std::chrono::nanoseconds(event.timestamp);
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(ts);
std::cout << "[" << duration.count() << "μs] " << event.data << std::endl;
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `open: Permission denied` | Device permissions should be 0666 |
| `read: Invalid argument` | Read size must be 40 bytes |
| Hangs forever | No events being produced - check `dmesg` |
| `epoll_wait: Bad file descriptor` | Check fd is valid |
| Garbled output | Wrong struct size - must be exactly 40 bytes |

## Required Files on RPi5

```
/home/user/os-project/
├── event_driver.ko      (kernel module)
├── consumer_app.cpp     (your code)
└── consumer_app         (compiled binary)
```

## For Presentation Demo

1. **Start timer first:**
   ```bash
   sudo insmod event_driver.ko timer_enabled=1
   ```

2. **Then start consumer:**
   ```bash
   ./consumer_app
   ```

3. **Show manual trigger:**
   ```bash
   echo "DEMO_EVENT" > /dev/pi5_event
   ```

4. **Disable timer to show manual-only mode:**
   ```bash
   echo 0 | sudo tee /sys/module/event_driver/parameters/timer_enabled
   ```

// SPDX-License-Identifier: MIT
/*
 * User-space Consumer App using epoll
 * Teammate's task: Complete this for the demo
 *
 * Build: g++ -o consumer_app consumer_app.cpp
 * Usage: ./consumer_app
 */

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

// Kernel module event payload (must match driver)
struct event_payload {
	long long timestamp;  // ktime_t in kernel
	char data[32];
};

int main() {
	const char* device = "/dev/pi5_event";
	int fd, epfd;
	struct epoll_event ev, events[1];
	struct event_payload event;

	// Open device (blocking mode)
	fd = open(device, O_RDONLY);
	if (fd < 0) {
		std::cerr << "Failed to open " << device << std::endl;
		return 1;
	}

	// Setup epoll
	epfd = epoll_create1(0);
	if (epfd < 0) {
		std::cerr << "epoll_create1 failed" << std::endl;
		close(fd);
		return 1;
	}

	ev.events = EPOLLIN;
	ev.data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		std::cerr << "epoll_ctl failed" << std::endl;
		close(epfd);
		close(fd);
		return 1;
	}

	std::cout << "Consumer started. Waiting for events..." << std::endl;
	std::cout << "[TIP: Use 'echo TEST > /dev/pi5_event' to trigger manually]" << std::endl;

	// Event loop - 0% CPU while waiting
	while (1) {
		int nfds = epoll_wait(epfd, events, 1, -1);  // -1 = block forever

		if (nfds < 0) {
			perror("epoll_wait");
			break;
		}

		if (nfds > 0) {
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

// SPDX-License-Identifier: GPL-2.0
/*
 * Kernel Event Queue IPC Driver - DEBUG VERSION with printk
 * Mục đích: Show internal processing để học
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/ktime.h>
#include <linux/uaccess.h>
#include <linux/poll.h>

#define DEVICE_NAME "pi5_event"
#define QUEUE_DEPTH 32
#define MSG_SIZE 32

struct event_msg {
	ktime_t timestamp;
	char data[MSG_SIZE];
};

struct circular_queue {
	struct event_msg buffer[QUEUE_DEPTH];
	int head;
	int tail;
	int count;
};

static struct circular_queue event_queue;
static DEFINE_SPINLOCK(queue_lock);
static DECLARE_WAIT_QUEUE_HEAD(read_wait);

static struct timer_list event_timer;

static bool timer_enabled = false;
module_param(timer_enabled, bool, 0644);
MODULE_PARM_DESC(timer_enabled, "Enable automatic timer events (default: false)");

// Debug function to print queue state
static void print_queue_state(const char *func) {
	printk(KERN_INFO "pi5_event [%s]: queue state: head=%d tail=%d count=%d\n",
	       func, event_queue.head, event_queue.tail, event_queue.count);
}

static int queue_put(struct event_msg *msg) {
	if (event_queue.count >= QUEUE_DEPTH)
		return -ENOSPC;

	event_queue.buffer[event_queue.head] = *msg;
	event_queue.head = (event_queue.head + 1) % QUEUE_DEPTH;
	event_queue.count++;
	return 0;
}

static int queue_get(struct event_msg *msg) {
	if (event_queue.count == 0)
		return -ENODATA;

	*msg = event_queue.buffer[event_queue.tail];
	event_queue.tail = (event_queue.tail + 1) % QUEUE_DEPTH;
	event_queue.count--;
	return 0;
}

static void timer_callback(struct timer_list *t) {
	struct event_msg msg = {
		.timestamp = ktime_get(),
		.data = "TIMER_EVENT"
	};
	unsigned long flags;

	printk(KERN_INFO "pi5_event [TIMER]: Callback started\n");

	spin_lock_irqsave(&queue_lock, flags);

	if (event_queue.count < QUEUE_DEPTH) {
		queue_put(&msg);
		printk(KERN_INFO "pi5_event [TIMER]: Added event, waking readers\n");
		print_queue_state("timer");
		spin_unlock_irqrestore(&queue_lock, flags);
		wake_up_interruptible(&read_wait);

		if (timer_enabled) {
			printk(KERN_INFO "pi5_event [TIMER]: Rescheduling in 3s\n");
			mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));
		}
	} else {
		printk(KERN_WARNING "pi5_event [TIMER]: Queue full, dropping event\n");
		spin_unlock_irqrestore(&queue_lock, flags);
		if (timer_enabled)
			mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));
	}

	printk(KERN_INFO "pi5_event [TIMER]: Callback finished\n");
}

static int event_open(struct inode *inode, struct file *file) {
	printk(KERN_INFO "pi5_event [OPEN]: Device opened\n");
	return 0;
}

static int event_release(struct inode *inode, struct file *file) {
	printk(KERN_INFO "pi5_event [RELEASE]: Device closed\n");
	return 0;
}

static ssize_t event_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos) {
	struct event_msg msg;
	int ret;

	printk(KERN_INFO "pi5_event [READ]: Called, count=%zu\n", count);

	if (count < sizeof(msg)) {
		printk(KERN_ERR "pi5_event [READ]: Invalid count\n");
		return -EINVAL;
	}

	printk(KERN_INFO "pi5_event [READ]: Queue count = %d\n", event_queue.count);

	if (event_queue.count == 0) {
		if (file->f_flags & O_NONBLOCK) {
			printk(KERN_INFO "pi5_event [READ]: Non-blocking, return EAGAIN\n");
			return -EAGAIN;
		}
		printk(KERN_INFO "pi5_event [READ]: Queue empty, sleeping...\n");
		ret = wait_event_interruptible(read_wait, event_queue.count > 0);
		if (ret) {
			printk(KERN_INFO "pi5_event [READ]: Woken by signal\n");
			return ret;
		}
		printk(KERN_INFO "pi5_event [READ]: Woken up! Queue has data\n");
	}

	spin_lock(&queue_lock);
	printk(KERN_INFO "pi5_event [READ]: Acquiring lock...\n");
	ret = queue_get(&msg);
	print_queue_state("read_after_get");
	spin_unlock(&queue_lock);
	printk(KERN_INFO "pi5_event [READ]: Released lock\n");

	if (ret) {
		printk(KERN_ERR "pi5_event [READ]: Queue get failed\n");
		return -EIO;
	}

	if (copy_to_user(buf, &msg, sizeof(msg))) {
		printk(KERN_ERR "pi5_event [READ]: copy_to_user failed\n");
		return -EFAULT;
	}

	printk(KERN_INFO "pi5_event [READ]: Returning %d bytes to user: '%s'\n",
	       (int)sizeof(msg), msg.data);
	return sizeof(msg);
}

static __poll_t event_poll(struct file *file, poll_table *wait) {
	__poll_t mask = 0;

	printk(KERN_INFO "pi5_event [POLL]: Called\n");

	poll_wait(file, &read_wait, wait);
	spin_lock(&queue_lock);
	if (event_queue.count > 0) {
		mask |= EPOLLIN | EPOLLRDNORM;
		printk(KERN_INFO "pi5_event [POLL]: Data available\n");
	} else {
		printk(KERN_INFO "pi5_event [POLL]: No data\n");
	}
	spin_unlock(&queue_lock);

	return mask;
}

static ssize_t event_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos) {
	struct event_msg msg;
	unsigned long flags;
	int ret;

	printk(KERN_INFO "pi5_event [WRITE]: Called with %zu bytes\n", count);

	if (count >= MSG_SIZE) {
		printk(KERN_ERR "pi5_event [WRITE]: Message too long\n");
		return -EMSGSIZE;
	}

	memset(&msg, 0, sizeof(msg));
	msg.timestamp = ktime_get();

	if (copy_from_user(msg.data, buf, count)) {
		printk(KERN_ERR "pi5_event [WRITE]: copy_from_user failed\n");
		return -EFAULT;
	}
	msg.data[count] = '\0';

	printk(KERN_INFO "pi5_event [WRITE]: Message: '%s'\n", msg.data);

	spin_lock_irqsave(&queue_lock, flags);
	printk(KERN_INFO "pi5_event [WRITE]: Acquired lock\n");

	if (event_queue.count >= QUEUE_DEPTH) {
		printk(KERN_WARNING "pi5_event [WRITE]: Queue full!\n");
		spin_unlock_irqrestore(&queue_lock, flags);
		return -ENOSPC;
	}

	ret = queue_put(&msg);
	print_queue_state("write");
	spin_unlock_irqrestore(&queue_lock, flags);
	printk(KERN_INFO "pi5_event [WRITE]: Released lock\n");

	if (ret != 0) {
		printk(KERN_ERR "pi5_event [WRITE]: Queue put failed\n");
		return -EIO;
	}

	printk(KERN_INFO "pi5_event [WRITE]: Waking readers\n");
	wake_up_interruptible(&read_wait);
	printk(KERN_INFO "pi5_event [WRITE]: Write complete\n");

	return count;
}

static const struct file_operations event_fops = {
	.owner		= THIS_MODULE,
	.open		= event_open,
	.release	= event_release,
	.read		= event_read,
	.write		= event_write,
	.poll		= event_poll,
};

static struct miscdevice event_device = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &event_fops,
	.mode	= 0666,
};

static int __init event_init(void) {
	int ret;

	printk(KERN_INFO "pi5_event [INIT]: Module loading...\n");

	ret = misc_register(&event_device);
	if (ret) {
		printk(KERN_ERR "pi5_event [INIT]: Failed to register device\n");
		return ret;
	}

	memset(&event_queue, 0, sizeof(event_queue));
	timer_setup(&event_timer, timer_callback, 0);

	if (timer_enabled) {
		mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));
		printk(KERN_INFO "pi5_event [INIT]: Timer enabled\n");
	}

	printk(KERN_INFO "pi5_event [INIT]: Driver loaded, /dev/%s created\n", DEVICE_NAME);
	printk(KERN_INFO "pi5_event [INIT]: Queue: head=%d tail=%d count=%d\n",
	       event_queue.head, event_queue.tail, event_queue.count);
	return 0;
}

static void __exit event_exit(void) {
	printk(KERN_INFO "pi5_event [EXIT]: Module unloading...\n");

	timer_enabled = false;
	del_timer_sync(&event_timer);
	printk(KERN_INFO "pi5_event [EXIT]: Timer stopped\n");

	wake_up_interruptible(&read_wait);
	printk(KERN_INFO "pi5_event [EXIT]: Woken all sleeping readers\n");

	kfifo_reset(&event_queue);
	printk(KERN_INFO "pi5_event [EXIT]: Queue reset\n");

	misc_deregister(&event_device);
	printk(KERN_INFO "pi5_event [EXIT]: Device unregistered\n");

	printk(KERN_INFO "pi5_event [EXIT]: Final queue state: head=%d tail=%d count=%d\n",
	       event_queue.head, event_queue.tail, event_queue.count);
	printk(KERN_INFO "pi5_event [EXIT]: Driver unloaded safely\n");
}

module_init(event_init);
module_exit(event_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OS Project");
MODULE_DESCRIPTION("Kernel Event Queue IPC - Debug Version with printk");

// SPDX-License-Identifier: GPL-2.0
/*
 * Kernel Event Queue IPC Driver - Full Version with Timer
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

	spin_lock_irqsave(&queue_lock, flags);

	if (event_queue.count < QUEUE_DEPTH) {
		queue_put(&msg);
		spin_unlock_irqrestore(&queue_lock, flags);
		wake_up_interruptible(&read_wait);

		if (timer_enabled)
			mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));
	} else {
		spin_unlock_irqrestore(&queue_lock, flags);
		printk(KERN_WARNING "pi5_event: queue full, dropping timer event\n");
		if (timer_enabled)
			mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));
	}
}

static int event_open(struct inode *inode, struct file *file) {
	return 0;
}

static int event_release(struct inode *inode, struct file *file) {
	return 0;
}

static ssize_t event_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos) {
	struct event_msg msg;
	int ret;

	if (count < sizeof(msg))
		return -EINVAL;

	if (event_queue.count == 0) {
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN;
		ret = wait_event_interruptible(read_wait, event_queue.count > 0);
		if (ret)
			return ret;
	}

	spin_lock(&queue_lock);
	ret = queue_get(&msg);
	spin_unlock(&queue_lock);

	if (ret)
		return -EIO;

	if (copy_to_user(buf, &msg, sizeof(msg)))
		return -EFAULT;

	return sizeof(msg);
}

static __poll_t event_poll(struct file *file, poll_table *wait) {
	__poll_t mask = 0;

	poll_wait(file, &read_wait, wait);
	spin_lock(&queue_lock);
	if (event_queue.count > 0)
		mask |= EPOLLIN | EPOLLRDNORM;
	spin_unlock(&queue_lock);

	return mask;
}

static ssize_t event_write(struct file *file, const char __user *buf,
			   size_t count, loff_t *ppos) {
	struct event_msg msg;
	unsigned long flags;

	if (count >= MSG_SIZE)
		return -EMSGSIZE;

	memset(&msg, 0, sizeof(msg));
	msg.timestamp = ktime_get();

	if (copy_from_user(msg.data, buf, count))
		return -EFAULT;
	msg.data[count] = '\0';

	spin_lock_irqsave(&queue_lock, flags);

	if (event_queue.count >= QUEUE_DEPTH) {
		spin_unlock_irqrestore(&queue_lock, flags);
		return -ENOSPC;
	}

	queue_put(&msg);
	spin_unlock_irqrestore(&queue_lock, flags);

	wake_up_interruptible(&read_wait);
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

	ret = misc_register(&event_device);
	if (ret) {
		printk(KERN_ERR "pi5_event: failed to register misc device\n");
		return ret;
	}

	memset(&event_queue, 0, sizeof(event_queue));
	timer_setup(&event_timer, timer_callback, 0);

	if (timer_enabled) {
		mod_timer(&event_timer, jiffies + msecs_to_jiffies(3000));
		printk(KERN_INFO "pi5_event: timer enabled\n");
	}

	printk(KERN_INFO "pi5_event: driver loaded, /dev/%s created\n", DEVICE_NAME);
	return 0;
}

static void __exit event_exit(void) {
	timer_enabled = false;
	del_timer_sync(&event_timer);
	wake_up_interruptible(&read_wait);
	memset(&event_queue, 0, sizeof(event_queue));
	misc_deregister(&event_device);

	printk(KERN_INFO "pi5_event: driver unloaded safely\n");
}

module_init(event_init);
module_exit(event_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OS Demo Project");
MODULE_DESCRIPTION("Kernel Event Queue IPC - Full Version with Timer");

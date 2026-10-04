// SPDX-License-Identifier: GPL-2.0
/*
 * envmon_drv.c - EnvMon: virtual environmental-sensor character driver.
 *
 * A kernel timer generates simulated temperature / humidity samples (bounded
 * random walk) into a power-of-two ring buffer.  User space consumes them via
 * /dev/envmon using read(), poll() and ioctl().  Statistics are exposed in
 * /proc/envmon.
 *
 * Concepts demonstrated: misc char device, file_operations, kernel timers,
 * spinlocks (IRQ-safe, timer runs in softirq), wait queues, poll(),
 * copy_to_user()/copy_from_user(), ioctl, module parameters, procfs/seq_file.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include "envmon_ioctl.h"

#define RING_SIZE 256U                 /* must be a power of two */
#define RING_MASK (RING_SIZE - 1U)

#define TEMP_MIN_mC   15000
#define TEMP_MAX_mC   45000
#define HUM_MIN_mpct  20000
#define HUM_MAX_mpct  90000

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 15, 0)
#define envmon_timer_sync_delete(t) timer_delete_sync(t)
#else
#define envmon_timer_sync_delete(t) del_timer_sync(t)
#endif

static unsigned int period_ms = 500;
module_param(period_ms, uint, 0444);
MODULE_PARM_DESC(period_ms, "Initial sampling period in ms (10..60000)");

struct envmon_dev {
	struct miscdevice  misc;
	struct timer_list  timer;
	spinlock_t         lock;          /* protects everything below */
	wait_queue_head_t  wq;
	struct envmon_sample ring[RING_SIZE];
	u32  head;                        /* next write index (free-running) */
	u32  tail;                        /* next read index  (free-running) */
	u32  period;
	u32  seq;
	u64  generated;
	u64  dropped;
	s32  temp_mC;
	s32  hum_mpct;
	u32  rng;
	bool stopping;
	bool overrun_pending;
};

static struct envmon_dev *edev;

static inline u32 ring_count(const struct envmon_dev *d)
{
	return d->head - d->tail;
}

static inline u32 xorshift32(u32 *s)
{
	u32 x = *s;

	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	*s = x;
	return x;
}

static inline s32 clamp_s32(s32 v, s32 lo, s32 hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

static void envmon_timer_fn(struct timer_list *t)
{
	struct envmon_dev *d = from_timer(d, t, timer);
	struct envmon_sample s;
	unsigned long flags;

	spin_lock_irqsave(&d->lock, flags);

	d->temp_mC  = clamp_s32(d->temp_mC +
				(s32)(xorshift32(&d->rng) % 301U) - 150,
				TEMP_MIN_mC, TEMP_MAX_mC);
	d->hum_mpct = clamp_s32(d->hum_mpct +
				(s32)(xorshift32(&d->rng) % 601U) - 300,
				HUM_MIN_mpct, HUM_MAX_mpct);

	s.timestamp_ns  = ktime_get_ns();
	s.temp_mC       = d->temp_mC;
	s.humidity_mpct = d->hum_mpct;
	s.seq           = d->seq++;
	s.flags         = 0;

	if (ring_count(d) == RING_SIZE) {      /* full: overwrite oldest */
		d->tail++;
		d->dropped++;
		d->overrun_pending = true;
	}
	if (d->overrun_pending) {
		s.flags |= ENVMON_F_OVERRUN;
		d->overrun_pending = false;
	}
	d->ring[d->head & RING_MASK] = s;
	d->head++;
	d->generated++;

	if (!d->stopping)
		mod_timer(&d->timer, jiffies + msecs_to_jiffies(d->period));

	spin_unlock_irqrestore(&d->lock, flags);

	wake_up_interruptible(&d->wq);
}

static int envmon_open(struct inode *inode, struct file *filp)
{
	filp->private_data = edev;
	return nonseekable_open(inode, filp);
}

static ssize_t envmon_read(struct file *filp, char __user *buf,
			   size_t len, loff_t *ppos)
{
	struct envmon_dev *d = filp->private_data;
	size_t want = len / sizeof(struct envmon_sample);
	size_t done = 0;

	if (want == 0)
		return -EINVAL;

	for (;;) {
		struct envmon_sample s;
		unsigned long flags;

		spin_lock_irqsave(&d->lock, flags);
		if (ring_count(d) == 0) {
			spin_unlock_irqrestore(&d->lock, flags);
			if (done)
				break;                       /* return what we have */
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;
			if (wait_event_interruptible(d->wq,
					READ_ONCE(d->head) != READ_ONCE(d->tail)))
				return -ERESTARTSYS;
			continue;                            /* re-check under lock */
		}
		s = d->ring[d->tail & RING_MASK];
		d->tail++;
		spin_unlock_irqrestore(&d->lock, flags);

		/* copy_to_user() may sleep: never call it with the spinlock held */
		if (copy_to_user(buf + done * sizeof(s), &s, sizeof(s)))
			return done ? (ssize_t)(done * sizeof(s)) : -EFAULT;
		if (++done == want)
			break;
	}
	return done * sizeof(struct envmon_sample);
}

static __poll_t envmon_poll(struct file *filp, poll_table *wait)
{
	struct envmon_dev *d = filp->private_data;
	__poll_t mask = 0;

	poll_wait(filp, &d->wq, wait);
	if (READ_ONCE(d->head) != READ_ONCE(d->tail))
		mask |= EPOLLIN | EPOLLRDNORM;
	return mask;
}

static long envmon_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct envmon_dev *d = filp->private_data;
	void __user *uarg = (void __user *)arg;
	unsigned long flags;

	if (_IOC_TYPE(cmd) != ENVMON_IOC_MAGIC)
		return -ENOTTY;

	switch (cmd) {
	case ENVMON_IOC_SET_PERIOD: {
		u32 p;

		if (copy_from_user(&p, uarg, sizeof(p)))
			return -EFAULT;
		if (p < ENVMON_MIN_PERIOD_MS || p > ENVMON_MAX_PERIOD_MS)
			return -EINVAL;
		spin_lock_irqsave(&d->lock, flags);
		d->period = p;
		mod_timer(&d->timer, jiffies + msecs_to_jiffies(p));
		spin_unlock_irqrestore(&d->lock, flags);
		return 0;
	}
	case ENVMON_IOC_GET_PERIOD: {
		u32 p;

		spin_lock_irqsave(&d->lock, flags);
		p = d->period;
		spin_unlock_irqrestore(&d->lock, flags);
		return copy_to_user(uarg, &p, sizeof(p)) ? -EFAULT : 0;
	}
	case ENVMON_IOC_RESET:
		spin_lock_irqsave(&d->lock, flags);
		d->tail = d->head = 0;
		d->seq = 0;
		d->generated = d->dropped = 0;
		d->overrun_pending = false;
		spin_unlock_irqrestore(&d->lock, flags);
		return 0;
	case ENVMON_IOC_GET_STATS: {
		struct envmon_stats st;

		spin_lock_irqsave(&d->lock, flags);
		st.generated  = d->generated;
		st.dropped    = d->dropped;
		st.buf_depth  = ring_count(d);
		st.period_ms  = d->period;
		spin_unlock_irqrestore(&d->lock, flags);
		return copy_to_user(uarg, &st, sizeof(st)) ? -EFAULT : 0;
	}
	default:
		return -ENOTTY;
	}
}

static const struct file_operations envmon_fops = {
	.owner          = THIS_MODULE,
	.open           = envmon_open,
	.read           = envmon_read,
	.poll           = envmon_poll,
	.unlocked_ioctl = envmon_ioctl,
	.compat_ioctl   = envmon_ioctl,
	.llseek         = no_llseek,
};

static int envmon_proc_show(struct seq_file *m, void *v)
{
	struct envmon_dev *d = edev;
	unsigned long flags;
	u64 gen, drop;
	u32 depth, per;
	s32 t, h;

	spin_lock_irqsave(&d->lock, flags);
	gen = d->generated; drop = d->dropped;
	depth = ring_count(d); per = d->period;
	t = d->temp_mC; h = d->hum_mpct;
	spin_unlock_irqrestore(&d->lock, flags);

	seq_printf(m, "period_ms:  %u\ngenerated:  %llu\ndropped:    %llu\n"
		      "buf_depth:  %u/%u\nlast_temp:  %d mC\nlast_hum:   %d m%%\n",
		   per, gen, drop, depth, RING_SIZE, t, h);
	return 0;
}

static int __init envmon_init(void)
{
	struct envmon_dev *d;
	int ret;

	if (period_ms < ENVMON_MIN_PERIOD_MS || period_ms > ENVMON_MAX_PERIOD_MS) {
		pr_err("envmon: period_ms %u out of range\n", period_ms);
		return -EINVAL;
	}
	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	spin_lock_init(&d->lock);
	init_waitqueue_head(&d->wq);
	d->period   = period_ms;
	d->temp_mC  = 25000;
	d->hum_mpct = 50000;
	d->rng      = (u32)ktime_get_ns() | 1U;     /* xorshift needs non-zero */

	d->misc.minor = MISC_DYNAMIC_MINOR;
	d->misc.name  = ENVMON_DEVICE_NAME;
	d->misc.fops  = &envmon_fops;
	d->misc.mode  = 0444;
	edev = d;

	ret = misc_register(&d->misc);
	if (ret) {
		pr_err("envmon: misc_register failed (%d)\n", ret);
		kfree(d);
		return ret;
	}
	if (!proc_create_single("envmon", 0444, NULL, envmon_proc_show))
		pr_warn("envmon: could not create /proc/envmon\n");

	timer_setup(&d->timer, envmon_timer_fn, 0);
	mod_timer(&d->timer, jiffies + msecs_to_jiffies(d->period));

	pr_info("envmon: loaded, period=%u ms, /dev/%s\n", d->period,
		ENVMON_DEVICE_NAME);
	return 0;
}

static void __exit envmon_exit(void)
{
	struct envmon_dev *d = edev;
	unsigned long flags;

	spin_lock_irqsave(&d->lock, flags);
	d->stopping = true;                    /* timer will not re-arm itself */
	spin_unlock_irqrestore(&d->lock, flags);
	envmon_timer_sync_delete(&d->timer);

	remove_proc_entry("envmon", NULL);
	misc_deregister(&d->misc);
	kfree(d);
	pr_info("envmon: unloaded\n");
}

module_init(envmon_init);
module_exit(envmon_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EnvMon Project");
MODULE_DESCRIPTION("EnvMon virtual environmental sensor character driver");
MODULE_VERSION("1.0");

/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * envmon_ioctl.h - Kernel/user shared ABI for the EnvMon virtual sensor driver.
 * Included by both driver/envmon_drv.c and the C++ user-space application.
 */
#ifndef ENVMON_IOCTL_H
#define ENVMON_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define ENVMON_DEVICE_NAME   "envmon"
#define ENVMON_MIN_PERIOD_MS 10U
#define ENVMON_MAX_PERIOD_MS 60000U

/* flags in envmon_sample.flags */
#define ENVMON_F_OVERRUN     (1U << 0)  /* older samples were dropped before this one */

/* One sensor reading. Fixed-point: milli-degrees Celsius, milli-percent RH. */
struct envmon_sample {
	__u64 timestamp_ns;   /* CLOCK_MONOTONIC-based (ktime_get_ns) */
	__s32 temp_mC;        /* e.g. 25500 == 25.5 C */
	__s32 humidity_mpct;  /* e.g. 48250 == 48.25 %RH */
	__u32 seq;            /* monotonically increasing sample number */
	__u32 flags;
};

struct envmon_stats {
	__u64 generated;      /* samples produced since load / reset */
	__u64 dropped;        /* samples overwritten because the ring was full */
	__u32 buf_depth;      /* samples currently waiting in the ring */
	__u32 period_ms;      /* current sampling period */
};

#define ENVMON_IOC_MAGIC       'E'
#define ENVMON_IOC_SET_PERIOD  _IOW(ENVMON_IOC_MAGIC, 1, __u32)
#define ENVMON_IOC_GET_PERIOD  _IOR(ENVMON_IOC_MAGIC, 2, __u32)
#define ENVMON_IOC_RESET       _IO(ENVMON_IOC_MAGIC, 3)
#define ENVMON_IOC_GET_STATS   _IOR(ENVMON_IOC_MAGIC, 4, struct envmon_stats)

#endif /* ENVMON_IOCTL_H */

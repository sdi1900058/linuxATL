/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI_LINUX_K22INFO_H
#define _UAPI_LINUX_K22INFO_H

#include <linux/types.h>

struct k22info {
	char comm[64];                  /* executable name */
	pid_t pid;                      /* process ID */
	pid_t parent_pid;               /* parent process ID */
	pid_t first_child_pid;          /* youngest child PID */
	pid_t next_sibling_pid;         /* oldest sibling PID */
	unsigned long nvcsw;            /* voluntary context switches */
	unsigned long nivcsw;           /* involuntary context switches */
	unsigned long start_time;       /* monotonic start time in nanoseconds */
};

#endif /* _UAPI_LINUX_K22INFO_H */

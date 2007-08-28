/*
 * arch/e1nommu/kernel/ptrace.c
 *
 *  Copyright (C) 2002-2003,    George Thanos <george.thanos@gdt.gr>
 *                              Yannis Mitsos <yannis.mitsos@gdt.gr>
 */

#include <linux/sched.h>

/*
 * Called by kernel/ptrace.c when detaching..
 * Make sure the single step bit is not set.
 */
void ptrace_disable(struct task_struct *child) { }

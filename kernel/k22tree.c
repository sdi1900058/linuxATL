// SPDX-License-Identifier: GPL-2.0
/*
 * k22tree.c - System call to traverse process tree in DFS order
 *
 * This system call traverses the complete process tree and exposes
 * process-specific information in Depth-First Search order.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/k22info.h>
#include <linux/pid.h>
#include <linux/rcupdate.h>
#include <linux/list.h>
#include <linux/slab.h>

/* Get the first child of a process */
static struct task_struct *get_first_child(struct task_struct *parent)
{
	struct task_struct *child;

	if (list_empty(&parent->children))
		return NULL;
	child = list_first_entry(&parent->children, struct task_struct, sibling);
	return child;
}

/* Get the next sibling of a process */
static struct task_struct *get_next_sibling(struct task_struct *task)
{
	struct task_struct *next;

	if (!task->parent || list_is_last(&task->sibling, &task->parent->children))
		return NULL;
	next = list_next_entry(task, sibling);
	return next;
}

/* Get the oldest sibling (first in parent's children list) */
static struct task_struct *get_oldest_sibling(struct task_struct *task)
{
	struct task_struct *sibling;

	if (!task->parent || list_empty(&task->parent->children))
		return NULL;
	sibling = list_first_entry(&task->parent->children, struct task_struct, sibling);
	return sibling;
}

/* Get the youngest child (last in children list) */
static struct task_struct *get_youngest_child(struct task_struct *parent)
{
	struct task_struct *child;

	if (list_empty(&parent->children))
		return NULL;
	child = list_last_entry(&parent->children, struct task_struct, sibling);
	return child;
}

/* Fill k22info structure with process information */
static void fill_k22info(struct k22info *info, struct task_struct *task)
{
	struct task_struct *first_child, *oldest_sibling;

	strscpy(info->comm, task->comm, sizeof(info->comm));
	info->pid = task_pid_nr(task);
	info->parent_pid = task->parent ? task_pid_nr(task->parent) : 0;
	info->nvcsw = task->nvcsw;
	info->nivcsw = task->nivcsw;
	info->start_time = task->start_time;

	/* Get first child (youngest) */
	first_child = get_youngest_child(task);
	info->first_child_pid = first_child ? task_pid_nr(first_child) : 0;

	/* Get oldest sibling */
	oldest_sibling = get_next_sibling(task);
	info->next_sibling_pid = oldest_sibling ? task_pid_nr(oldest_sibling) : 0;
}

/*
 * Iterative DFS traversal using an explicit stack
 * This avoids recursion and prevents potential stack overflow
 */
static int dfs_traverse_iterative(struct task_struct *root, struct k22info *buf,
				  int max_entries, int *count)
{
	struct task_struct **stack;
	int stack_top = 0;
	int stack_size = 256; /* Initial stack size */
	struct task_struct *current, *child;
	int ret = 0;

	/* Allocate stack for DFS traversal */
	stack = kmalloc_array(stack_size, sizeof(struct task_struct *), GFP_ATOMIC);
	if (!stack)
		return -ENOMEM;

	/* Push root onto stack */
	stack[stack_top++] = root;

	while (stack_top > 0) {
		/* Pop current task from stack */
		current = stack[--stack_top];

		/* Process current task if it's a thread group leader */
		if (thread_group_leader(current)) {
			if (*count < max_entries)
				fill_k22info(&buf[*count], current);
			(*count)++;
		}

		/* Push children onto stack in reverse order (to maintain DFS order) */
		list_for_each_entry_reverse(child, &current->children, sibling) {
			/* Check if we need to expand the stack */
			if (stack_top >= stack_size) {
				struct task_struct **new_stack;
				int new_size = stack_size * 2;

				new_stack = krealloc(stack, new_size * sizeof(struct task_struct *),
						     GFP_ATOMIC);
				if (!new_stack) {
					ret = -ENOMEM;
					goto out_free;
				}
				stack = new_stack;
				stack_size = new_size;
			}

			/* Push child onto stack */
			stack[stack_top++] = child;
		}
	}

out_free:
	kfree(stack);
	return ret;
}

/* System call implementation */
SYSCALL_DEFINE2(k22tree, struct k22info __user *, buf, int __user *, ne)
{
	struct k22info *kern_buf;
	int num_entries, total_processes = 0;
	int ret = 0;
	struct task_struct *task;

	/* Input validation */
	if (!buf || !ne)
		return -EINVAL;
	if (get_user(num_entries, ne))
		return -EFAULT;
	if (num_entries < 1)
		return -EINVAL;

	/* Allocate kernel buffer outside of lock */
	kern_buf = kmalloc_array(num_entries, sizeof(struct k22info), GFP_KERNEL);
	if (!kern_buf)
		return -ENOMEM;

	/* DFS traversal with tasklist_lock held */
	read_lock(&tasklist_lock);
	task = &init_task;
	ret = dfs_traverse_iterative(task, kern_buf, num_entries, &total_processes);
	read_unlock(&tasklist_lock);

	if (ret) {
		kfree(kern_buf);
		return ret;
	}

	/* Copy data to user space (no lock held) */
	if (copy_to_user(buf, kern_buf, min(total_processes, num_entries) *
			 sizeof(struct k22info))) {
		kfree(kern_buf);
		return -EFAULT;
	}

	/* Update number of entries written */
	if (put_user(min(total_processes, num_entries), ne)) {
		kfree(kern_buf);
		return -EFAULT;
	}

	kfree(kern_buf);
	return total_processes;
}

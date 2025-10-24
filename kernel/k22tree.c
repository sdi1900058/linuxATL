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

//get the first child of the process
static struct task_struct *get_first_child(struct task_struct *parent)
{
	struct task_struct *child;
	if(list_empty(&parent->children))
		return NULL;
	child=list_first_entry(&parent->children,struct task_struct,sibling);
	return child;
}
//get the next sibling
static struct task_struct *get_next_sibling(struct task_struct *task)
{
	struct task_struct *next;
	
	if (!task->pare||list_is_last(&task->sibling, &task->parent->children))return NULL;
	next=list_next_entry(task,sibling);
	return next;
}

//oldest sibling get
static struct task_struct *get_oldest_sibling(struct task_struct *task)
{
	struct task_struct *sibling;
	if(!task->parent||list_empty(&task->parent->children))return NULL;
	sibling=list_first_entry(&task->parent->children, struct task_struct,sibling);
	return sibling;
}
//younged child
static struct task_struct *get_youngest_child(struct task_struct *parent)
{
	struct task_struct *child;
	if(list_empty(&parent->children))return NULL;
	child=list_last_entry(&parent->children,struct task_struct,sibling);
	return child;
}

// using getters to fill the info asked
static void fill_k22info(struct k22info *info,struct task_struct *task)
{
	struct task_struct *first_child,*oldest_sibling;
	strncpy(info->comm,task->comm,sizeof(info->comm)-1);
	info->comm[sizeof(info->comm)-1]='\0';
	info->pid=task_pid_nr(task);
	info->parent_pid=task->parent?task_pid_nr(task->parent):0;
	info->nvcsw=task->nvcsw;
	info->nivcsw=task->nivcsw;
	info->start_time=task->start_time;
	//first child
	first_child =get_first_child(task);
	info->first_child_pid = first_child?task_pid_nr(first_child):0;
	//oldest
	oldest_sibling=get_oldest_sibling(task);
	info->next_sibling_pid=oldest_sibling?task_pid_nr(oldest_sibling):0;
}

//dfs recursive implemented
static int dfs_traverse_recursive(struct task_struct *task,struct k22info *buf,int max_entries,int *count)
{
	struct task_struct *child;
	if (*count>=max_entries)return 0;
	//if(thread group leader)task processed
	if (thread_group_leader(task)){
		fill_k22info(&buf[*count],task);
		(*count)++;
	}
	
	//recurse for each child
	list_for_each_entry(child,&task->children,sibling){
		if (*count>=max_entries)
			break;
		dfs_traverse_recursive(child,buf,max_entries,count);
	}
	return 0;
}

//sys call main
SYSCALL_DEFINE2(k22tree,struct k22info __user *,buf,int __user *,ne)
{
	struct k22info *kern_buf;
	int num_entries,total_processes=0;
	int ret=0;
	struct task_struct *task;
	
	//input validation
	//TODO: check for overflow in num_entries
	//TODO: idk what else i should check
	if (!buf || !ne)return -EINVAL;
	if (get_user(num_entries, ne))return -EFAULT;
	if (num_entries < 1)return -EINVAL;

	//kernel buffer alloc
	kern_buf=kmalloc_array(num_entries,sizeof(struct k22info),GFP_KERNEL);
	if (!kern_buf)return -ENOMEM;
	//dfs process tree traversal
	read_lock(&tasklist_lock);
	task=&init_task;
	total_processes=0;
	ret=dfs_traverse_recursive(task,kern_buf,num_entries,&total_processes);
	read_unlock(&tasklist_lock);
	if (ret){
		kfree(kern_buf);
		return ret;
	}
	//copy data to user
	if (copy_to_user(buf,kern_buf,total_processes * sizeof(struct k22info))){
		kfree(kern_buf);
		return -EFAULT;
	}
	//entry number updated
	if (put_user(total_processes,ne)){
		kfree(kern_buf);
		return -EFAULT;
	}
	kfree(kern_buf);
	return total_processes;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/k22info.h>

#define K22TREE_SYSCALL 467

static int find_parent(int *stack, int top, int parent_pid) {
    int i;
    for (i = 0; i <= top; i++) {
        if (stack[i] == parent_pid) {
            return i;
        }
    }
    return -1;
}

int main() {
    struct k22info *buf;
    int *stack;
    int size = 100;  /* Start with reasonable buffer size */
    int actual_size;
    int ret;
    int i, j, p_pos;
    struct k22info *p;
    
    printf("- User-space buf. size: %d\n", size);
    
    /* Allocate buffer */
    buf = malloc(size * sizeof(struct k22info));
    if (!buf) {
        perror("malloc");
        return 1;
    }
    
    /* Call syscall */
    ret = syscall(K22TREE_SYSCALL, buf, &size);
    if (ret < 0) {
        perror("k22tree syscall");
        free(buf);
        return 1;
    }
    
    printf("- syscall return val:   %d\n", ret);
    
    /* If we got fewer entries than buffer size, we have all data */
    if (ret <= size) {
        printf("--- OK ---\n");
        actual_size = ret;
    } else {
        /* Need larger buffer, double the size and try again */
        free(buf);
        size *= 2;
        printf("- User-space buf. size: %d\n", size);
        
        buf = malloc(size * sizeof(struct k22info));
        if (!buf) {
            perror("malloc");
            return 1;
        }
        
        ret = syscall(K22TREE_SYSCALL, buf, &size);
        if (ret < 0) {
            perror("k22tree syscall");
            free(buf);
            return 1;
        }
        
        printf("- syscall return val:   %d\n", ret);
        printf("--- OK ---\n");
        actual_size = ret;
    }
    
    /* Allocate stack for depth calculation */
    stack = malloc(actual_size * sizeof(int));
    if (!stack) {
        perror("malloc");
        free(buf);
        return 1;
    }
    
    /* Print header */
    printf("#comm,pid,ppid,fcldpid,nsblpid,nvcsw,nivcsw,stime\n");
    
    /* Print first entry (root) */
    p = &buf[0];
    printf("%s,%d,%d,%d,%d,%ld,%ld,%ld\n", 
           p->comm, p->pid, p->parent_pid, p->first_child_pid, 
           p->next_sibling_pid, p->nvcsw, p->nivcsw, p->start_time);
    
    /* Initialize stack with root */
    stack[0] = p->pid;
    int top = 0;
    
    /* Process remaining entries */
    for (i = 1; i < actual_size; i++) {
        p = &buf[i];
        p_pos = find_parent(stack, top, p->parent_pid);
        
        /* Update stack to current depth */
        top = p_pos;
        stack[++top] = p->pid;
        
        /* Print dashes for depth */
        for (j = 0; j < top; j++) {
            printf("-");
        }
        
        /* Print process info */
        printf("%s,%d,%d,%d,%d,%ld,%ld,%ld\n", 
               p->comm, p->pid, p->parent_pid, p->first_child_pid, 
               p->next_sibling_pid, p->nvcsw, p->nivcsw, p->start_time);
    }
    
    free(buf);
    free(stack);
    return 0;
}

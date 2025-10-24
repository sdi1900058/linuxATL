#ifndef _UAPI_LINUX_K22INFO_H
#define _UAPI_LINUX_K22INFO_H
#include <sys/types.h>
struct k22info{
    char comm[64];                  //executable name
    pid_t pid;                      //pID
    pid_t parent_pid;               //parent pId
    pid_t first_child_pid;          //youngest child pid
    pid_t next_sibling_pid;         //oldest sibling pid
    unsigned long nvcsw;            // voluntary context switches
    unsigned long nivcsw;           // involuntary switches
    unsigned long start_time;       //monotonic start timenanoseconds
};
#endif

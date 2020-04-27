/*
 * Copyright (c) 2020 Jie Zheng
 */
#ifndef _TASK_SCHED_H
#define _TASK_SCHED_H
#include <hart.h>

// The only one `current`: task indicator
extern struct hart * current;

void
schedule_task(struct hart * hartptr);

void
process_blocking_list(void);

void
process_exiting_list(void);

void
task_vmm_sched_init(struct hart * hartptr,
                    void (*next_func)(void * opaque),
                    void * opaque);

struct hart *
process_running_list(void);

#endif

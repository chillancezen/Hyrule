/*
 * Copyright (c) 2020 Jie Zheng
 */

#include <hart.h>

// REF: https://github.com/chillancezen/ZeldaOS/blob/master/kernel/task.c

static struct list_elem task_list_head;
static struct list_elem task_blocking_list_head;
static struct list_elem task_exit_list_head;



__attribute__((constructor)) void
sched_pre_init(void)
{
    list_init(&task_list_head);
    list_init(&task_blocking_list_head);
    list_init(&task_exit_list_head);
}

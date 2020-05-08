/*
 * Copyright (c) 2020 Jie Zheng
 */

#ifndef _TASK_H
#define _TASK_H
#include <hart.h>

uint32_t
new_pid(void);

void
dump_threads(struct hart * hartptr);

void
raw_task_wake_up(struct hart * task);

void
register_task(struct virtual_machine * vm);

void
unregister_task(struct virtual_machine * vm);

uint32_t
call_clone(struct hart * hartptr, uint32_t flags, uint32_t child_stack_addr,
           uint32_t ptid_addr, uint32_t tls_addr, uint32_t ctid_addr);

uint32_t
call_wait4(struct hart * hartptr,
           int32_t pid,
           uint32_t wstatus_addr,
           uint32_t options,
           uint32_t rusage_addr);

uint32_t
call_execve(struct hart * hartptr,
            uint32_t filename_addr,
            uint32_t argv_addr,
            uint32_t envp_addr);
void
do_exit(struct hart * hartptr, uint32_t status);

uint32_t
call_set_tid_address(struct hart * hartptr, uint32_t tid_address);

uint32_t
call_set_robust_list(struct hart * hartptr, uint32_t head_addr,
                     uint32_t len);

#endif

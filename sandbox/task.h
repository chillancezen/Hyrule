/*
 * Copyright (c) 2020 Jie Zheng
 */

#ifndef _TASK_H
#define _TASK_H
#include <hart.h>

void
register_task(struct virtual_machine * vm);


uint32_t
call_clone(struct hart * hartptr, uint32_t flags, uint32_t child_stack_addr,
           uint32_t ptid_addr, uint32_t tls_addr, uint32_t ctid_addr);

#endif

/*
 * Copyright (c) 2020 Jie Zheng
 */

#ifndef _SYSCALL_H
#define _SYSCALL_H
#include <hart.h>
#define NR_SYSCALL_LINUX    512

typedef uint32_t (*sys_handler)(struct hart * hartptr, ...);

uint32_t
do_syscall(struct hart * hartptr, uint32_t syscall_number_a7,
           uint32_t arg_a0, uint32_t arg_a1, uint32_t arg_a2,
           uint32_t arg_a3, uint32_t arg_a4, uint32_t arg_a5);

#endif

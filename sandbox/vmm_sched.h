/*
 * Copyright (c) 2020 Jie Zheng
 */
#ifndef _VMM_SCHED_H
#define _VMM_SCHED_H
#include <stdint.h>

struct x86_64_cpustate {
    // NOTE rsp is not kept in the struct, the location of the struct itself is RSP
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    uint64_t rflag;
    uint64_t rip; // this is pushed via call.
    uint64_t trap_reason; // trap_reason must be pushed to stack before calling
}__attribute__((packed));

#define USERSPACE_TRAP_REASON_INIT      0x0
#define USERSPACE_TRAP_REASON_YIELD_CPU 0x1

#define call_userspace_trap(reason) {                                          \
    __asm__ volatile("pushq %%rax;"                                            \
                     ".extern user_space_trap;"                                \
                     "call user_space_trap;"                                   \
                     "popq %%rax;"                                             \
                     :                                                         \
                     :"a"(reason)                                              \
                     :"memory");                                               \
}

#define yield_cpu()     call_userspace_trap(USERSPACE_TRAP_REASON_YIELD_CPU)


void
schedule_idle_task(void);
#endif


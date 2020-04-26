/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <vmm_sched.h>
#include <util.h>
#include <log.h>

uint64_t
userspace_trap_handler(struct x86_64_cpustate * cpu)
{

    
    log_info("userspace_trap_handler:%d\n", cpu->trap_reason);
    return (uint64_t)cpu;
}


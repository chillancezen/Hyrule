/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <vmm_sched.h>
#include <task_sched.h>
#include <util.h>
#include <log.h>
#include <vm.h>

static struct hart idle_task;

void
task_vmm_sched_init(struct hart * hartptr,
                    void (*next_func)(void * opaque),
                    void * opaque)
{
    uint64_t stack = (uint64_t)hartptr->vmm_stack_ptr;
    stack -= sizeof(struct x86_64_cpustate);
    stack &= ~15;

    struct x86_64_cpustate * cpu = (struct x86_64_cpustate *)stack;
    memset(cpu, 0x0, sizeof(struct x86_64_cpustate));
    cpu->rdi = (uint64_t)opaque;
    cpu->rip = (uint64_t)next_func;
    cpu->trap_reason = USERSPACE_TRAP_REASON_INIT;
    hartptr->host_cpustate = cpu;
}

void
schedule_idle_task(void)
{
    // idle task reuse stack of major program
    current = &idle_task;

    yield_cpu();

    while (1) {
        // sleep for some time.
        yield_cpu();
    }
}
uint64_t
userspace_trap_handler(struct x86_64_cpustate * cpu)
{
    struct hart * next_task;
    uint64_t next_task_stack = (uint64_t)cpu;
    // must keep the host cpu state in order to restore it later.
    ASSERT(current);
    current->host_cpustate = cpu;
    // idle task is treated specifically.
    if (current != &idle_task) {
        schedule_task(current);
    }
   
    process_blocking_list();
    process_exiting_list();
    next_task = process_running_list();
    if (!next_task) {
        next_task = &idle_task;
    }
    current = next_task;
    ASSERT(current && current->host_cpustate);
    next_task_stack = (uint64_t)current->host_cpustate;
    if (current == &idle_task) {
        log_trace("next task to run: [idle]\n");
    } else {
        log_trace("next task to run: %d\n", next_task->native_vmptr->pid);
    }
    return next_task_stack;
}


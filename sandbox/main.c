/*
 * Copyright (c) 2019-2020 Jie Zheng
 */

#include <stdio.h>
#include <physical_memory.h>
#include <util.h>
#include <hart.h>
#include <vm.h>
#include <mmu.h>
#include <translation.h>
#include <debug.h>
#include <ini.h>
#include <app.h>
#include <vmm_sched.h>
#include <task_sched.h>

static void
log_init(void)
{
    int verbosity = 5;
    char * verbosity_string = getenv("VERBOSITY");
    if (verbosity_string) {
        verbosity = atoi(verbosity_string);
        if (verbosity > 5) {
            // FATAL message must be displayed.
            verbosity = 5;
        }
    }
    log_set_level(verbosity);
}

int
main(int argc, char ** argv)
{
    log_init();
        
    if (argc <= 1) {
        log_fatal("please specify the application host file path\n");
        exit(1);
    }
    struct virtual_machine sandbox_vm;
    char * envp[]= {
        NULL
    };
    application_sandbox_init(&sandbox_vm, argv[1], argv + 1, envp);

    // The initial process is initialized as current task to be scheduled.
    // no need to put it in running_tasks and yield cpu to run it.
    // XXX: finally it's been made a init task put into running queue
    transit_state(sandbox_vm.hartptr, TASK_STATE_RUNNING);
    task_vmm_sched_init(sandbox_vm.hartptr,
                        (void (*)(void *))vmresume,
                        sandbox_vm.hartptr);
    schedule_task(sandbox_vm.hartptr);

    // initialize idle task which is the first task to run.
    schedule_idle_task();
    __not_reach();
    return 0;
}

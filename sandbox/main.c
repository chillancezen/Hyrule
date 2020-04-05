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
    //add_breakpoint(0x1d38c);
    //add_breakpoint(0x159e6c);
    //add_breakpoint(0x155b00);
    //add_breakpoint(0x104e0);
    //add_breakpoint(0x104e4);
    //add_breakpoint(0x104e8);
    vmresume(hart_by_id(&sandbox_vm, sandbox_vm.boot_hart));
    return 0;
}

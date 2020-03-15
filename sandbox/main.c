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

int
main(int argc, char ** argv)
{
    if (argc <= 1) {
        log_fatal("please specify the application host file path\n");
        exit(1);
    }
    struct virtual_machine sandbox_vm;
    application_sandbox_init(&sandbox_vm, argv[1]);
    //add_breakpoint(0x10308);
    vmresume(hart_by_id(&sandbox_vm, sandbox_vm.boot_hart));
#if 0
    // once config file is loaded, don't release it ever.
    ini_t * ini_config = ini_load(argv[1]);
    if (!ini_config) {
        log_fatal("can not load ini file:%s\n", argv[1]);
        exit(2);
    }
    // vmm platform init
    vmm_misc_init(ini_config);
    // Boot a VM
    struct virtual_machine vm;
    virtual_machine_init(&vm, ini_config);
    vmresume(hart_by_id(&vm, vm.boot_hart));
    __not_reach();

#endif
    return 0;
}

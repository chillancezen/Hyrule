/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <syscall.h>
#include <string.h>
#include <log.h>
#include <vm.h>
#include <uaccess.h>

static sys_handler handlers[NR_SYSCALL_LINUX];



uint32_t
do_syscall(struct hart * hartptr, uint32_t syscall_number_a7,
           uint32_t arg_a0, uint32_t arg_a1, uint32_t arg_a2,
           uint32_t arg_a3, uint32_t arg_a4, uint32_t arg_a5)
{
    ASSERT(syscall_number_a7 < NR_SYSCALL_LINUX);
    sys_handler handler = handlers[syscall_number_a7];
    if (!handler) {
        log_fatal("syscall:%d not implemented, see full syscalls include/uapi/asm-generic/unistd.h\n", syscall_number_a7);
        __not_reach();
    }
    uint32_t ret = handler(hartptr, arg_a0, arg_a1, arg_a2, arg_a3, arg_a4, arg_a5);
    log_trace("pc:%x syscall:%d args[a0:%08x, a1:%08x, a2:%08x, "
              "a3:%08x, a4:%08x, a5:%08x] ret:%08x\n",
              hartptr->pc, syscall_number_a7,
              arg_a0, arg_a1, arg_a2, arg_a3, arg_a4, arg_a5, ret);
    return ret;
}

static uint32_t
call_getuid(struct hart * hartptr)
{
        return 0;
}


static uint32_t
call_brk(struct hart * hartptr, uint32_t addr)
{
    struct virtual_machine * vm = hartptr->vmptr;
    if (!addr || addr <= vm->vma_heap->addr_high) {
        // return currnet location of program break.
        return vm->vma_heap->addr_high;
    } else {
        ASSERT(addr > vm->vma_heap->addr_high);
        uint32_t addr_high_bak = vm->vma_heap->addr_high;
        vm->vma_heap->addr_high = addr;
        if (!is_vma_eligable(vm, vm->vma_heap)) {
            vm->vma_heap->addr_high = addr_high_bak;
            return -1;
        }
        vm->vma_heap->host_base = realloc(vm->vma_heap->host_base,
                                          vm->vma_heap->addr_high - vm->vma_heap->addr_low);
        if (!vm->vma_heap->host_base) {
            return -1;
        }
    }
    // we have to extend the vma.
    return addr;
}


static uint32_t
call_uname(struct hart * hartptr, uint32_t utsname_addr)
{
    // The data structure /include/uapi/linux/utsname.h
    void * utsname = user_world_pointer(hartptr, utsname_addr);
    strcpy(utsname + 0 * 65, "ZeldaLinux"); // sysname
    strcpy(utsname + 1 * 65, "hyrule"); // nodename
    strcpy(utsname + 2 * 65, "v0.0.1"); //release
    strcpy(utsname + 3 * 65, "native build"); //version
    strcpy(utsname + 4 * 65, "riscv32"); //version
    strcpy(utsname + 5 * 65, "castle"); //version
    return 0;
}

__attribute__((constructor)) static void
syscall_init(void)
{
    memset(handlers, 0x0, sizeof(handlers));
#define _(num, func)                                                           \
    handlers[num] = (sys_handler)func
   
    _(160, call_uname);
    _(174, call_getuid);
    _(175, call_getuid);
    _(176, call_getuid);
    _(177, call_getuid);
    _(214, call_brk);
#undef _
}

/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <syscall.h>
#include <string.h>
#include <log.h>
#include <vm.h>
#include <uaccess.h>
#include <errno.h>

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
        if (!is_vma_eligible(vm, vm->vma_heap)) {
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
    strcpy(utsname + 2 * 65, "5.4.0"); //release. XXX: DON'T SET IT TOO LOW, OR GLIBC PANICS
    strcpy(utsname + 3 * 65, "v2020.03"); //version
    strcpy(utsname + 4 * 65, "riscv32"); //machine
    strcpy(utsname + 5 * 65, "castle"); //domain
    return 0;
}

static uint32_t
call_openat(struct hart * hartptr, uint32_t dirfd, uint32_t pathaddr,
            uint32_t flags, uint32_t mode)
{
    char * path = user_world_pointer(hartptr, pathaddr);
    return do_openat(hartptr, dirfd, path, flags, mode);
}

static uint32_t
call_writev(struct hart * hartptr, uint32_t fd, uint32_t iov_addr,
            uint32_t iovcnt)
{
    struct iovec32 * iov_base = user_world_pointer(hartptr, iov_addr);
    return do_writev(hartptr, fd, iov_base, iovcnt);
}

static uint32_t
generic_callback_nosys(struct hart * hartptr)
{
    return -ENOSYS;
}

static uint32_t
call_statx(struct hart * hartptr, uint32_t dirfd, uint32_t pathname_addr,
           uint32_t flags, uint32_t mask, uint32_t statxbuf_addr)
{
    //char * pathname = user_world_pointer(hartptr, pathname_addr);
    //printf("%s\n", pathname);
    //__not_reach();
    return 0;
}


static uint32_t
call_write(struct hart * hartptr, uint32_t fd, uint32_t buf_addr,
           uint32_t nr_to_write)
{
    void * buf = user_world_pointer(hartptr, buf_addr);
    return do_write(hartptr, fd, buf, nr_to_write);
}

static uint32_t
call_exit(struct hart * hartptr, uint32_t status)
{
    // FIXME: a lot of work to do.
    exit(status);
    __not_reach();
    return 0;
}

static uint32_t
call_mmap(struct hart * hartptr, uint32_t proposal_addr, uint32_t len,
          uint32_t prot, uint32_t flags, uint32_t fd, uint32_t offset)
{ 
    return do_mmap(hartptr, proposal_addr, len, prot, flags, fd, offset);
}

static uint32_t
call_munmap(struct hart * hartptr, uint32_t addr, uint32_t len)
{
    return do_munmap(hartptr, addr, len);
}

__attribute__((constructor)) static void
syscall_init(void)
{
    memset(handlers, 0x0, sizeof(handlers));
#define _(num, func)                                                           \
    handlers[num] = (sys_handler)func
    _(56, call_openat);
    _(64, call_write);
    _(66, call_writev);
    _(78, generic_callback_nosys);
    _(94, call_exit);
    _(160, call_uname);
    _(174, call_getuid);
    _(175, call_getuid);
    _(176, call_getuid);
    _(177, call_getuid);
    _(214, call_brk);
    _(215, call_munmap);
    _(222, call_mmap);
    _(291, call_statx);
#undef _
}

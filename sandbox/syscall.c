/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <syscall.h>
#include <string.h>
#include <log.h>
#include <vm.h>
#include <uaccess.h>
#include <errno.h>
#include <sys/time.h>
#include <sysnet.h>
#include <signal.h>
#include <sys/sysinfo.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <task.h>
#include <tinyprintf.h>

static sys_handler handlers[NR_SYSCALL_LINUX];
static char * handler_name[NR_SYSCALL_LINUX];



uint32_t
do_syscall(struct hart * hartptr, uint32_t syscall_number_a7,
           uint32_t arg_a0, uint32_t arg_a1, uint32_t arg_a2,
           uint32_t arg_a3, uint32_t arg_a4, uint32_t arg_a5)
{
    ASSERT(syscall_number_a7 < NR_SYSCALL_LINUX);
    sys_handler handler = handlers[syscall_number_a7];
    if (!handler) {
        log_fatal("pc:%x syscall:%d not implemented, see full syscalls list include/uapi/asm-generic/unistd.h\n",
                  hartptr->pc, syscall_number_a7);
        __not_reach();
    }
    uint32_t ret = handler(hartptr, arg_a0, arg_a1, arg_a2, arg_a3, arg_a4, arg_a5);
    log_trace("pc:%x syscall:(%s no.%d) args[a0:%08x, a1:%08x, a2:%08x, "
              "a3:%08x, a4:%08x, a5:%08x] ret:%08x errno:%d\n",
              hartptr->pc, handler_name[syscall_number_a7], syscall_number_a7,
              arg_a0, arg_a1, arg_a2, arg_a3, arg_a4, arg_a5, ret, errno);
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
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_VM);
    if (!addr || addr <= vm->vma_heap->addr_high) {
        // return currnet location of program break.
        return vm->vma_heap->addr_high;
    } else {
        ASSERT(addr > vm->vma_heap->addr_high);
        uint32_t addr_high_bak = vm->vma_heap->addr_high;
        vm->vma_heap->addr_high = addr;
        if (!is_vma_eligible(vm, vm->vma_heap)) {
            vm->vma_heap->addr_high = addr_high_bak;
            return -ENOMEM;
        }
        vm->vma_heap->host_base = realloc(vm->vma_heap->host_base,
                                          vm->vma_heap->addr_high - vm->vma_heap->addr_low);
        if (!vm->vma_heap->host_base) {
            return -ENOMEM;
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

__attribute__((unused))
static uint32_t
generic_callback_nosys(struct hart * hartptr)
{
    return -ENOSYS;
}

static uint32_t
call_statx(struct hart * hartptr, uint32_t dirfd, uint32_t pathname_addr,
           uint32_t flags, uint32_t mask, uint32_t statxbuf_addr)
{
    char * pathname = user_world_pointer(hartptr, pathname_addr);
    void * statxbuf = user_world_pointer(hartptr, statxbuf_addr);
    return do_statx(hartptr, dirfd, pathname, flags, mask, statxbuf);
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

    do_exit(hartptr, status);

    __not_reach();
    return 0;
}

static uint32_t
call_exit_group(struct hart * hartptr, uint32_t status)
{
    // all children in the calling process's thread group are going to be terminated.
    do_exit(hartptr, status);

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

static uint32_t
call_gettimeofday(struct hart * hartptr, uint32_t tv_addr, uint32_t tz_addr)
{
    void * tv = tv_addr ? user_world_pointer(hartptr, tv_addr) : NULL; 
    void * tz = tz_addr ? user_world_pointer(hartptr, tz_addr) : NULL;
    return gettimeofday(tv, tz);
}

static uint32_t
call_ioctl(struct hart * hartptr, uint32_t fd, uint32_t request,
           uint32_t argp_addr)
{
    return do_ioctl(hartptr, fd, request, argp_addr);
}

static uint32_t
call_read(struct hart * hartptr, uint32_t fd, uint32_t buf_addr, uint32_t count)
{
    void * buf = user_world_pointer(hartptr, buf_addr);
    return do_read(hartptr, fd, buf, count);
}

static uint32_t
call_close(struct hart * hartptr, int fd)
{
    return do_close(hartptr, fd);
}

static uint32_t
call_socket(struct hart * hartptr, uint32_t socket_family,
            uint32_t socket_type, uint32_t protocal)
{
    return do_socket(hartptr, socket_family, socket_type, protocal);
}

#include <time.h>
static uint32_t
call_clock_gettime(struct hart * hartptr, uint32_t clk_id,
                   uint32_t timespec_addr)
{
    void * timespec_ptr = user_world_pointer(hartptr, timespec_addr);
    return clock_gettime(clk_id, timespec_ptr);
}

static uint32_t
call_getdents64(struct hart * hartptr, uint32_t fd, uint32_t dirp_addr,
                uint32_t count)
{
   return do_getdents64(hartptr, fd, dirp_addr, count); 
}

static uint32_t
call_readlinkat(struct hart * hartptr, uint32_t dirfd, uint32_t pathname_addr,
                uint32_t buff_addr, uint32_t buf_size)
{
    char * pathname = user_world_pointer(hartptr, pathname_addr);
    void * buff = user_world_pointer(hartptr, buff_addr);
    return do_readlinkat(hartptr, dirfd, pathname, buff, buf_size);
}

static uint32_t
call_sendfile(struct hart * hartptr, uint32_t out_fd, uint32_t fd,
              uint32_t offset_addr, uint32_t count)
{
    void * offset = NULL;
    if (offset_addr) {
        offset = user_world_pointer(hartptr, offset_addr);
    }
    return do_sendfile(hartptr, out_fd, fd, offset, count);
}

static uint32_t
call_unlinkat(struct hart * hartptr, uint32_t dirfd,
              uint32_t pathname_addr, uint32_t flags)
{
    char * pathname = user_world_pointer(hartptr, pathname_addr);
    return do_unlinkat(hartptr, dirfd, pathname, flags);
}

static uint32_t
call_faccessat(struct hart * hartptr, uint32_t dirfd,
               uint32_t pathname_addr, uint32_t mode, uint32_t flags)
{
    // always accessible
    return 0;
}

static uint32_t
call_lseek(struct hart * hartptr, uint32_t fd, uint32_t offset,
           uint32_t whence)
{
    return do_lseek(hartptr, fd, offset, whence);
}

static uint32_t
call_getpid(struct hart * hartptr)
{
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_NATIVE);
    return vm->tgid;    
}

static uint32_t
call_getppid(struct hart * hartptr)
{
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_NATIVE);
    return vm->ppid;
}

static uint32_t
call_getcwd(struct hart * hartptr, uint32_t buf_addr, uint32_t size)
{
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FS);
    if (buf_addr == 0) {
        return 0;
    }
    void * buf = user_world_pointer(hartptr, buf_addr);

    uint32_t nchar = strlen(vm->cwd);
    memcpy(buf, vm->cwd, nchar + 1);
    log_debug("cwd of pid %d: %s\n", vm->pid, vm->cwd);
    return nchar + 1;
}

static uint32_t
call_chdir(struct hart * hartptr, uint32_t path_addr)
{

    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FS);
    char * path = user_world_pointer(hartptr, path_addr);

    int is_absolute_path = 0;
    char * ptr = path;
    for (; *ptr && *ptr == ' '; ptr++);
    is_absolute_path = *ptr == '/';

    char cpath[MAX_PATH];
    char ncpath[MAX_PATH];

    if (is_absolute_path) {
        canonicalize_path_name((uint8_t *)cpath, (const uint8_t *)ptr);
    } else {
        tfp_sprintf(ncpath, "%s/%s", vm->cwd, ptr);
        canonicalize_path_name((uint8_t *)cpath, (const uint8_t *)ncpath);
    }

    tfp_sprintf(cpath, "%s/", cpath);
    log_debug("process: %d change dir: %s\n", vm->pid, cpath);
    strcpy(vm->cwd, cpath);
    return 0;
}

static uint32_t
call_getpgid(struct hart * hartptr)
{
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_NATIVE);
    return vm->ppid;
    // FIXME
    return -ESRCH;
}

static uint32_t
call_sysinfo(struct hart * hartptr, uint32_t sysinfo_addr)
{
    //struct sysinfo * sysinfo_buf = user_world_pointer(hartptr, sysinfo_addr);
    // FIXME: customize some fields in the struct
    //return sysinfo(sysinfo_buf);

    return -ENOSYS;
}

static uint32_t
call_getrlimit(struct hart * hartptr, uint32_t resource, uint32_t rlim_addr)
{
    void * rlim = user_world_pointer(hartptr, rlim_addr);
    return ERRNO(getrlimit(resource, rlim));
}

static uint32_t
call_pselect(struct hart * hartptr, uint32_t nfds, uint32_t readfds_addr,
             uint32_t writefds_addr, uint32_t exceptfds_addr,
             uint32_t timeout_addr, uint32_t sigmask_addr)
{
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    int is_readfds_set = user_accessible(hartptr, readfds_addr);
    int is_writefds_set = user_accessible(hartptr, writefds_addr);
    int is_exceptfds_set = user_accessible(hartptr, exceptfds_addr);

    fd_set * readfds = is_readfds_set ? user_world_pointer(hartptr, readfds_addr) : NULL;
    fd_set * writefds = is_writefds_set ? user_world_pointer(hartptr, writefds_addr) : NULL;
    fd_set * exceptfds = is_exceptfds_set ? user_world_pointer(hartptr, exceptfds_addr) : NULL;

    fd_set readfds_host;
    fd_set writefds_host;
    fd_set exceptfds_host;

    FD_ZERO(&readfds_host);
    FD_ZERO(&writefds_host);
    FD_ZERO(&exceptfds_host);

    int idx;
    for (idx = 0; idx < nfds + 1 && idx < MAX_FILES_NR; idx++) {
        if (!vm->files[idx].valid) {
            continue;
        }

        if (readfds && FD_ISSET(idx, readfds)) {
            FD_SET(vm->files[idx].host_fd, &readfds_host);
            log_trace("call_pselect readfds: %d\n", idx);
        }

        if (writefds && FD_ISSET(idx, writefds)) {
            FD_SET(vm->files[idx].host_fd, &writefds_host);
            log_trace("call_pselect writefds: %d\n", idx);
        }

        if (exceptfds && FD_ISSET(idx, exceptfds)) {
            FD_SET(vm->files[idx].host_fd, &exceptfds_host);
            log_trace("call_pselect exceptfds: %d\n", idx);
        }

    }

    // FIXME: do pselect from host side
    return 0;
}

static uint32_t
call_mprotect(struct hart * hartptr, uint32_t addr_addr, uint32_t len,
              uint32_t prot)
{
    // FIXME: modify pm region attributes
    return 0;
}

static uint32_t
call_madvice(struct hart * hartptr, uint32_t addr_addr, uint32_t len,
             uint32_t advice)
{
    // FIXME: implement the body
    return 0;
}
__attribute__((constructor)) static void
syscall_init(void)
{
    memset(handlers, 0x0, sizeof(handlers));
#define _(num, func) {                                                         \
    handlers[num] = (sys_handler)func;                                         \
    handler_name[num] = #func;                                                 \
}


    _(17, call_getcwd);
    _(23, call_dup);
    _(24, call_dup3);
    _(25, call_fnctl);
    _(29, call_ioctl);
    _(35, call_unlinkat);
    _(48, call_faccessat);
    _(49, call_chdir);
    _(56, call_openat);
    _(57, call_close);
    _(61, call_getdents64);
    _(62, call_lseek);
    _(63, call_read);
    _(64, call_write);
    _(66, call_writev);
    _(71, call_sendfile);
    _(72, call_pselect);
    _(78, call_readlinkat);
    _(93, call_exit);
    _(94, call_exit_group);
    _(96, call_set_tid_address);
    _(99, call_set_robust_list);
    _(113, call_clock_gettime);
    _(129, call_kill);
    _(134, call_sigaction);
    _(135, call_sigprocmask);
    _(155, call_getpgid);
    _(160, call_uname);
    _(163, call_getrlimit);
    _(169, call_gettimeofday);
    _(172, call_getpid);
    _(173, call_getppid);
    _(174, call_getuid);
    _(175, call_getuid);
    _(176, call_getuid);
    _(177, call_getuid);
    _(179, call_sysinfo);
    _(198, call_socket);
    _(214, call_brk);
    _(215, call_munmap);
    _(220, call_clone);
    _(221, call_execve);
    _(222, call_mmap);
    _(226, call_mprotect);
    _(233, call_madvice);
    _(260, call_wait4);
    _(291, call_statx);
#undef _
}

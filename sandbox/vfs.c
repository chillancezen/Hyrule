/*
 * Copyright (c) 2020 Jie Zheng
 */

#include <vm.h>
#include <vfs.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <uaccess.h>
#include <sys/mman.h>
#include <app.h>
#include <elf.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/sendfile.h>
#include <fcntl.h>


int
canonicalize_path_name(uint8_t * dst, const uint8_t * src)
{
    int idx = 0;
    int idx1 = 0;
    int16_t iptr = 0;
    int16_t index_ptr = 0;
    int16_t word_start = -1;
    int16_t word_end = 0;
    int16_t word_len = 0;
    int16_t index_stack[256];
    memset(index_stack, 0x0, sizeof(index_stack));
    for (idx = 0; idx < MAX_PATH; idx++) {
        if (src[idx] != '/' && src[idx] && idx != 0)
            continue;
        if (word_start == -1 && src[idx]) {
            word_start = idx;
            continue;
        }
        if (word_start == -1 && !src[idx])
            break;
        /*At least '/' appears before.*/
        word_len = idx - word_start;
        if (word_len == 1) {
            /*
             * Give special care to '/'
             */
            if(!src[idx] && !index_ptr) {
                index_stack[index_ptr++] = word_start;
            } else {
                word_start = idx;
            }
        }else if(word_len == 2 && src[idx - 1] == '.') {
            word_start = idx;
        } else if (word_len == 3 &&
            src[idx - 1] == '.' &&
            src[idx - 2] == '.') {
            word_start = idx;
            index_ptr = index_ptr ? index_ptr -1 : 0;
        } else {
            index_stack[index_ptr++] = word_start;
            word_start = idx;
        }
        if (!src[idx])
            break;
    }
    /* Reform the path*/
    for(idx = 0; idx < index_ptr; idx++) {
        word_start = index_stack[idx];
        word_end = 0;
        for(idx1 = word_start + 1; idx1 < MAX_PATH; idx1++) {
            if(src[idx1] == '/') {
                word_end = idx1;
                break;   
            } else if (!src[idx1]) {
                word_end = idx1 + 1;
                break;
            }
        }
        for(idx1 = word_start; idx1 < word_end; idx1++) {
            dst[iptr++] = src[idx1];
        }
    }
    dst[iptr] = '\x0';
    return 0;
}

struct file *
allocate_file_descriptor(struct virtual_machine * vm)
{
    struct file * free_file = NULL;
    int idx = 0;
    for (idx = 0; idx < MAX_FILES_NR; idx++) {
        if (!vm->files[idx].valid) {
            free_file = &vm->files[idx];
            memset(free_file, 0x0, sizeof(struct file));
            free_file->fd = idx;
            free_file->valid = 1;
            break;
        }
    }
    return free_file;
}

void
deallocate_file_descriptor(struct virtual_machine * vm,
                           struct file * target_file)
{
    ASSERT(target_file->valid);
    if (target_file->host_cpath) {
        free(target_file->host_cpath);
    }
    memset(target_file, 0x0, sizeof(struct file));
}

#define AT_FDCWD -100
uint32_t
do_openat(struct hart * hartptr, uint32_t dirfd, const char * guest_path,
          uint32_t flags, uint32_t mode)
{
    struct virtual_machine * vm_fs = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FS);
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    // judge whether a relative path is given.
    char * ptr = (char *)guest_path;
    for(;*ptr && *ptr == ' '; ptr++);
    int is_relative_path = *ptr != '/';
    const char * relative_dir_path = "";

    uint8_t guest_cpath[MAX_PATH];
    ASSERT(!canonicalize_path_name(guest_cpath, (const uint8_t *)guest_path));
    if (is_relative_path) {
        if ((int32_t)dirfd == AT_FDCWD) {
            relative_dir_path = vm_fs->cwd;
        } else {
            // FIXME:
            __not_reach();
        }
    }
    // compose host absolute file path.
    uint8_t host_path[MAX_PATH];
    uint8_t host_cpath[MAX_PATH];
    ptr = (char *)host_path;
    ptr = append_string(ptr, (const char *)vm_fs->root);
    ptr = append_string(ptr, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    // setup an unoccupied file descriptor
    struct file * new_file = allocate_file_descriptor(vm_files);
    if (!new_file) {
        return -ENOMEM;
    }
    int host_fd = open((char *)host_cpath, flags, mode);
    if (host_fd < 0) {
        deallocate_file_descriptor(vm_files, new_file);
        return ERRNO(host_fd);
    }
    new_file->host_fd = host_fd;
    uint8_t guest_abs_path[MAX_PATH];
    uint8_t guest_abs_cpath[MAX_PATH];
    ptr = append_string((char *)guest_abs_path, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(guest_abs_cpath, guest_abs_path));
    new_file->host_cpath = strdup((const char *)host_cpath);
    new_file->clone_blob.file_type = FILE_TYPE_REGULAR;
    new_file->clone_blob.mode = mode;
    new_file->clone_blob.flags = flags;

    log_debug("openat: guest path:%s host path:%s fd\n", guest_path, host_cpath,
              new_file->fd);
    return new_file->fd;
}

uint32_t
do_close(struct hart * hartptr, uint32_t fd)
{
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm->files[fd].valid) {
        return -EBADF;
    }
    uint32_t rc = close(vm->files[fd].host_fd);

    deallocate_file_descriptor(vm, &vm->files[fd]);   
    return ERRNO(rc);
}

struct statx_timestamp {
    uint64_t tv_sec;    /* Seconds since the Epoch (UNIX time) */
    uint32_t tv_nsec;   /* Nanoseconds since tv_sec */
}__attribute__((packed));

struct statx {
    uint32_t stx_mask;     /* Mask of bits indicating
                filled fields */
    uint32_t stx_blksize;     /* Block size for filesystem I/O */
    uint64_t stx_attributes;  /* Extra file attribute indicators */
    uint32_t stx_nlink;       /* Number of hard links */
    uint32_t stx_uid;     /* User ID of owner */
    uint32_t stx_gid;     /* Group ID of owner */
    uint16_t stx_mode;     /* File type and mode */
    uint64_t stx_ino;     /* Inode number */
    uint64_t stx_size;     /* Total size in bytes */
    uint64_t stx_blocks;      /* Number of 512B blocks allocated */
    uint64_t stx_attributes_mask;
                 /* Mask to show what's supported
                in stx_attributes */

    /* The following fields are file timestamps */
    struct statx_timestamp stx_atime;  /* Last access */
    struct statx_timestamp stx_btime;  /* Creation */
    struct statx_timestamp stx_ctime;  /* Last status change */
    struct statx_timestamp stx_mtime;  /* Last modification */

    /* If this file represents a device, then the next two
     fields contain the ID of the device */
    uint32_t stx_rdev_major;  /* Major ID */
    uint32_t stx_rdev_minor;  /* Minor ID */

    /* The next two fields contain the ID of the device
     containing the filesystem where the file resides */
    uint32_t stx_dev_major;   /* Major ID */
    uint32_t stx_dev_minor;   /* Minor ID */
}__attribute__((packed));


uint32_t
do_statx(struct hart * hartptr, uint32_t dirfd, char * pathname, uint32_t flag,
         uint32_t mask, void * statxbuf)
{
    struct virtual_machine * vm_fs = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FS);

    char * ptr = pathname;
    for(;*ptr && *ptr == ' '; ptr++);
    int is_relative_path = *ptr != '/';
    const char * relative_dir_path = "";

    uint8_t guest_cpath[MAX_PATH];
    ASSERT(!canonicalize_path_name(guest_cpath, (const uint8_t *)pathname));
    if (is_relative_path) {
        if ((int32_t)dirfd == AT_FDCWD) {
            relative_dir_path = vm_fs->cwd;
        } else {
            // FIXME
        }
    }
    uint8_t host_path[MAX_PATH];
    uint8_t host_cpath[MAX_PATH];
    ptr = (char *)host_path;
    ptr = append_string(ptr, (const char *)vm_fs->root);
    ptr = append_string(ptr, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    // XXX: with gcc 4.8, I can't find statx system call warpper in GLIBC,
    // so I use direct syscall.
    int rc = syscall(__NR_statx, AT_FDCWD, host_cpath, flag, mask, statxbuf);
    return ERRNO(rc);
}

uint32_t
do_readlinkat(struct hart * hartptr, uint32_t dirfd, const char * pathname,
              void * buf, uint32_t buf_size)
{
    struct virtual_machine * vm_fs = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FS);
    char * ptr = (char *)pathname;
    for(;*ptr && *ptr == ' '; ptr++);
    int is_relative_path = *ptr != '/';
    const char * relative_dir_path = "";

    uint8_t guest_cpath[MAX_PATH];
    ASSERT(!canonicalize_path_name(guest_cpath, (const uint8_t *)pathname));
    if (is_relative_path) {
        if ((int32_t)dirfd == AT_FDCWD) {
            relative_dir_path = vm_fs->cwd;
        } else {
            // FIXME
        }
    }

    uint8_t host_path[MAX_PATH];
    uint8_t host_cpath[MAX_PATH];
    ptr = (char *)host_path;
    ptr = append_string(ptr, (const char *)vm_fs->root);
    ptr = append_string(ptr, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    return ERRNO(readlinkat(AT_FDCWD, (const char *)host_cpath, buf, buf_size));
}

uint32_t
do_writev(struct hart * hartptr, uint32_t fd,
          struct iovec32 * guest_iov, uint32_t iovcnt)

{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EINVAL;
    }
    
    struct iovec * host_vecbase = malloc(sizeof(struct iovec) * iovcnt);
    if (!host_vecbase) {
        return -ENOMEM;
    }
    int idx = 0;
    for (idx = 0; idx < iovcnt; idx++) {
        host_vecbase[idx].iov_base =
            user_world_pointer(hartptr, guest_iov[idx].iov_base);
        host_vecbase[idx].iov_len = guest_iov[idx].iov_len;
    }
    uint32_t host_rc = writev(vm_files->files[fd].host_fd, host_vecbase, iovcnt);
    free(host_vecbase);
    return ERRNO(host_rc);
}

uint32_t
do_write(struct hart * hartptr, uint32_t fd, void * buf, uint32_t nr_write)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    return ERRNO(write(vm_files->files[fd].host_fd, buf, nr_write));
}

uint32_t
do_read(struct hart * hartptr, uint32_t fd, void * buf, uint32_t nr_read)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    return ERRNO(read(vm_files->files[fd].host_fd, buf, nr_read));
}

uint32_t
do_ioctl(struct hart * hartptr, uint32_t fd, uint32_t request,
         uint32_t argp_addr)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    void * argp = NULL;
    if (user_accessible(hartptr, argp_addr)) {
        argp = user_world_pointer(hartptr, argp_addr);
    }
    return ERRNO(ioctl(vm_files->files[fd].host_fd, request, argp));
}

uint32_t
do_getdents64(struct hart * hartptr, uint32_t fd, uint32_t dirp_addr,
              uint32_t count)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    void * dirp = user_world_pointer(hartptr, dirp_addr);
    return syscall(__NR_getdents64, vm_files->files[fd].host_fd, dirp, count);
}

uint32_t
do_sendfile(struct hart * hartptr, uint32_t out_fd, uint32_t fd,
            void * offset, uint32_t count)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }

    if (out_fd > MAX_FILES_NR || !vm_files->files[out_fd].valid) {
        return -EBADF;
    }

    return ERRNO(sendfile(vm_files->files[out_fd].host_fd, vm_files->files[fd].host_fd, offset, count));
}

#define PAGE_ROUNDUP(addr) (((addr) & 4095) ? (((addr) & ~4095) + 4096) : (addr))

static uint32_t
allocate_mmap_region(struct virtual_machine * vm, uint32_t proposal_addr,
                      uint32_t len, uint32_t mprot)
{
    uint32_t eligible_addr_low = 0;
    int is_eligible = 0;
    if (proposal_addr) {
        uint32_t addr_low = PAGE_ROUNDUP(proposal_addr);
        uint32_t addr_high = addr_low + len;

        // Test whether the address has already been mapped.
        //  If another mapping already exists there, the kernel picks a new
        //  address that may or may not depend on the hint.
        is_eligible = is_range_eligible(vm, addr_low, addr_high);
        if (is_eligible) {
            eligible_addr_low = addr_low;
        }
    }

    if (!is_eligible)  {
        uint32_t addr_found = search_free_mmap_region(vm, len);
        if (addr_found) {
            is_eligible = 1;
            eligible_addr_low = PAGE_ROUNDUP(addr_found);
        }
    }

    if (!is_eligible) {
        return -ENOMEM;
    }

    mmap_setup(vm, eligible_addr_low, len, mprot, NULL);
    return eligible_addr_low; 
}


uint32_t
do_mmap(struct hart* hartptr, uint32_t proposal_addr, uint32_t len,
        uint32_t prot, uint32_t flags, uint32_t fd, uint32_t offset)
{
    
    uint32_t mprot = 0;
    mprot |= (flags & PROT_READ) ? PROGRAM_READ : 0;
    mprot |= (flags & PROT_WRITE) ? PROGRAM_WRITE : 0;
    mprot |= (flags & PROT_EXEC) ? PROGRAM_EXECUTE : 0;

    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_VM);
    if (len == 0) {
        return -EINVAL;
    }
    
    if (flags & MAP_ANONYMOUS) {
        return allocate_mmap_region(vm, proposal_addr, len, mprot); 
    } else {
        // FIXME: FILE BACKED MAPPING
        __not_reach();
    }
    return 0;
}


uint32_t
do_munmap(struct hart * hartptr, uint32_t addr, uint32_t len)
{
    struct virtual_machine * vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_VM);
    struct pm_region_operation * pmr = search_pm_region_callback(vm, addr);
    if (!pmr) {
        return -EINVAL;
    }
    uint32_t region_len = pmr->addr_high - pmr->addr_low;
    if (region_len != len) {
        return -EINVAL;
    }
    
    if (pmr->pmr_reclaim) {
        pmr->pmr_reclaim(pmr->opaque, hartptr, pmr);
    }
    unregister_pm_region(vm, pmr);
    return 0;
}

uint32_t
do_unlinkat(struct hart * hartptr, uint32_t dirfd,
            const char * pathname, uint32_t flags)
{
    struct virtual_machine * vm_fs = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FS);
    char * ptr = (char *)pathname;
    for(;*ptr && *ptr == ' '; ptr++);
    int is_relative_path = *ptr != '/';
    const char * relative_dir_path = "";

    uint8_t guest_cpath[MAX_PATH];
    ASSERT(!canonicalize_path_name(guest_cpath, (const uint8_t *)pathname));
    if (is_relative_path) {
        if ((int32_t)dirfd == AT_FDCWD) {
            relative_dir_path = vm_fs->cwd;
        } else {
            // FIXME
        }
    }

    uint8_t host_path[MAX_PATH];
    uint8_t host_cpath[MAX_PATH];
    ptr = (char *)host_path;
    ptr = append_string(ptr, (const char *)vm_fs->root);
    ptr = append_string(ptr, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    return ERRNO(unlinkat(AT_FDCWD, (const char *)host_cpath, flags));
}

uint32_t
do_lseek(struct hart * hartptr, uint32_t fd, uint32_t offset, uint32_t whence)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    
    return ERRNO(lseek(vm_files->files[fd].host_fd, offset, whence));
}

static uint32_t
do_fcntl_duplicate_fd(struct hart * hartptr, uint32_t fd, uint32_t cmd, uint32_t opaque)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }

    struct file * target_file = allocate_file_descriptor(vm_files);
    if (!target_file) {
        return -ENOMEM;
    }

    int32_t host_fd = fcntl(vm_files->files[fd].host_fd, cmd, opaque);
    if (host_fd < 0) {
        deallocate_file_descriptor(vm_files, target_file);
        return ERRNO(host_fd);
    }

    target_file->host_fd = host_fd;
    if (vm_files->files[fd].host_cpath) {
        target_file->host_cpath = strdup(vm_files->files[fd].host_cpath);
        target_file->clone_blob.file_type = vm_files->files[fd].clone_blob.file_type;
        target_file->clone_blob.flags = vm_files->files[fd].clone_blob.flags;
        target_file->clone_blob.mode = vm_files->files[fd].clone_blob.mode;
    }
    return target_file->fd;
}

static uint32_t
do_fcntl_getfd(struct hart * hartptr, uint32_t fd)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    return ERRNO(fcntl(vm_files->files[fd].host_fd, F_GETFD));
}

static uint32_t
do_fcntl_getfl(struct hart * hartptr, uint32_t fd)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    return ERRNO(fcntl(vm_files->files[fd].host_fd, F_GETFL));
}

static uint32_t
do_fcntl_setfd(struct hart * hartptr, uint32_t fd, uint32_t opaque)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    return ERRNO(fcntl(vm_files->files[fd].host_fd, F_SETFD, opaque));
}

uint32_t
call_fnctl(struct hart * hartptr, uint32_t fd, uint32_t cmd, uint32_t opaque)
{
    switch(cmd)
    {
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
            return do_fcntl_duplicate_fd(hartptr, fd, cmd, opaque);
            break;
        case F_GETFD:
            return do_fcntl_getfd(hartptr, fd);
            break;
        case F_GETFL:
            return do_fcntl_getfl(hartptr, fd);
            break;
        case F_SETFD:
            return do_fcntl_setfd(hartptr, fd, opaque);
            break;
        default:
            log_fatal("not recognized fcntl command:%d see full list:./include/uapi/asm-generic/fcntl.h\n", cmd);
            break;
    }
    return -ENOSYS;
}

uint32_t
call_dup(struct hart * hartptr, uint32_t fd)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (fd > MAX_FILES_NR || !vm_files->files[fd].valid) {
        return -EBADF;
    }
    struct file * target_file = allocate_file_descriptor(vm_files);
    if (!target_file) {
        return -ENOMEM;
    }

    target_file->host_fd = dup(vm_files->files[fd].host_fd);
    if (vm_files->files[fd].host_cpath) {
        target_file->host_cpath = strdup(vm_files->files[fd].host_cpath);
        target_file->clone_blob.file_type = vm_files->files[fd].clone_blob.file_type;
        target_file->clone_blob.flags = vm_files->files[fd].clone_blob.flags;
        target_file->clone_blob.mode = vm_files->files[fd].clone_blob.mode;
    }

    return target_file->fd;
}

uint32_t
call_dup3(struct hart * hartptr, uint32_t old_fd, uint32_t new_fd,
          uint32_t flags)
{
    struct virtual_machine * vm_files = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_FILES);
    if (old_fd > MAX_FILES_NR || !vm_files->files[old_fd].valid) {
        return -EBADF;
    }

    if (new_fd > MAX_FILES_NR) {
        return -EBADF;
    }

    if (new_fd == old_fd) {
        return 0;
    }

    if (vm_files->files[new_fd].valid) {
        do_close(hartptr, new_fd);
    }

    vm_files->files[new_fd].valid = 1;
    vm_files->files[new_fd].fd = new_fd;
    vm_files->files[new_fd].host_fd = dup(vm_files->files[old_fd].host_fd);
    if (vm_files->files[old_fd].host_cpath) {
        vm_files->files[new_fd].host_cpath = strdup(vm_files->files[old_fd].host_cpath);
        vm_files->files[new_fd].clone_blob.file_type = vm_files->files[old_fd].clone_blob.file_type;
        vm_files->files[new_fd].clone_blob.mode = vm_files->files[old_fd].clone_blob.mode;
        vm_files->files[new_fd].clone_blob.flags = vm_files->files[old_fd].clone_blob.flags;
    }
    return new_fd;
}

void
dump_file_descriptors(struct virtual_machine * vm)
{
    struct virtual_machine * vm_files = get_linked_vm(vm, LINKAGE_HINT_FILES);
    struct virtual_machine * vm_fs = get_linked_vm(vm, LINKAGE_HINT_FS);
    log_info("dump file descriptors\n");
    log_info("\troot: %s\n", vm_fs->root);
    log_info("\tcwd : %s\n", vm_fs->cwd);
    int idx = 0;
    for (idx = 0; idx < MAX_FILES_NR; idx++) {
        if (!vm_files->files[idx].valid) {
            continue;
        }
        log_info("\tfd:%-2d host_fd:%-2d  filepath:%s\n",
                 vm_files->files[idx].fd, vm_files->files[idx].host_fd,
                 vm_files->files[idx].host_cpath);
    }
}

void
vfs_init(struct virtual_machine * vm)
{
    struct hart * hartptr = hart_by_id(vm, 0);
    ASSERT(0 == do_openat(hartptr, AT_FDCWD, "/dev/stdin", O_RDONLY, 0));
    ASSERT(1 == do_openat(hartptr, AT_FDCWD, "/dev/stdout", O_WRONLY, 0));
    ASSERT(2 == do_openat(hartptr, AT_FDCWD, "/dev/stderr", O_WRONLY, 0));
}

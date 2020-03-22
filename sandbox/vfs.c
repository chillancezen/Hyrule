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

#define MAX_PATH 512
 
static int
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
        if (src[idx] != '/' && src[idx])
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
    ASSERT(target_file->ref_count == 0);
    if (target_file->guest_cpath) {
        free(target_file->guest_cpath);
    }
    memset(target_file, 0x0, sizeof(struct file));
}

#define AT_FDCWD -100
uint32_t
do_openat(struct hart * hartptr, uint32_t dirfd, const char * guest_path,
          uint32_t flags, uint32_t mode)
{
    struct virtual_machine * vm = hartptr->vmptr;
    // judge whether a relative path is given.
    char * ptr = (char *)guest_path;
    for(;*ptr && *ptr == ' '; ptr++);
    int is_relative_path = *ptr != '/';
    const char * relative_dir_path = "";

    uint8_t guest_cpath[MAX_PATH];
    ASSERT(!canonicalize_path_name(guest_cpath, (const uint8_t *)guest_path));
    if (is_relative_path) {
        if ((int32_t)dirfd == AT_FDCWD) {
            relative_dir_path = vm->cwd;
        } else {
            // FIXME:
            __not_reach();
        }
    }
    // comprise host absolute file path.
    uint8_t host_path[MAX_PATH];
    uint8_t host_cpath[MAX_PATH];
    ptr = (char *)host_path;
    ptr = append_string(ptr, (const char *)vm->root);
    ptr = append_string(ptr, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    // setup an unoccupied file descriptor
    struct file * new_file = allocate_file_descriptor(vm);
    if (!new_file) {
        return -ENOMEM;
    }
    int host_fd = open((char *)host_cpath, flags, mode);
    if (host_fd < 0) {
        deallocate_file_descriptor(vm, new_file);
        return host_fd;
    }
    new_file->host_fd = host_fd;
    new_file->ref_count += 1;
    uint8_t guest_abs_path[MAX_PATH];
    uint8_t guest_abs_cpath[MAX_PATH];
    ptr = append_string((char *)guest_abs_path, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(guest_abs_cpath, guest_abs_path));
    new_file->guest_cpath = strdup((const char *)guest_abs_cpath);

    return new_file->fd;
}

uint32_t
do_close(struct hart * hartptr, uint32_t fd)
{
    struct virtual_machine * vm = hartptr->vmptr;
    if (fd > MAX_FILES_NR || !vm->files[fd].valid ||
        vm->files[fd].closed) {
        return -EBADF;
    }
    uint32_t rc = close(vm->files[fd].host_fd);

    vm->files[fd].closed = 1;
    vm->files[fd].ref_count -= 1;
   
    if (vm->files[fd].ref_count <= 0) {
        deallocate_file_descriptor(vm, &vm->files[fd]);   
    }
    return rc;
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
    struct virtual_machine * vm = hartptr->vmptr;
    char * ptr = pathname;
    for(;*ptr && *ptr == ' '; ptr++);
    int is_relative_path = *ptr != '/';
    const char * relative_dir_path = "";

    uint8_t guest_cpath[MAX_PATH];
    ASSERT(!canonicalize_path_name(guest_cpath, (const uint8_t *)pathname));
    if (is_relative_path) {
        if ((int32_t)dirfd == AT_FDCWD) {
            relative_dir_path = vm->cwd;
        } else {
            // FIXME
        }
    }
    uint8_t host_path[MAX_PATH];
    uint8_t host_cpath[MAX_PATH];
    ptr = (char *)host_path;
    ptr = append_string(ptr, (const char *)vm->root);
    ptr = append_string(ptr, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    // XXX: with gcc 4.8, I can't find statx system call warpper in GLIBC,
    // so I use direct syscall.
    int rc = syscall(__NR_statx, AT_FDCWD, host_cpath, flag, mask, statxbuf);
    return rc;
}

uint32_t
do_readlinkat(struct hart * hartptr, uint32_t dirfd, const char * pathname,
              void * buf, uint32_t buf_size)
{
    struct virtual_machine * vm = hartptr->vmptr;
    char * ptr = (char *)pathname;
    for(;*ptr && *ptr == ' '; ptr++);
    int is_relative_path = *ptr != '/';
    const char * relative_dir_path = "";

    uint8_t guest_cpath[MAX_PATH];
    ASSERT(!canonicalize_path_name(guest_cpath, (const uint8_t *)pathname));
    if (is_relative_path) {
        if ((int32_t)dirfd == AT_FDCWD) {
            relative_dir_path = vm->cwd;
        } else {
            // FIXME
        }
    }

    uint8_t host_path[MAX_PATH];
    uint8_t host_cpath[MAX_PATH];
    ptr = (char *)host_path;
    ptr = append_string(ptr, (const char *)vm->root);
    ptr = append_string(ptr, (const char *)relative_dir_path);
    ptr = append_string(ptr, (const char *)guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    return readlinkat(AT_FDCWD, (const char *)host_cpath, buf, buf_size);
}
uint32_t
do_writev(struct hart * hartptr, uint32_t fd,
          struct iovec32 * guest_iov, uint32_t iovcnt)

{
    struct virtual_machine * vm = hartptr->vmptr;
    if (fd > MAX_FILES_NR || !vm->files[fd].valid ||
        vm->files[fd].closed) {
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
    uint32_t host_rc = writev(vm->files[fd].host_fd, host_vecbase, iovcnt);
    free(host_vecbase);
    return host_rc;
}

uint32_t
do_write(struct hart * hartptr, uint32_t fd, void * buf, uint32_t nr_write)
{
    struct virtual_machine * vm = hartptr->vmptr;
    if (fd > MAX_FILES_NR || !vm->files[fd].valid ||
        vm->files[fd].closed) {
        return -EBADF;
    }
    return write(vm->files[fd].host_fd, buf, nr_write);
}

uint32_t
do_read(struct hart * hartptr, uint32_t fd, void * buf, uint32_t nr_read)
{
    struct virtual_machine * vm = hartptr->vmptr;
    if (fd > MAX_FILES_NR || !vm->files[fd].valid ||
        vm->files[fd].closed) {
        return -EBADF;
    }
    return read(vm->files[fd].host_fd, buf, nr_read);
}

uint32_t
do_ioctl(struct hart * hartptr, uint32_t fd, uint32_t request,
         uint32_t argp_addr)
{
    struct virtual_machine * vm = hartptr->vmptr;
    if (fd > MAX_FILES_NR || !vm->files[fd].valid ||
        vm->files[fd].closed) {
        return -EBADF;
    }
    void * argp = NULL;
    if (user_accessible(hartptr, argp_addr)) {
        argp = user_world_pointer(hartptr, argp_addr);
    }
    return ioctl(vm->files[fd].host_fd, request, argp);
}

uint32_t
do_getdents64(struct hart * hartptr, uint32_t fd, uint32_t dirp_addr,
              uint32_t count)
{
    struct virtual_machine * vm = hartptr->vmptr;
    if (fd > MAX_FILES_NR || !vm->files[fd].valid ||
        vm->files[fd].closed) {
        return -EBADF;
    }
    void * dirp = user_world_pointer(hartptr, dirp_addr);
    return syscall(__NR_getdents64, vm->files[fd].host_fd, dirp, count);
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

    struct virtual_machine * vm = hartptr->vmptr;
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
    struct virtual_machine * vm = hartptr->vmptr;
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


void
dump_file_descriptors(struct virtual_machine * vm)
{
    log_info("dump file descriptors\n");
    int idx = 0;
    for (idx = 0; idx < MAX_FILES_NR; idx++) {
        if (!vm->files[idx].valid) {
            continue;
        }
        log_info("\tfd:%-2d filepath:%s\n", vm->files[idx].fd, vm->files[idx].guest_cpath);
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

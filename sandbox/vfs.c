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
    const char * ptr = guest_path;
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
    sprintf((char *)host_path, "%s/%s/%s", vm->root, relative_dir_path, guest_cpath);
    ASSERT(!canonicalize_path_name(host_cpath, host_path));

    // setup an unoccupied file descriptor
    struct file * new_file = allocate_file_descriptor(vm);
    if (!new_file) {
        return -ENOMEM;
    }
    int host_fd = open((char *)host_cpath, flags, mode);
    if (new_file->host_fd < 0) {
        deallocate_file_descriptor(vm, new_file);
        return host_fd;
    }
    new_file->host_fd = host_fd;
    new_file->ref_count += 1;
    uint8_t guest_abs_path[MAX_PATH];
    uint8_t guest_abs_cpath[MAX_PATH];
    sprintf((char *)guest_abs_path, "%s/%s", relative_dir_path, guest_cpath);
    ASSERT(!canonicalize_path_name(guest_abs_cpath, guest_abs_path));
    new_file->guest_cpath = strdup((const char *)guest_abs_cpath);

    return new_file->fd;
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
        return -EINVAL;
    }
    return write(vm->files[fd].host_fd, buf, nr_write);
}

void
vfs_init(struct virtual_machine * vm)
{
    struct hart * hartptr = hart_by_id(vm, 0);
    ASSERT(0 == do_openat(hartptr, AT_FDCWD, "/dev/stdin", O_RDONLY, 0));
    ASSERT(1 == do_openat(hartptr, AT_FDCWD, "/dev/stdout", O_WRONLY, 0));
    ASSERT(2 == do_openat(hartptr, AT_FDCWD, "/dev/stderr", O_WRONLY, 0));
}

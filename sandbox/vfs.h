/*
 * Copyright (c) 2020 Jie Zheng
 */
#ifndef _VFS_H
#define _VFS_H
#include <stdint.h>
struct virtual_machine;
struct hart;

struct file {
    uint32_t fd;
    uint32_t host_fd;
    uint32_t external_fd;
    uint32_t ref_count;

    uint32_t valid:1;
    uint32_t external:1;
    uint32_t closed:1;

    char * guest_cpath;
};


struct iovec32 {
    uint32_t iov_base;
    uint32_t iov_len;
}__attribute__((packed));

void
vfs_init(struct virtual_machine * vm);

uint32_t
do_openat(struct hart * hartptr, uint32_t dirfd, const char * guest_path,
          uint32_t flags, uint32_t mode);

uint32_t
do_writev(struct hart * hartptr, uint32_t fd,
          struct iovec32 * guest_iov, uint32_t iovcnt);

uint32_t
do_write(struct hart * hartptr, uint32_t fd, void * buf, uint32_t nr_write);

#endif

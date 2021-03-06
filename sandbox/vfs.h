/*
 * Copyright (c) 2020 Jie Zheng
 */
#ifndef _VFS_H
#define _VFS_H
#include <stdint.h>
struct virtual_machine;
struct hart;

enum FILE_TYPE {
    FILE_TYPE_ERROR,
    FILE_TYPE_REGULAR,
    FILE_TYPE_SOCKET,
};
struct file {
    int32_t fd;
    int32_t host_fd;
    //int32_t external_fd;
    //uint32_t ref_count;

    uint32_t valid:1;
    //uint32_t external:1;
    //uint32_t closed:1;

    char * host_cpath;

    struct {
        uint32_t flags;
        uint32_t mode;
        enum FILE_TYPE file_type;        
    }clone_blob;
};


struct iovec32 {
    uint32_t iov_base;
    uint32_t iov_len;
}__attribute__((packed));

#define MAX_PATH 512

int
canonicalize_path_name(uint8_t * dst, const uint8_t * src);

void
vfs_init(struct virtual_machine * vm);

uint32_t
do_openat(struct hart * hartptr, uint32_t dirfd, const char * guest_path,
          uint32_t flags, uint32_t mode);

uint32_t
do_close(struct hart * hartptr, uint32_t fd);

uint32_t
do_statx(struct hart * hartptr, uint32_t dirfd, char * pathname, uint32_t flag,
         uint32_t mask, void * statxbuf);

uint32_t
do_writev(struct hart * hartptr, uint32_t fd,
          struct iovec32 * guest_iov, uint32_t iovcnt);

uint32_t
do_write(struct hart * hartptr, uint32_t fd, void * buf, uint32_t nr_write);

uint32_t
do_mmap(struct hart* hartptr, uint32_t proposal_addr, uint32_t len,
        uint32_t prot, uint32_t flags, uint32_t fd, uint32_t offset);

uint32_t
do_munmap(struct hart * hartptr, uint32_t addr, uint32_t len);

uint32_t
do_ioctl(struct hart * hartptr, uint32_t fd, uint32_t request,
         uint32_t argp_addr);

uint32_t
do_read(struct hart * hartptr, uint32_t fd, void * buf, uint32_t nr_read);

uint32_t
do_getdents64(struct hart * hartptr, uint32_t fd, uint32_t dirp_addr,
              uint32_t count);
uint32_t
do_readlinkat(struct hart * hartptr, uint32_t dirfd, const char * pathname_addr,
              void * buf, uint32_t buf_size);

uint32_t
do_sendfile(struct hart * hartptr, uint32_t out_fd, uint32_t fd,
            void * offset, uint32_t count);

uint32_t
do_unlinkat(struct hart * hartptr, uint32_t dirfd,
            const char * pathname, uint32_t flags);

uint32_t
do_lseek(struct hart * hartptr, uint32_t fd, uint32_t offset, uint32_t whence);

uint32_t
call_fnctl(struct hart * hartptr, uint32_t fd, uint32_t cmd, uint32_t opaque);

uint32_t
call_dup(struct hart * hartptr, uint32_t fd);

uint32_t
call_dup3(struct hart * hartptr, uint32_t old_fd, uint32_t new_fd,
          uint32_t flags);

void
dump_file_descriptors(struct virtual_machine * vm);

#endif

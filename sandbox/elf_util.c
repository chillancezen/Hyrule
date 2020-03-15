/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <elf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <util.h>

// return fd of open file, value less than 0 is returned upon error
int
elf_open(const char * elf_host_path)
{
    int fd = open(elf_host_path, O_RDONLY);
    return fd;
}

// 0 <-> success
int
elf_read(int fd, void * buffer, int offset, int size)
{
    lseek(fd, offset, SEEK_SET);
    int nr_read = read(fd, buffer, size);
    return nr_read != size;
}

int
elf_header(int fd, struct elf32_elf_header * blob)
{
    int rc = elf_read(fd, blob, 0, sizeof(struct elf32_elf_header));
    if (!rc) {
        // DO ELF HEADER FORMAT VALIDATION
        #define _(cond)                                                        \
            rc |= !(cond);
        _(ELF32_IDENTITY_ELF ==*(uint32_t *)blob->e_ident);
        _(ELF32_CLASS_ELF32 == blob->e_ident[4]);
        _(ELF32_MACHINE_RISCV == blob->e_machine);
        #undef _
    }
    return rc;
}

int
elf_program_header(int fd,  const struct elf32_elf_header * elf_hdr,
                   struct elf32_program_header * prog_hdr, int index)
{
    if (index >= elf_hdr->e_phnum) {
        return -1;
    }
    ASSERT(sizeof(struct elf32_program_header) == elf_hdr->e_phentsize);
    int offset = elf_hdr->e_phoff + index * elf_hdr->e_phentsize;
    return elf_read(fd, prog_hdr, offset, elf_hdr->e_phentsize);
}

void
elf_close(int fd)
{
    close(fd);
}

/*
 * Copyright (c) 2018-2020 Jie Zheng
 */
#ifndef _ELF_H
#define _ELF_H
#include <stdint.h>

/*
 * I borrow the definition from /uer/include/elf.h
 */
#define EI_NIDENT 16

struct elf32_elf_header{
    uint8_t e_ident[EI_NIDENT]; /* Magic number and other info */
    uint16_t e_type;         /* Object file type */
    uint16_t e_machine;      /* Architecture */
    uint32_t e_version;      /* Object file version */
    uint32_t e_entry;        /* Entry point virtual address */
    uint32_t e_phoff;        /* Program header table file offset */
    uint32_t e_shoff;        /* Section header table file offset */
    uint32_t e_flags;        /* Processor-specific flags */
    uint16_t e_ehsize;       /* ELF header size in bytes */
    uint16_t e_phentsize;    /* Program header table entry size */
    uint16_t e_phnum;        /* Program header table entry count */
    uint16_t e_shentsize;    /* Section header table entry size */
    uint16_t e_shnum;        /* Section header table entry count */
    uint16_t e_shstrndx;     /* Section header string table index */
};

struct elf32_section_header {
    uint32_t sh_name;        /* Section name (string tbl index) */
    uint32_t sh_type;        /* Section type */
    uint32_t sh_flags;       /* Section flags */
    uint32_t sh_addr;        /* Section virtual addr at execution */
    uint32_t sh_offset;      /* Section file offset */
    uint32_t sh_size;        /* Section size in bytes */
    uint32_t sh_link;        /* Link to another section */
    uint32_t sh_info;        /* Additional section information */
    uint32_t sh_addralign;   /* Section alignment */
    uint32_t sh_entsize;     /* Entry size if section holds table */
};

struct elf32_program_header{
    uint32_t p_type;         /* Segment type */
    uint32_t p_offset;       /* Segment file offset */
    uint32_t p_vaddr;        /* Segment virtual address */
    uint32_t p_paddr;        /* Segment physical address */
    uint32_t p_filesz;       /* Segment size in file */
    uint32_t p_memsz;        /* Segment size in memory */
    uint32_t p_flags;        /* Segment flags */
    uint32_t p_align;        /* Segment alignment */
};

#define ELF32_IDENTITY_ELF 0x464c457f
#define ELF32_MACHINE_RISCV 0xf3
#define ELF32_CLASS_ELF32 0x1
#define ELF32_ENDIAN_LITTLE 0x1

#define ELF32_TYPE_EXEC 0x2
#define ELF32_MACHINE_I386 0x3 

#define PROGRAM_TYPE_LOAD 1
#define PROGRAM_READ (1 << 2)
#define PROGRAM_WRITE (1 << 1)
#define PROGRAM_EXECUTE (1 << 0)

int
elf_open(const char * elf_host_path);

void
elf_close(int fd);

int
elf_read(int fd, void * buffer, int offset, int size);

int
elf_header(int fd, struct elf32_elf_header * blob);

int
elf_program_header(int fd,  const struct elf32_elf_header * elf_hdr,
                   struct elf32_program_header * prog_hdr, int index);

#endif

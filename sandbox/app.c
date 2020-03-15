/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <app.h>
#include <string.h>
#include <util.h>
#include <elf.h>

static void
cpu_init(struct virtual_machine * vm)
{
    vm->nr_harts = 1;
    vm->boot_hart = 0;

    vm->harts = aligned_alloc(64, vm->nr_harts * sizeof(struct hart));
    ASSERT(vm->harts);

    // FIXME:hart->pc will be loaded when resolving the elf file
    struct hart * hartptr = hart_by_id(vm, 0);
    hart_init(hartptr, 0);
    hartptr->vmptr = vm;
    
    // XXX:application always works in machine mode, no mmu is needed.
    // TLB is not initialized.
}

void *
vma_generic_direct(uint64_t addr, struct hart * hartptr,
                   struct pm_region_operation * pmr)
{
    void * memory_access_base = pmr->host_base + (addr - pmr->addr_low);
    return memory_access_base;
}

uint64_t
vma_generic_read(uint64_t addr, int access_size, struct hart * hartptr,
                 struct pm_region_operation * pmr)
{
    uint64_t val = 0;
    void * memory_access_base = pmr->host_base + (addr - pmr->addr_low);
    switch (access_size)
    {
#define _(size, type)                                                          \
        case size:                                                             \
            val = *(type *)memory_access_base;                                 \
            break
        _(1, uint8_t);
        _(2, uint16_t);
        _(4, uint32_t);
        _(8, uint64_t);
        default:
            __not_reach();
            break;
#undef _
    }
    return val;
}

void
vma_generic_write(uint64_t addr, int access_size, uint64_t value, struct hart * hartptr,
                  struct pm_region_operation * pmr)
{
    void * memory_access_base = pmr->host_base + (addr - pmr->addr_low);
    switch (access_size)
    {
#define _(size, type)                                                          \
        case size:                                                             \
            *(type *)memory_access_base = (type)value;                         \
            break
        _(1, uint8_t);
        _(2, uint16_t);
        _(4, uint32_t);
        _(8, uint64_t);
        default:
            __not_reach();
            break;
#undef _
    }
}

#define DEFAULT_STACK_SIZE  (1024 * 1024 * 8)
#define DEFAULT_STACK_CELIING 0xE0000000


static void
stack_init(struct virtual_machine * vm)
{
    void * host_stack_base = preallocate_physical_memory(DEFAULT_STACK_SIZE);
    struct pm_region_operation pmr = {
        .addr_low = DEFAULT_STACK_CELIING - DEFAULT_STACK_SIZE,
        .addr_high = DEFAULT_STACK_CELIING,
        .flags = PROGRAM_READ | PROGRAM_WRITE,
        .pmr_read = vma_generic_read,
        .pmr_write = vma_generic_write,
        .pmr_direct = vma_generic_direct,
        .host_base = host_stack_base,
    };
    sprintf(pmr.pmr_desc, "stack[%08x-%08x].RW", pmr.addr_low, pmr.addr_high);
    hart_by_id(vm, 0)->registers.sp = DEFAULT_STACK_CELIING - 0x100;
    register_pm_region_operation(vm, &pmr);
}

static void
program_init(struct virtual_machine * vm, const char * app_path)
{
    int fd_app = elf_open(app_path);
    ASSERT(fd_app >= 0);

    struct elf32_elf_header elf_hdr;
    ASSERT(!elf_header(fd_app, &elf_hdr));
    hart_by_id(vm, 0)->pc = elf_hdr.e_entry;

    
    int idx = 0;
    for (idx = 0; idx < elf_hdr.e_phnum; idx++) {
        struct elf32_program_header prog_hdr;
        ASSERT(!elf_program_header(fd_app, &elf_hdr, &prog_hdr, idx));
        if (prog_hdr.p_type != PROGRAM_TYPE_LOAD) {
            continue;
        }
        void * host_base = preallocate_physical_memory(prog_hdr.p_memsz);
        ASSERT(!(elf_read(fd_app, host_base, prog_hdr.p_offset, prog_hdr.p_filesz)));

        struct pm_region_operation pmr = {
            .addr_low = prog_hdr.p_vaddr,
            .addr_high = prog_hdr.p_vaddr + prog_hdr.p_memsz,
            .flags = prog_hdr.p_flags,
            .pmr_read = vma_generic_read,
            .pmr_write = vma_generic_write,
            .pmr_direct = vma_generic_direct,
            .host_base = host_base,
        };
        sprintf(pmr.pmr_desc, "elf[%08x-%08x].%s%s%s", pmr.addr_low, pmr.addr_high,
                prog_hdr.p_flags & PROGRAM_READ ? "R" : "",
                prog_hdr.p_flags & PROGRAM_WRITE ? "W" : "",
                prog_hdr.p_flags & PROGRAM_EXECUTE ? "X" : "");
        register_pm_region_operation(vm, &pmr);
    }
    stack_init(vm);
    dump_memory_regions(vm);
}

void
application_sandbox_init(struct virtual_machine * vm, const char * app_path)
{
    memset(vm, 0x0, sizeof(struct virtual_machine));

    cpu_init(vm);
    program_init(vm, app_path);
}


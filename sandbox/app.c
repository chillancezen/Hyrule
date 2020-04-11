/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <app.h>
#include <string.h>
#include <util.h>
#include <elf.h>
#include <uaccess.h>
#include <vfs.h>
#include <unistd.h>
#include <stdlib.h>
#include <tinyprintf.h>

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

void
vma_generic_reclaim(void * opaque, struct hart * hartptr,
                    struct pm_region_operation * pmr)
{
    free(pmr->host_base);
}

#define DEFAULT_STACK_SIZE  (1024 * 1024 * 8)
#define DEFAULT_STACK_CELIING 0xE0000000


static void
stack_init(struct virtual_machine * vm)
{
    void * host_stack_base = preallocate_physical_memory(DEFAULT_STACK_SIZE);
    ASSERT(host_stack_base);
    struct pm_region_operation pmr = {
        .addr_low = DEFAULT_STACK_CELIING - DEFAULT_STACK_SIZE,
        .addr_high = DEFAULT_STACK_CELIING,
        .flags = PROGRAM_READ | PROGRAM_WRITE,
        .pmr_read = vma_generic_read,
        .pmr_write = vma_generic_write,
        .pmr_direct = vma_generic_direct,
        .pmr_reclaim = vma_generic_reclaim,
        .host_base = host_stack_base,
        .opaque = NULL,
    };
    sprintf(pmr.pmr_desc, "stack[%08x-%08x].RW", pmr.addr_low, pmr.addr_high);
    hart_by_id(vm, 0)->registers.sp = DEFAULT_STACK_CELIING - 0x100;
    register_pm_region_operation(vm, &pmr);

    vm->vma_stack = search_pm_region_callback(vm, pmr.addr_low);
    ASSERT(vm->vma_stack);
}

static void
heap_init(struct virtual_machine * vm, uint32_t proposed_prog_break)
{
    uint32_t prog_break = proposed_prog_break + 4096 * 16;
    // round up to page alignment.
    prog_break &= ~(4095);
    struct pm_region_operation pmr = {
        .addr_low = prog_break - 1,
        .addr_high = prog_break,
        .flags = PROGRAM_READ | PROGRAM_WRITE,
        .pmr_read = vma_generic_read,
        .pmr_write = vma_generic_write,
        .pmr_direct = vma_generic_direct,
        .pmr_reclaim = vma_generic_reclaim,
        .host_base = NULL,
        .opaque = NULL,
    };
    sprintf(pmr.pmr_desc, "heap[%08x-%08x].RW", pmr.addr_low, pmr.addr_high);
    register_pm_region_operation(vm, &pmr);

    vm->vma_heap = search_pm_region_callback(vm, prog_break - 1);
    ASSERT(vm->vma_heap);
}

void
mmap_setup(struct virtual_machine * vm, uint32_t addr_low, uint32_t len,
           uint32_t flags, void * host_base)
{
    if (!host_base) {
        host_base = preallocate_physical_memory(len);
    }
    struct pm_region_operation pmr = {
        .addr_low = addr_low,
        .addr_high = addr_low + len,
        .flags = flags,
        .pmr_read = vma_generic_read,
        .pmr_write = vma_generic_write,
        .pmr_direct = vma_generic_direct,
        .pmr_reclaim = vma_generic_reclaim,
        .host_base = host_base,
        .opaque = NULL,
    };
    tfp_sprintf(pmr.pmr_desc, "mmap[%08x-%08x].RW", pmr.addr_low, pmr.addr_high);
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

    uint32_t prog_break = 0; 
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
            .pmr_reclaim = vma_generic_reclaim,
            .host_base = host_base,
            .opaque = NULL,
        };
        sprintf(pmr.pmr_desc, "elf[%08x-%08x].%s%s%s", pmr.addr_low, pmr.addr_high,
                prog_hdr.p_flags & PROGRAM_READ ? "R" : "",
                prog_hdr.p_flags & PROGRAM_WRITE ? "W" : "",
                prog_hdr.p_flags & PROGRAM_EXECUTE ? "X" : "");
        register_pm_region_operation(vm, &pmr);

        if (pmr.addr_high > prog_break) {
            prog_break = pmr.addr_high;
        }
    }
    stack_init(vm);
    heap_init(vm, prog_break);
    dump_memory_regions(vm);
}

#define MAX_NR_ENVP 128
#define MAX_NR_ARGV 128

static void
env_setup(struct virtual_machine * vm, char ** argv, char ** envp)
{
    struct hart * hartptr = hart_by_id(vm, 0);
    uint32_t stack_top = hartptr->registers.sp;
    uint32_t stack_top_gap = 0;
    void * stack_top_upointer = user_world_pointer(hartptr, stack_top);
    ASSERT(stack_top_upointer);
    #define PUSH_BLOB(blob, len) {                                             \
        stack_top_upointer -= (len);                                           \
        stack_top_gap += (len);                                                \
        memcpy(stack_top_upointer, (blob), (len));                             \
    }

    #define LOCAL_ALIGN(align) {                                               \
        uint32_t __riscv_stack_top = stack_top - stack_top_gap;                \
        uint32_t __riscv_stack_aligned = __riscv_stack_top & (~((align) - 1)); \
        stack_top_gap += __riscv_stack_top - __riscv_stack_aligned;            \
        stack_top_upointer -= __riscv_stack_top - __riscv_stack_aligned;       \
    }

    #define STACK_POS() ({                                                     \
        stack_top - stack_top_gap;                                             \
    })

    int idx = 0;

    // copy all environment variables strings on to the stack.
    uint32_t env_pointer[MAX_NR_ENVP];
    uint32_t nr_env = 0;
    for (idx = 0; idx < MAX_NR_ENVP && envp[idx]; idx++) {
        int len = strlen(envp[idx]);
        PUSH_BLOB(envp[idx], len + 1);
        env_pointer[nr_env] = STACK_POS();
        nr_env++;
    }

    // copy all arguments strings onto the stack.
    uint32_t arg_pointer[MAX_NR_ARGV];
    uint32_t nr_arg = 0;
    for (idx = 0; idx < MAX_NR_ARGV && argv[idx]; idx++) {
        int len = strlen(argv[idx]);
        PUSH_BLOB(argv[idx], len + 1);
        arg_pointer[nr_arg] = STACK_POS();
        nr_arg++;
    }

    LOCAL_ALIGN(16);

    // put auxiliary variables onto the stack.
    struct {
        uint32_t a_type;
        uint32_t a_value;
    }__attribute__((packed)) aux_vector[] = {
        {25, stack_top}, // XXX:random. GLIBC needs it.
        {9, hartptr->pc}, // entry point
        {0, 0}, // end of vector
    };
    int nr_aux = sizeof(aux_vector)/sizeof(aux_vector[0]);   
    for (idx = nr_aux - 1; idx >=0; idx--) {
        PUSH_BLOB(&aux_vector[idx], sizeof(aux_vector[idx]));  
    }

    // put all env pointers onto the stack.
    uint32_t null_terminator = 0;
    PUSH_BLOB(&null_terminator, 4);
    for (idx = nr_env - 1; idx >= 0; idx--) {
        PUSH_BLOB(&env_pointer[idx], 4);
    }

    // put all arg pointers onto the stack
    PUSH_BLOB(&null_terminator, 4);
    for (idx = nr_arg - 1; idx >= 0; idx--) {
        PUSH_BLOB(&arg_pointer[idx], 4);
    }

    // put number of arg onto the stack
    PUSH_BLOB(&nr_arg, 4);

    hartptr->registers.sp = STACK_POS();
    #undef PUSH_BLOB
    #undef LOCAL_ALIGN
    #undef STACK_POS
}


static void
app_root_init(struct virtual_machine * vm)
{
    char * root = getenv("ROOT");
    if (!root) {
        log_fatal("please sepcify environment variable: ROOT\n");
        exit(-1);
    }
    vm->root = root;
}

static uint32_t gpid_counter = 1;

static void
misc_init(struct virtual_machine * vm)
{
    vm->pid = gpid_counter;
    gpid_counter += 1;

    // FIXME: fix the parent pid here
    vm->ppid = vm->pid;

    strcpy(vm->cwd, "/");
}

void
application_sandbox_init(struct virtual_machine * vm, const char * app_path,
                         char ** argv, char ** envp)
{
    memset(vm, 0x0, sizeof(struct virtual_machine));

    cpu_init(vm);
    program_init(vm, app_path);
    env_setup(vm, argv, envp);

    app_root_init(vm);
    vfs_init(vm);

    misc_init(vm);
}


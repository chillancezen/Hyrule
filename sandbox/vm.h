/*
 * Copyright (c) 2019 Jie Zheng
 */

#ifndef _VM_H
#define _VM_H
#include <stdint.h>
#include <hart.h>
#include <physical_memory.h>
#include <util.h>
#include <fdt.h>
#include <ini.h>
#include <pm_region.h>
#include <vfs.h>
#include <list.h>

#define MAX_VMA_NR  128
#define MAX_FILES_NR   128

struct virtual_machine {

    //int nr_harts;
    //int boot_hart;
    // XXX:due to historical reason: harts must be deprecated. one struct vm
    // corresponds to one struct hart.
    //struct hart * harts;
    struct hart * hartptr;
    // main memory
    //int64_t main_mem_size;
    //int64_t main_mem_base;
    //void * main_mem_host_base;
    
    // bootrom
    //int64_t bootrom_size;
    //int64_t bootrom_base;
    //void * bootrom_host_base;

    // the build buffer of FDT
    //struct fdt_build_blob fdt;

    // the ini conifguration
    //ini_t * ini_config;

    // XXX: CLONE_VM shares below area
    // VMA regions: here we reuse existing data structure: pm_region_operation
    int nr_pmr_ops;
    struct pm_region_operation  pmr_ops[MAX_VMA_NR];
    //struct list_elem pmr_head;
    struct pm_region_operation * vma_heap;
    struct pm_region_operation * vma_stack;
    
    // XXX: CLONE_FILES shares below area
    // files operation
    struct file files[MAX_FILES_NR];

    // XXX: CLONE_FS shares below area
    char root[128];
    char cwd[128];

    uint32_t pid;   // ID of the process
    uint32_t ppid;  // ID of the parent process, if ppid == pid, the process tree terminates upward
    uint32_t tgid;  // ID of the shared thread group

    // the parent_vm can be searched via pid, however, we put the pointer here
    // to speed up the process to search the parent
    struct virtual_machine * parent_vm;
    // here are the flags of vm cloning
    uint32_t cloned_vm:1;
    uint32_t cloned_files:1;
    uint32_t cloned_fs:1;

    // if a child has something to share with parent, the parent's ref_count has
    // to be incremented. only ref_count reaches to 0, the struct can be released.
    int32_t ref_count;

    // two addresses maintained per-thread(task)
    // http://man7.org/linux/man-pages/man2/set_tid_address.2.html
    uint32_t set_child_tid;
    uint32_t clear_child_tid;

    struct list_elem list_node;    
};

enum linkage_hint {
    LINKAGE_HINT_NATIVE,
    LINKAGE_HINT_VM,
    LINKAGE_HINT_FILES,
    LINKAGE_HINT_FS,
};

static inline struct virtual_machine *
get_linked_vm(struct virtual_machine * current, enum linkage_hint hint)
{
    struct virtual_machine * vm = current;
    while (1) {
        int should_go = 0;
        switch(hint)
        {
            case LINKAGE_HINT_VM:
                should_go = !!vm->cloned_vm;
                break;
            case LINKAGE_HINT_FS:
                should_go = !!vm->cloned_fs;
                break;
            case LINKAGE_HINT_FILES:
                should_go = !!vm->cloned_files;
                break;
            default:
                break;
        }
        if (should_go) {
            ASSERT(vm->parent_vm);
            vm = vm->parent_vm;
        } else {
            break;
        }
    }
    return vm;
}

static inline void
reference_task(struct virtual_machine * vm)
{
    vm->ref_count += 1;
}

static inline void
deference_task(struct virtual_machine * vm)
{
    vm->ref_count -= 1;
}
__attribute__((always_inline))
static inline struct hart *
hart_by_id(struct virtual_machine * vm, int hart_id)
{
    // XXX: 1:1 mapping.
    //ASSERT(hart_id >= 0 && hart_id < vm->nr_harts);
    //return &vm->harts[hart_id];
    return vm->hartptr;
}


#define MEGA(nr_mega) (1024 * 1024 * (nr_mega))

#define IMAGE_TYPE_BINARY 0x1
#define IMAGE_TYPE_ELF 0x2

void
virtual_machine_init(struct virtual_machine * vm, ini_t *);

void
bootrom_init(struct virtual_machine * vm);

void
ram_init(struct virtual_machine * vm);

void
uart_init(struct virtual_machine * vm);

void
clint_init(struct virtual_machine * vm);

#endif

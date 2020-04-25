/*
 * Copyright (c) 2020 Jie Zheng
 */

#include <task.h>
#include <list.h>
#include <vm.h>
#include <log.h>

#define __USE_GNU
#include <sched.h>

#include <fcntl.h>
#include <unistd.h>

static struct list_elem global_task_list_head;


void
dump_threads(struct hart * hartptr)
{
    struct virtual_machine * vm = hartptr->native_vmptr;
    log_info("Dump process: %d\n", vm->pid);
    log_info("\tparent pid: %d\n", vm->ppid);
    log_info("\tthread group id: %d\n", vm->tgid);
    log_info("\tflag: CLONE_VM:%d CLONE_FS:%d CLONE_FILES:%d\n",
             vm->cloned_vm, vm->cloned_fs, vm->cloned_files);
    log_info("\tthread group members:\n");
    struct list_elem * list;
    LIST_FOREACH_START(&global_task_list_head, list) {
        struct virtual_machine * _vm = CONTAINER_OF(list, struct virtual_machine, list_node);
        if (_vm->tgid != vm->tgid) {
            continue;
        }
        log_info("\t    thread: %d\n", _vm->pid);
    }
    LIST_FOREACH_END();
}
static struct virtual_machine *
allocate_virtual_machine_descriptor(void)
{
    struct virtual_machine * vm = aligned_alloc(64, sizeof(struct virtual_machine));
    ASSERT(vm);
    memset(vm, 0x0, sizeof(struct virtual_machine));
    return vm;
}

static void
task_cpu_init(struct virtual_machine * current_vm,
              struct virtual_machine * child_vm)
{
    child_vm->hartptr = aligned_alloc(64, sizeof(struct hart));
    ASSERT(child_vm->hartptr);

    hart_init(child_vm->hartptr, 0);
    child_vm->hartptr->native_vmptr = child_vm;

    // copy and modify cpu state
    memcpy(&child_vm->hartptr->registers,
           &current_vm->hartptr->registers,
           sizeof(struct integer_register_profile));
    child_vm->hartptr->registers.a0 = 0;
    child_vm->hartptr->pc = current_vm->hartptr->pc + 4;
}

static void
duplicate_virtual_memory(struct virtual_machine * current_vm,
                         struct virtual_machine * child_vm)
{
    int idx = 0;
    child_vm->nr_pmr_ops = current_vm->nr_pmr_ops;
    for (idx = 0; idx < current_vm->nr_pmr_ops; idx++) {
        // XXX: COW semantics are not implemented here for that No single page
        // tracking is in this emulator 
        memcpy(&child_vm->pmr_ops[idx],
               &current_vm->pmr_ops[idx],
               sizeof(struct pm_region_operation));
        int pmr_len = child_vm->pmr_ops[idx].addr_high - child_vm->pmr_ops[idx].addr_low;
        child_vm->pmr_ops[idx].host_base = preallocate_physical_memory(pmr_len);
        ASSERT(child_vm->pmr_ops[idx].host_base);
        memcpy(child_vm->pmr_ops[idx].host_base,
               current_vm->pmr_ops[idx].host_base,
               pmr_len);
        if (&current_vm->pmr_ops[idx] == current_vm->vma_heap) {
            child_vm->vma_heap = &child_vm->pmr_ops[idx];   
        }
        if (&current_vm->pmr_ops[idx] == current_vm->vma_stack) {
            child_vm->vma_stack = &child_vm->pmr_ops[idx];
        }
    }
}

static void
task_vm_init(struct virtual_machine * current_vm,
             struct virtual_machine * child_vm, uint32_t flags)
{
    if (flags & CLONE_VM) {
        // If child process shares virtual machine with parent process, we don't
        // allocate vma delicated to child process.
        child_vm->cloned_vm = 1;
        reference_task(current_vm);    
    } else {
        // XXX: duplicate everything of virtual memory from parent process.
        duplicate_virtual_memory(current_vm, child_vm);
    }
}

static void
task_linkage_init(struct virtual_machine * current_vm,
                  struct virtual_machine * child_vm, uint32_t flags)
{
    child_vm->pid = new_pid();
    child_vm->ppid = current_vm->pid;
    
    if (flags & CLONE_THREAD) {
        child_vm->tgid = current_vm->pid;
    } else {
        child_vm->tgid = child_vm->pid;
    }

    child_vm->parent_vm = current_vm;
    reference_task(current_vm);
}

static void
task_fs_init(struct virtual_machine * current_vm,
             struct virtual_machine * child_vm, uint32_t flags)
{
    if (flags & CLONE_FS) {
        child_vm->cloned_fs = 1;
        reference_task(current_vm);
    } else {
        memcpy(child_vm->root, current_vm->root, sizeof(child_vm->root));
        memcpy(child_vm->cwd, current_vm->cwd, sizeof(child_vm->cwd));
    }
}


static void
duplicate_files(struct virtual_machine * current_vm,
                struct virtual_machine * child_vm)
{
    int idx = 0;
    for (idx = 0; idx < MAX_FILES_NR; idx++) {
        if (!current_vm->files[idx].valid) {
            child_vm->files[idx].valid = 0;
            continue;
        }

        child_vm->files[idx].valid = 1;
        child_vm->files[idx].fd = current_vm->files[idx].fd;
        if (current_vm->files[idx].host_cpath) {
            child_vm->files[idx].host_cpath = strdup(current_vm->files[idx].host_cpath);
        }
        child_vm->files[idx].clone_blob.file_type = current_vm->files[idx].clone_blob.file_type;
        child_vm->files[idx].clone_blob.mode = current_vm->files[idx].clone_blob.mode;
        child_vm->files[idx].clone_blob.flags = current_vm->files[idx].clone_blob.flags;
        
        switch(child_vm->files[idx].clone_blob.file_type)
        {
            case FILE_TYPE_REGULAR:
                ASSERT(child_vm->files[idx].host_cpath);
                child_vm->files[idx].host_fd = open(child_vm->files[idx].host_cpath,
                                                    child_vm->files[idx].clone_blob.flags,
                                                    child_vm->files[idx].clone_blob.mode);
                break;
            case FILE_TYPE_SOCKET:
                child_vm->files[idx].host_fd = dup(current_vm->files[idx].host_fd);
                break;
            default:
                __not_reach();
                break;
        }
    }
}

static void
task_files_init(struct virtual_machine * current_vm,
                struct virtual_machine * child_vm,
                uint32_t flags)
{
    if (flags & CLONE_FILES) {
        child_vm->cloned_files = 1;
        reference_task(current_vm);
    } else {
        duplicate_files(current_vm, child_vm);
    }
}

#include <translation.h>

/*
 * This will duplicate all information of a task including cpu state information.
 * it's usually called when creating a process.
 */
uint32_t
clone_task(struct virtual_machine * current_vm, uint32_t flags)
{
    struct virtual_machine * child_vm = allocate_virtual_machine_descriptor();

    task_linkage_init(current_vm, child_vm, flags);
    task_cpu_init(current_vm, child_vm);
    task_vm_init(current_vm, child_vm, flags);
    task_fs_init(current_vm, child_vm, flags);
    task_files_init(current_vm, child_vm, flags);
    register_task(child_vm);
    vmresume(child_vm->hartptr);
    return child_vm->pid;
}

void
register_task(struct virtual_machine * vm)
{
    list_append(&global_task_list_head, &vm->list_node);
    log_info("registering task:%d\n", vm->pid);
}


__attribute__((constructor))
static void task_pre_init(void)
{
    list_init(&global_task_list_head);
}

// GLIBC: ./sysdeps/unix/sysv/linux/bits/sched.h
//        ./sysdeps/unix/sysv/linux/riscv/clone.S
uint32_t
call_clone(struct hart * hartptr, uint32_t flags, uint32_t child_stack_addr,
           uint32_t ptid_addr, uint32_t tls_addr, uint32_t ctid_addr)
{
    struct virtual_machine * current = hartptr->native_vmptr;
    log_debug("call_clone:{flags:%x child_stack_addr:%x ptid_addr:%x, "
              "tls_addr:%x ctid_addr:%x}\n", flags, child_stack_addr,
              ptid_addr, tls_addr, ctid_addr);
    
    return clone_task(current, flags);
}

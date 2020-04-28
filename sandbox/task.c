/*
 * Copyright (c) 2020 Jie Zheng
 */

#include <task.h>
#include <task_sched.h>
#include <list.h>
#include <vm.h>
#include <log.h>

#define __USE_GNU
#include <sched.h>

#include <fcntl.h>
#include <unistd.h>
#include <uaccess.h>
#include <tinyprintf.h>
#include <app.h>

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

void
reclaim_virtual_memory(struct virtual_machine * vm)
{
    if (vm->cloned_vm) {
        ASSERT(vm->parent_vm);
        deference_task(vm->parent_vm);
        vm->cloned_vm = 0;
    } else {
        int idx = 0;
        for (idx = 0; idx < vm->nr_pmr_ops; idx++) {
            if (vm->pmr_ops[idx].pmr_reclaim) {
                vm->pmr_ops[idx].pmr_reclaim(vm->pmr_ops[idx].opaque,
                                             vm->hartptr,
                                             &vm->pmr_ops[idx]);
            }
        }
        vm->nr_pmr_ops = 0;
        vm->vma_heap = NULL;
        vm->vma_stack = NULL;
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
#include <vmm_sched.h>
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
    // XXX: now the child process is able to be scheduled.
    task_vmm_sched_init(child_vm->hartptr,
                        (void (*)(void *))vmresume,
                        child_vm->hartptr);
    schedule_task(child_vm->hartptr);
    //yield_cpu();
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

#define MAX_NR_ARGV 128
#define MAX_NR_ENVP 128

uint32_t
call_execve(struct hart * hartptr,
            uint32_t filename_addr,
            uint32_t argv_addr,
            uint32_t envp_addr)
{
    // XXX: we only receive binary executable file
    char * filename = strdup(user_world_pointer(hartptr, filename_addr));
    uint32_t * argv = user_world_pointer(hartptr, argv_addr);
    uint32_t * envp = user_world_pointer(hartptr, envp_addr);
    __attribute__((unused)) char * host_argv[MAX_NR_ARGV];
    __attribute__((unused)) char * host_envp[MAX_NR_ENVP];
    int idx = 0;
    for (idx = 0; idx < MAX_NR_ARGV - 1; idx++) {
        if (!argv[idx]) {
            break;   
        }
        host_argv[idx] = strdup((char *)user_world_pointer(hartptr, argv[idx]));
    }
    host_argv[idx] = NULL;

    for (idx = 0; idx < MAX_NR_ENVP; idx++) {
        if (!envp[idx]) {
            break;
        }
        host_envp[idx] = strdup((char *)user_world_pointer(hartptr, envp[idx]));
    }
    host_envp[idx] = NULL;

    // Now all paremteres that host can see are all ready
    // XXX: even execevi() is called in thread context, it works well.
    struct virtual_machine * vm_vm = get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_VM);
    // Now it's safe to release the virtual memory allocated for previous task,
    // because we do backup all these strings.
    reclaim_virtual_memory(vm_vm);


    // canonicalize filename
    char host_path[MAX_PATH];
    char host_cpath[MAX_PATH];
    {
        const char * ptr = filename;
        for (; *ptr && *ptr == ' '; ptr++);
        int is_absolute = *ptr == '/';
        
        char guest_cpath[MAX_PATH];
        if (is_absolute) {
            canonicalize_path_name((uint8_t *)guest_cpath, (const uint8_t *)ptr);
        } else {
            char guest_path[MAX_PATH];
            tfp_sprintf(guest_path, "%s/%s", vm_vm->cwd, ptr);
            canonicalize_path_name((uint8_t *)guest_cpath, (const uint8_t *)guest_path);
        }
        tfp_sprintf(host_path, "%s/%s", vm_vm->root, guest_cpath);
        canonicalize_path_name((uint8_t *)host_cpath, (const uint8_t *)host_path);
    }
    // XXX: MUST flush the translation cache because the instruction stream has
    // changed.
    reset_registers(hartptr);
    flush_translation_cache(hartptr);

    program_init(vm_vm, host_cpath);
    env_setup(vm_vm, host_argv, host_envp);
    
    free(filename);
    for (idx = 0; idx < MAX_NR_ARGV && host_argv[idx]; idx++) {
        free(host_argv[idx]);
    }
    for (idx = 0; idx < MAX_NR_ENVP && host_envp[idx]; idx++) {
        free(host_envp[idx]);
    }
   
    // you might yield cpu here, but it's not necessary.
    yield_cpu(); 
    // note a successful execve() all never return.
    vmresume(hartptr);
    return 0;
}
uint32_t
call_wait4(struct hart * hartptr,
           int32_t pid,
           uint32_t wstatus_addr,
           uint32_t options,
           uint32_t rusage_addr)
{
    // FIXME: implement the wait body
    transit_state(hartptr, TASK_STATE_INTERRUPTIBLE);
    yield_cpu();
    return 0;
}

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
#include <wait_queue.h>

static struct list_elem global_task_list_head;

uint8_t *
task_state_to_string(enum task_state stat);


void
raw_task_wake_up(struct hart * task)
{
    log_debug("wake up process:%d current status:%s\n",
              task->native_vmptr->pid,
              task_state_to_string(task->state));

    switch(task->state)
    {
        case TASK_STATE_RUNNING:
            break;
        case TASK_STATE_INTERRUPTIBLE:
            transit_state(task, TASK_STATE_RUNNING);
            break;
        case TASK_STATE_UNINTERRUPTIBLE:
            // In an `TASK_STATE_UNINTERRUPTIBLE` state, the previous
            // `non_stop_state` must be in `TASK_STATE_INTERRUPTIBLE`, we
            // restore it to RUNNING state, but do not run the task
            // immediately. it muust be continued with explicit SIGCONT signal.
            ASSERT(task->non_stop_state == TASK_STATE_RUNNING ||
                   task->non_stop_state == TASK_STATE_INTERRUPTIBLE);
            task->non_stop_state = TASK_STATE_RUNNING;
            break;
        default:
            log_fatal("current task state:%d\n", task->state);
            __not_reach();
            break;
    }
}

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

static void
task_threads_init(struct virtual_machine * current_vm,
                  struct virtual_machine * child_vm,
                  uint32_t child_stack,
                  uint32_t flags,
                  uint32_t ctid_addr)
{
    if (flags & CLONE_CHILD_SETTID) {
        child_vm->set_child_tid = ctid_addr;
        log_debug("clone pid:%d set_child_tid address:0x%x\n",
                  child_vm->pid, child_vm->set_child_tid);
    }

    // IF a thread context is created, the stack must be changed to the new one
    if (flags & CLONE_THREAD) {
        ASSERT(child_stack);
        child_vm->hartptr->registers.sp = child_stack;
    }
}

#include <translation.h>
#include <vmm_sched.h>
/*
 * This will duplicate all information of a task including cpu state information.
 * it's usually called when creating a process.
 */
static uint32_t
clone_task(struct virtual_machine * current_vm,
           uint32_t child_stack,
           uint32_t flags,
           uint32_t ptid_addr,
           uint32_t tls_addr,
           uint32_t ctid_addr)
{
    struct virtual_machine * child_vm = allocate_virtual_machine_descriptor();

    task_linkage_init(current_vm, child_vm, flags);
    task_cpu_init(current_vm, child_vm);
    task_vm_init(current_vm, child_vm, flags);
    task_fs_init(current_vm, child_vm, flags);
    task_files_init(current_vm, child_vm, flags);
    task_threads_init(current_vm, child_vm, child_stack, flags, ctid_addr);

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
    log_info("registering task:%d into global tasklist\n", vm->pid);
}


void
unregister_task(struct virtual_machine * vm)
{
    if (element_in_list(&global_task_list_head, &vm->list_node)) {
        list_delete(&global_task_list_head, &vm->list_node);
        log_info("unregistering task:%d from global tasklist\n", vm->pid);
    }
}

__attribute__((constructor))
static void task_pre_init(void)
{
    list_init(&global_task_list_head);
}


void
do_exit(struct hart * hartptr, uint32_t status)
{

    hartptr->wait_state_exited = 1;
    
    wake_up(&hartptr->wq_state_notification);
    transit_state(hartptr, TASK_STATE_EXITING);
    yield_cpu();

    // This task will never be rescheduled.
    __not_reach();
}


uint32_t
call_set_tid_address(struct hart * hartptr, uint32_t tid_address)
{
    struct virtual_machine * vm = hartptr->native_vmptr;
    vm->set_child_tid = tid_address;
    log_debug("set_tid_address pid:%d, address:0x%x\n",
              vm->pid, vm->set_child_tid);
    return vm->pid;
}

uint32_t
call_set_robust_list(struct hart * hartptr, uint32_t head_addr,
                     uint32_t len)
{
    // FIXME: implement the call body
    return 0;
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
    
    return clone_task(current, child_stack_addr, flags, ptid_addr, tls_addr, ctid_addr);
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

    log_debug("execve %s in process:%d\n", host_cpath, hartptr->native_vmptr->pid);
     
    // you might yield cpu here, but it's not necessary.
    yield_cpu(); 
    // note a successful execve() will never return.
    vmresume(hartptr);
    return 0;
}

// wait for the children processes' state changed:
// 1) the child is terminated
// 2) the child is stoppped by a signal
// 3) the child is resumed by a signal
// *** wait a child terminated to release all the resources for the child
// without wait: the child remains in a 'zombie' state
// if child's state has changed: these calls return immediately

#define WNOHANG_MASK     0x1   /* Don't block waiting.  */
#define WUNTRACED_MASK   0x2   /* Report status of stopped children.  */

struct wait_blob {
    struct virtual_machine * vm;
    struct wait_queue wait;
    struct list_elem list_node;
};

uint32_t
call_wait4(struct hart * hartptr,
           int32_t pid_hint,
           uint32_t wstatus_addr,
           uint32_t options,
           uint32_t rusage_addr)
{
    int32_t ret = -1;
    struct virtual_machine * target_vm = NULL;
    struct virtual_machine * vm = hartptr->native_vmptr;
    struct list_elem * list;
    struct list_elem wait_blob_head;
    list_init(&wait_blob_head);
    
    LIST_FOREACH_START(&global_task_list_head, list) {
        struct virtual_machine * _vm = CONTAINER_OF(list,
                                                    struct virtual_machine,
                                                    list_node);
        if (vm->pid != _vm->ppid || vm->pid == _vm->pid) {
            continue;
        }

        int is_to_wait = 0;
        if (pid_hint == -1) {
            // wait for any child process
            is_to_wait = vm->pid == _vm->ppid;
        } else if (pid_hint < -1) {
            // wait if child's tgid is equal to abs(pid_hint)
            is_to_wait = _vm->tgid == -pid_hint;
        } else if (!pid_hint) {
            // wait if child's tgid is equal to the pid of calling process
            is_to_wait = _vm->tgid == vm->pid;
        } else {
            is_to_wait = _vm->pid == pid_hint;
        }
        
        if (!is_to_wait) {
            continue;
        }

        // the child is waitable, adjust the ret value in case WNOHANG is specified
        ret = 0;

        // Found one that has exited
        if (_vm->hartptr->wait_state_exited) {
            ret = _vm->pid;
            target_vm = _vm;
            break;
        }

        if (!(options & WNOHANG_MASK)) {
            // hang until at lease one child state changed.
            struct wait_blob * _wait_blob = malloc(sizeof(struct wait_blob));
            ASSERT(_wait_blob);
            memset(_wait_blob, 0x0, sizeof(struct wait_blob));
            _wait_blob->vm = _vm;
            initialize_wait_queue_entry(&_wait_blob->wait, hartptr);
            list_append(&wait_blob_head, &_wait_blob->list_node);
            add_wait_queue_entry(&_vm->hartptr->wq_state_notification, &_wait_blob->wait);
        }
    }
    LIST_FOREACH_END();

    // FIXME: handle pending signals
    if (!list_empty(&wait_blob_head)) {
        transit_state(hartptr, TASK_STATE_INTERRUPTIBLE);
        yield_cpu();
    }

    while ((list = list_fetch(&wait_blob_head))) {
        struct wait_blob * _wait_blob = CONTAINER_OF(list,
                                                     struct wait_blob,
                                                     list_node);
        if (ret <= 0) {
            if (_wait_blob->vm->hartptr->wait_state_exited ||
                (options & WUNTRACED_MASK && (
                 _wait_blob->vm->hartptr->wait_state_continued ||
                 _wait_blob->vm->hartptr->wait_state_stopped))) {
                ret = _wait_blob->vm->pid;
                target_vm = _wait_blob->vm;
            }
        }
        remove_wait_queue_entry(&_wait_blob->vm->hartptr->wq_state_notification,
                                &_wait_blob->wait);
        free(_wait_blob);
    }
    log_debug("call_wait4, caller's process id: %d result:%d\n",
              vm->pid, ret);
    
    if (ret > 0) {
        // remove the process entry in case the child becomes a zombie
        ASSERT(target_vm);
        unregister_task(target_vm);
    }

    return ret;
}

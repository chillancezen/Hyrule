/*
 * Copyright (c) 2020 Jie Zheng
 */

#include <task.h>
#include <list.h>
#include <vm.h>
#include <log.h>

static struct list_elem global_task_list_head;


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
uint32_t
call_clone(struct hart * hartptr, uint32_t flags, uint32_t child_stack_addr,
           uint32_t ptid_addr, uint32_t tls_addr, uint32_t ctid_addr)
{

    printf("pc:%x\n", hartptr->pc);
    printf("flags:%x\n", flags);
    printf("child_stack_addr:%x\n", child_stack_addr);
    printf("ptid_addr:%x\n", ptid_addr);
    printf("tls_addr:%x\n", tls_addr);
    printf("ctid_addr:%x\n", ctid_addr);
    return -EINVAL;
}

/*
 * Copyright (c) 2020 Jie Zheng
 */

#include <task_sched.h>
#include <vm.h>
#include <task.h>

// REF: https://github.com/chillancezen/ZeldaOS/blob/master/kernel/task.c

struct hart * current;

static struct list_elem running_tasks;
static struct list_elem blocking_tasks;
static struct list_elem exiting_tasks;

static enum task_state transition_table[TASK_STATE_MAX][TASK_STATE_MAX];

__attribute__((constructor)) static void
task_state_transition_init(void)
{
#define _(state1, state2) transition_table[(state1)][(state2)] = 1
    memset(transition_table, 0x0, sizeof(transition_table));
    _(TASK_STATE_ZOMBIE, TASK_STATE_ZOMBIE);
    _(TASK_STATE_RUNNING, TASK_STATE_RUNNING);
    _(TASK_STATE_EXITING, TASK_STATE_EXITING);
    _(TASK_STATE_INTERRUPTIBLE, TASK_STATE_INTERRUPTIBLE);
    _(TASK_STATE_UNINTERRUPTIBLE, TASK_STATE_UNINTERRUPTIBLE);
    _(TASK_STATE_RUNNING, TASK_STATE_INTERRUPTIBLE);
    _(TASK_STATE_RUNNING, TASK_STATE_UNINTERRUPTIBLE);
    _(TASK_STATE_RUNNING, TASK_STATE_EXITING);
    _(TASK_STATE_INTERRUPTIBLE, TASK_STATE_RUNNING);
    _(TASK_STATE_INTERRUPTIBLE, TASK_STATE_UNINTERRUPTIBLE);
    _(TASK_STATE_UNINTERRUPTIBLE, TASK_STATE_RUNNING);
    _(TASK_STATE_UNINTERRUPTIBLE, TASK_STATE_INTERRUPTIBLE);
#undef _
}

uint8_t *
task_state_to_string(enum task_state stat)
{
    uint8_t * str = (uint8_t *)"UNKNOWN";
    switch(stat)
    {
        case TASK_STATE_ZOMBIE:
            str = (uint8_t *)"TASK_STATE_ZOMBIE";
            break;
        case TASK_STATE_EXITING:
            str = (uint8_t *)"TASK_STATE_EXITING";
            break;
        case TASK_STATE_INTERRUPTIBLE:
            str = (uint8_t *)"TASK_STATE_INTERRUPTIBLE";
            break;
        case TASK_STATE_UNINTERRUPTIBLE:
            str = (uint8_t *)"TASK_STATE_UNINTERRUPTIBLE";
            break;
        case TASK_STATE_RUNNING:
            str = (uint8_t *)"TASK_STATE_RUNNING";
            break;
        default:
            __not_reach();
            break;
    }
    return str;
}

void
transit_state(struct hart * hartptr, enum task_state target_state)
{
    enum task_state prev_state;
    prev_state = hartptr->state;
    hartptr->state = target_state;
    ASSERT(target_state < TASK_STATE_MAX);
    ASSERT(transition_table[prev_state][target_state]);
    log_debug("Process:0x%x state transition from:%s to %s\n",
              hartptr->native_vmptr->pid,
              task_state_to_string(prev_state),
              task_state_to_string(target_state));
}


void
schedule_task(struct hart * hartptr)
{
    switch(hartptr->state)
    {
        case TASK_STATE_RUNNING:
            list_append(&running_tasks, &hartptr->list);
            break;
        case TASK_STATE_INTERRUPTIBLE:
        case TASK_STATE_UNINTERRUPTIBLE:
            list_append(&blocking_tasks, &hartptr->list);
            break;
        case TASK_STATE_EXITING:
            list_append(&exiting_tasks, &hartptr->list);
            break;
        default:
            __not_reach();
            break;
    }
    struct virtual_machine * vm = hartptr->native_vmptr;
    log_debug("scheduling task:%d target state: %s\n", vm->pid,
              task_state_to_string(hartptr->state));
}

void
process_blocking_list(void)
{
    struct list_elem * _list = NULL;
    struct hart * hartptr = NULL;
    LIST_FOREACH_START(&blocking_tasks, _list) {
        hartptr = CONTAINER_OF(_list, struct hart, list);
        switch(hartptr->state)
        {
            case TASK_STATE_RUNNING:
            case TASK_STATE_EXITING:
                list_delete(&blocking_tasks, _list);
                schedule_task(hartptr);
                break;
            case TASK_STATE_INTERRUPTIBLE:
            case TASK_STATE_UNINTERRUPTIBLE:
                break;
            default:
                __not_reach();
                break;
        }
    }
    LIST_FOREACH_END();
}

void
process_exiting_list(void)
{
    struct list_elem * list = NULL;
    __attribute__((unused)) struct hart * hartptr = NULL;

    while ((list = list_fetch(&exiting_tasks))) {
        hartptr = CONTAINER_OF(list, struct hart, list);
        // FIXME: reclaim resources of the hart and vm

        //unregister_task(hartptr->native_vmptr);        
    }
}

struct hart *
process_running_list(void)
{
    struct list_elem * list = NULL;
    struct hart * hartptr = NULL;
    struct hart * next_task = NULL;
    while ((list = list_fetch(&running_tasks))) {
        hartptr = CONTAINER_OF(list, struct hart, list);
        if (hartptr->state == TASK_STATE_RUNNING) {
            next_task = hartptr;
            break;
        } else {
            schedule_task(hartptr);
        }
    }
    return next_task;
}

__attribute__((constructor)) void
sched_pre_init(void)
{
    list_init(&running_tasks);
    list_init(&blocking_tasks);
    list_init(&exiting_tasks);
}

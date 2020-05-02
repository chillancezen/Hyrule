/*
 * Copyright (c) 2018-2020 Jie Zheng
 */

#include <wait_queue.h>
#include <util.h>
#include <task.h>

void
initialize_wait_queue_head(struct wait_queue_head * head)
{
    list_init(&head->pivot);
}


void
initialize_wait_queue_entry(struct wait_queue * entry, struct hart * task)
{
    list_init(&entry->list);
    entry->task = task;
}

/*
 * This is reentrant......
 */
void
add_wait_queue_entry(struct wait_queue_head * head, struct wait_queue * entry)
{
    if (!element_in_list(&head->pivot, &entry->list)) {
        list_append(&head->pivot, &entry->list);
    }
}


void
remove_wait_queue_entry(struct wait_queue_head * head,
    struct wait_queue * entry)
{
    if (element_in_list(&head->pivot, &entry->list)) {
        list_delete(&head->pivot, &entry->list);
    }
}

void
wake_up(struct wait_queue_head * head)
{
    struct list_elem * _list = NULL;
    struct wait_queue * entry;
    LIST_FOREACH_START(&head->pivot, _list) {
        entry = CONTAINER_OF(_list, struct wait_queue, list);
        ASSERT(entry->task);
        raw_task_wake_up(entry->task);
    }
    LIST_FOREACH_END();
}

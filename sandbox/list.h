/*
 * Copyright (c) 2018-2020 Jie Zheng
 */
#ifndef _LIST_H
#define _LIST_H
#include <stdint.h>

/*
 * CAVEATS: Do *NOT* modify the struct of list_elem, otherwise it may impact
 * Zelda Drive serialization. 
 */
struct list_elem {
    struct list_elem * prev;
    struct list_elem * next;
}__attribute__((packed));


#define list_init(head) {\
    (head)->prev = NULL; \
    (head)->next = NULL; \
}

#define list_last_elem(head) ((head)->prev)
#define list_first_elem(head) ((head)->next)

#define list_empty(head) (!((head)->next))

#define list_node_detached(head, node) (!(node)->prev &&\
    !(node)->next && \
    list_first_elem(head) != (node))

/*
 * Put an element at the tail of the list
 */
void list_append(struct list_elem * head, struct list_elem * elem);
/*
 * Put an element at the front of the list
 */ 
void list_prepend(struct list_elem * head, struct list_elem * elem);
/*
 * Pop an element at the tail of the list
 * return NULL if the list is empty
 */
struct list_elem * list_pop(struct list_elem * head);
/*
 * fetch an element at the front of the list
 * return NULL if the list is empty
 */
struct list_elem * list_fetch(struct list_elem * head);

/*
 * delete an element at any place,
 * it will panic if the elem is not in the list
 */
void list_delete(struct list_elem * head, struct list_elem * elem);

#define LIST_FOREACH_START(head, elem) { \
    struct list_elem * __elem = (head)->next; \
    struct list_elem * __next = NULL; \
    for(; __elem; __elem = __next) { \
        __next = __elem->next; \
        (elem) = __elem;

#define LIST_FOREACH_END() }}

int32_t
element_in_list(struct list_elem * head, struct list_elem * elem);

#endif

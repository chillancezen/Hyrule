/*
 * Copyright (c) 2020 Jie Zheng
 */

#ifndef _UACCESS_H
#define _UACCESS_H
#include <vm.h>
#include <hart.h>

static inline int
user_accessible(struct hart * hartptr, uint32_t uaddress)
{
    struct pm_region_operation * pmr =
        search_pm_region_callback(get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_VM), uaddress);
    return pmr && pmr->pmr_direct;
}

static inline void *
user_world_pointer(struct hart * hartptr, uint32_t uaddress)
{
    struct pm_region_operation * pmr =
        search_pm_region_callback(get_linked_vm(hartptr->native_vmptr, LINKAGE_HINT_VM), uaddress);
    if (!pmr || !pmr->pmr_direct) {
        dump_hart(hartptr);
        __not_reach();
    }
    return pmr->pmr_direct(uaddress, hartptr, pmr);
}


#endif

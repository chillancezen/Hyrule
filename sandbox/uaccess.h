/*
 * Copyright (c) 2020 Jie Zheng
 */

#ifndef _UACCESS_H
#define _UACCESS_H
#include <vm.h>
#include <hart.h>

static inline void *
user_world_pointer(struct hart * hartptr, uint32_t uaddress)
{
    struct pm_region_operation * pmr =
        search_pm_region_callback(hartptr->vmptr, uaddress);
    ASSERT(pmr && pmr->pmr_direct);
    return pmr->pmr_direct(uaddress, hartptr, pmr);
}

#endif
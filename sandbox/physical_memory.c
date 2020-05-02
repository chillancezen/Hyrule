/*
 * Copyright (c) 2019 Jie Zheng
 */
#include <physical_memory.h>

#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

#define VMM_BASE_PAGE_SIZE 4096

void *
preallocate_physical_memory(int64_t nr_bytes)
{
    // FIXME: when loading an app, its .bss section must be cleared to zero,
    // but I didn't, instead, every time a region is allocated, the memory region
    // is filled out with zero.
    // To improve the performance, we MUST do .bss section clearance than this.
    void * rc = aligned_alloc(VMM_BASE_PAGE_SIZE, nr_bytes);
    if (rc) {
        memset(rc, 0x0, nr_bytes);
    }
    return rc;
}


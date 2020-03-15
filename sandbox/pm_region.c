/*
 * Copyright (c) 2019 Jie Zheng
 */
#include <pm_region.h>
#include <util.h>
#include <vm.h>

// XXX: Define MMIO operations globally on a per-vm basis. for simpicity purpose
// I don't put it in a VM's blob.
// XXX: remove global PM_regions. I put all them into vm's container.
//static struct pm_region_operation  pmr_ops[MAX_NR_PM_REGIONS];
//static int nr_pmr_ops = 0;

void
dump_memory_regions(struct virtual_machine * vm)
{
    int idx = 0;
    log_info("dump memory layout\n");
    for(idx = 0; idx < vm->nr_pmr_ops; idx++) {
        log_info("\t[0x%08x - 0x%08x] %s\n", vm->pmr_ops[idx].addr_low,
                 vm->pmr_ops[idx].addr_high, vm->pmr_ops[idx].pmr_desc);
    }
}
int
pm_region_operation_compare(const struct pm_region_operation * pmr1,
                            const struct pm_region_operation * pmr2)
{
    ASSERT(pmr1->addr_low <= pmr1->addr_high);
    ASSERT(pmr2->addr_low <= pmr2->addr_high);
    if (pmr1->addr_high <= pmr2->addr_low) {
        return -1;   
    }
    if (pmr1->addr_low >= pmr2->addr_high) {
        return 1;
    }

#if BUILD_TYPE == BUILD_TYPE_DEBUG
    if ((pmr1->addr_low <= pmr2->addr_low &&
         pmr1->addr_high >= pmr2->addr_high) ||
        (pmr2->addr_low <= pmr1->addr_low &&
         pmr2->addr_high >= pmr1->addr_high)) {
        return 0;
    }
    __not_reach();
    return 0;
#else
    return 0;
#endif
}


struct pm_region_operation *
search_pm_region_callback(struct virtual_machine * vm, uint64_t guest_pa)
{
    struct pm_region_operation target = {
        .addr_low = guest_pa,
        .addr_high = guest_pa + 1
    };
    struct pm_region_operation * rc;
    rc = SEARCH(struct pm_region_operation, vm->pmr_ops, vm->nr_pmr_ops,
                pm_region_operation_compare, &target);
    #if BUILD_TYPE == BUILD_TYPE_DEBUG
    if (!rc) {
        log_debug("can not find a memory for address: 0x%x\n", guest_pa);
    }
    #endif
    return rc;
}

void
register_pm_region_operation(struct virtual_machine * vm, const struct pm_region_operation * pmr)
{
    ASSERT(pmr->pmr_read && pmr->pmr_write);
    ASSERT(!search_pm_region_callback(vm, pmr->addr_low))
    ASSERT(vm->nr_pmr_ops < MAX_NR_PM_REGIONS);
    memcpy(&vm->pmr_ops[vm->nr_pmr_ops], pmr, sizeof(struct pm_region_operation));
    vm->nr_pmr_ops += 1;
    SORT(struct pm_region_operation, vm->pmr_ops, vm->nr_pmr_ops, pm_region_operation_compare);
    {
        ASSERT(search_pm_region_callback(vm, pmr->addr_low));
    }
}

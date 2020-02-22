/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <csr.h>


static void
csr_scounteren_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    csr->csr_blob = value;
    log_trace("hart id:%d pc:%08x, csr:scounteren write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
}

static uint32_t
csr_scounteren_read(struct hart *hartptr, struct csr_entry *csr)
{
    log_trace("hart id:%d pc:%08x, csr:scounteren read:0x%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    return csr->csr_blob;
}

static void
csr_scounteren_reset(struct hart *hartptr, struct csr_entry * csr)
{
    csr->csr_blob = 0x0;
}

static struct csr_registery_entry scounteren_csr_entry = {
    .csr_addr = 0x106,
    .csr_registery = {
        .wpri_mask = WPRI_MASK_ALL,
        .reset = csr_scounteren_reset,
        .read = csr_scounteren_read,
        .write = csr_scounteren_write
    }
};



#include <mmu.h>
#include <mmu_tlb.h>

static void
csr_satp_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    //uint32_t old_blob = csr->csr_blob;
    csr->csr_blob = value;
    log_trace("hart id:%d pc:%08x, csr:satp write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    // UPDATE: The PC is not changed, later we have to raise an exception
    // if it fails to translate the address.
    // THE PC IS CHANGED TO ITS VIRTUAL ADDRESS
    #if 0
    if (csr->csr_blob & 0x80000000 && (!(old_blob & 0x80000000))) {
        uint32_t va = 0;
        int rc = pa_to_va(hartptr, hartptr->pc, hartptr->itlb,hartptr->itlb_cap,
                          &va);
        ASSERT(!rc);
        hartptr->pc = va;
    }
    #endif
    invalidate_tlb(hartptr->itlb, hartptr->itlb_cap);
    invalidate_tlb(hartptr->dtlb, hartptr->dtlb_cap);
    flush_translation_cache(hartptr);
}

static uint32_t
csr_satp_read(struct hart *hartptr, struct csr_entry *csr)
{
    log_trace("hart id:%d pc:%08x, csr:satp read:0x%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    return csr->csr_blob;
}

static void
csr_satp_reset(struct hart *hartptr, struct csr_entry * csr)
{
    csr->csr_blob = 0x0;
}

static struct csr_registery_entry satp_csr_entry = {
    .csr_addr = CSR_ADDRESS_SATP,
    .csr_registery = {
        .wpri_mask = WPRI_MASK_ALL,
        .reset = csr_satp_reset,
        .read = csr_satp_read,
        .write = csr_satp_write
    }
};



static void
csr_sie_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    hartptr->ienable.bits.ssi = (value >> 1) & 0x1;
    hartptr->ienable.bits.sti = (value >> 5) & 0x1;
    hartptr->ienable.bits.sei = (value >> 9) & 0x1;
    log_trace("hart id:%d pc:%08x, csr:sie write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, value);
}

static uint32_t
csr_sie_read(struct hart *hartptr, struct csr_entry *csr)
{
    uint32_t blob = 0;
    blob |= (uint32_t)(hartptr->ienable.bits.usi) << 0;
    blob |= (uint32_t)(hartptr->ienable.bits.ssi) << 1;
    blob |= (uint32_t)(hartptr->ienable.bits.uti) << 4;
    blob |= (uint32_t)(hartptr->ienable.bits.sti) << 5;
    blob |= (uint32_t)(hartptr->ienable.bits.uei) << 8;
    blob |= (uint32_t)(hartptr->ienable.bits.sei) << 9;
    log_trace("hart id:%d pc:%08x, csr:sie read:0x%x\n",
              hartptr->hart_id, hartptr->pc, blob);
    return blob;
}


static struct csr_registery_entry sie_csr_entry = {
    .csr_addr = CSR_ADDRESS_SIE,
    .csr_registery = {
        .wpri_mask = 0x00000222,
        .read = csr_sie_read,
        .write = csr_sie_write
    }
};


static void
csr_sip_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    // ONLY SSIP is writable.
    hartptr->ipending.bits.ssi = (value >> 1) & 0x1;
    log_trace("hart id:%d pc:%08x, csr:sip write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, value);
}

static uint32_t
csr_sip_read(struct hart *hartptr, struct csr_entry *csr)
{
    uint32_t blob = 0;
    blob |= (uint32_t)(hartptr->ipending.bits.ssi) << 1;
    blob |= (uint32_t)(hartptr->ipending.bits.sti) << 5;
    blob |= (uint32_t)(hartptr->ipending.bits.sei) << 9;
    log_trace("hart id:%d pc:%08x, csr:sip read:0x%x\n",
              hartptr->hart_id, hartptr->pc, blob);
    return blob;
}


static struct csr_registery_entry sip_csr_entry = {
    .csr_addr = CSR_ADDRESS_SIP,
    .csr_registery = {
        .wpri_mask = 0x00000222,
        .read = csr_sip_read,
        .write = csr_sip_write
    }
};



static void
csr_sstatus_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    hartptr->status.uie = (value >> 0) & 0x1;
    hartptr->status.sie = (value >> 1) & 0x1;
    hartptr->status.upie = (value >> 4) & 0x1;
    hartptr->status.spie = (value >> 5) & 0x1;
    hartptr->status.spp = (value >> 8) & 0x1;
    log_trace("hart id:%d pc:%08x, csr:sstatus write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, value);
}

static uint32_t
csr_sstatus_read(struct hart *hartptr, struct csr_entry *csr)
{
    uint32_t blob = 0;
    blob |= (uint32_t)(hartptr->status.uie) << 0;
    blob |= (uint32_t)(hartptr->status.sie) << 1;
    blob |= (uint32_t)(hartptr->status.upie) << 4;
    blob |= (uint32_t)(hartptr->status.spie) << 5;
    blob |= (uint32_t)(hartptr->status.spp) << 8;
    log_trace("hart id:%d pc:%08x, csr:sstatus read:0x%x\n",
              hartptr->hart_id, hartptr->pc, blob);
    return blob;
}


static struct csr_registery_entry sstatus_csr_entry = {
    .csr_addr = CSR_ADDRESS_SSTATUS,
    .csr_registery = {
        .wpri_mask = 0x00000133,
        .read = csr_sstatus_read,
        .write = csr_sstatus_write
    }
};

static void
csr_stvec_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    csr->csr_blob = value;
    log_trace("hart id:%d pc:%08x, csr:stvec write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
}

static uint32_t
csr_stvec_read(struct hart *hartptr, struct csr_entry *csr)
{
    log_trace("hart id:%d pc:%08x, csr:stvec read:0x%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    return csr->csr_blob;
}

static void
csr_stvec_reset(struct hart *hartptr, struct csr_entry * csr)
{
    csr->csr_blob = 0x0;
}

static struct csr_registery_entry stvec_csr_entry = {
    .csr_addr = CSR_ADDRESS_STVEC,
    .csr_registery = {
        .wpri_mask = WPRI_MASK_ALL,
        .reset = csr_stvec_reset,
        .read = csr_stvec_read,
        .write = csr_stvec_write
    }
};

static void
csr_sscratch_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    csr->csr_blob = value;
    log_trace("hart id:%d pc:%08x, csr:sscratch write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
}

static uint32_t
csr_sscratch_read(struct hart *hartptr, struct csr_entry *csr)
{
    log_trace("hart id:%d pc:%08x, csr:sscratch read:0x%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    return csr->csr_blob;
}

static void
csr_sscratch_reset(struct hart *hartptr, struct csr_entry * csr)
{
    csr->csr_blob = 0x0;
}

static struct csr_registery_entry sscratch_csr_entry = {
    .csr_addr = CSR_ADDRESS_SSCRATCH,
    .csr_registery = {
        .wpri_mask = WPRI_MASK_ALL,
        .reset = csr_sscratch_reset,
        .read = csr_sscratch_read,
        .write = csr_sscratch_write
    }
};

static void
csr_sepc_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    csr->csr_blob = value;
    log_debug("hart id:%d pc:%08x, csr:sepc write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
}

static uint32_t
csr_sepc_read(struct hart *hartptr, struct csr_entry *csr)
{
    log_debug("hart id:%d pc:%08x, csr:sepc read:0x%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    return csr->csr_blob;
}

static void
csr_sepc_reset(struct hart *hartptr, struct csr_entry * csr)
{
    csr->csr_blob = 0x0;
}

static struct csr_registery_entry sepc_csr_entry = {
    .csr_addr = CSR_ADDRESS_SEPC,
    .csr_registery = {
        .wpri_mask = WPRI_MASK_ALL,
        .reset = csr_sepc_reset,
        .read = csr_sepc_read,
        .write = csr_sepc_write
    }
};

static void
csr_stval_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    csr->csr_blob = value;
    log_trace("hart id:%d pc:%08x, csr:stval write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
}

static uint32_t
csr_stval_read(struct hart *hartptr, struct csr_entry *csr)
{
    log_trace("hart id:%d pc:%08x, csr:stval read:0x%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    return csr->csr_blob;
}

static void
csr_stval_reset(struct hart *hartptr, struct csr_entry * csr)
{
    csr->csr_blob = 0x0;
}

static struct csr_registery_entry stval_csr_entry = {
    .csr_addr = CSR_ADDRESS_STVAL,
    .csr_registery = {
        .wpri_mask = WPRI_MASK_ALL,
        .reset = csr_stval_reset,
        .read = csr_stval_read,
        .write = csr_stval_write
    }
};

static void
csr_scause_write(struct hart *hartptr, struct csr_entry * csr, uint32_t value)
{
    csr->csr_blob = value;
    log_trace("hart id:%d pc:%08x, csr:scause write 0x:%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
}

static uint32_t
csr_scause_read(struct hart *hartptr, struct csr_entry *csr)
{
    log_trace("hart id:%d pc:%08x, csr:scause read:0x%x\n",
              hartptr->hart_id, hartptr->pc, csr->csr_blob);
    return csr->csr_blob;
}

static void
csr_scause_reset(struct hart *hartptr, struct csr_entry * csr)
{
    csr->csr_blob = 0x0;
}

static struct csr_registery_entry scause_csr_entry = {
    .csr_addr = CSR_ADDRESS_SCAUSE,
    .csr_registery = {
        .wpri_mask = WPRI_MASK_ALL,
        .reset = csr_scause_reset,
        .read = csr_scause_read,
        .write = csr_scause_write
    }
};

__attribute__((constructor)) static void
csr_supervisor_level_init(void)
{
    register_csr_entry(&scounteren_csr_entry);
    register_csr_entry(&satp_csr_entry);
    register_csr_entry(&sie_csr_entry);
    register_csr_entry(&sip_csr_entry);
    register_csr_entry(&sstatus_csr_entry);
    register_csr_entry(&stvec_csr_entry);
    register_csr_entry(&sscratch_csr_entry);
    register_csr_entry(&sepc_csr_entry);
    register_csr_entry(&stval_csr_entry);
    register_csr_entry(&scause_csr_entry);
}


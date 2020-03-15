/*
 * Copyright (c) 2019-2020 Jie Zheng
 */

#include <translation.h>
#include <util.h>
#include <debug.h>
#include <hart_util.h>
#include <mmu.h>
#include <mmu_tlb.h>

__attribute__((unused)) static void
ebreak_callback(struct hart * hartptr)
{
#if defined(NATIVE_DEBUGER)
    enter_vmm_dbg_shell(hartptr, 0);
#endif
}

void
riscv_ebreak_translator(struct decoding * dec, struct prefetch_blob * blob,
                        uint32_t instruction)
{
    uint32_t instruction_linear_address = blob->next_instruction_to_fetch;
    struct hart * hartptr = (struct hart *)blob->opaque;
    PRECHECK_TRANSLATION_CACHE(ebreak_instruction, blob);
    BEGIN_TRANSLATION(ebreak_instruction);
        __asm__ volatile("movq %%r12, %%rdi;"
                         "movq $ebreak_callback, %%rax;"
                         SAVE_GUEST_CONTEXT_SWITCH_REGS()
                         "call *%%rax;"
                         RESTORE_GUEST_CONTEXT_SWITCH_REGS()
                         PROCEED_TO_NEXT_INSTRUCTION()
                         END_INSTRUCTION(ebreak_instruction)
                         :
                         :
                         :"memory");
        BEGIN_PARAM_SCHEMA()
            PARAM32() /*dummy*/
        END_PARAM_SCHEMA()
    END_TRANSLATION(ebreak_instruction);
        BEGIN_PARAM(ebreak_instruction)
            instruction_linear_address
        END_PARAM()
    COMMIT_TRANSLATION(ebreak_instruction, hartptr, instruction_linear_address);
    blob->next_instruction_to_fetch += 4;
}

__attribute__((unused)) static void
mret_callback(struct hart * hartptr)
{
    assert_hart_running_in_mmode(hartptr);
    adjust_mstatus_upon_mret(hartptr);
    adjust_pc_upon_mret(hartptr);
    // translation cache must be flushed, because adjusted privilege level may
    // diff in addressing space
    flush_translation_cache(hartptr);
    
}

void
riscv_mret_translator(struct decoding * dec, struct prefetch_blob * blob,
                      uint32_t instruction)
{
    uint32_t instruction_linear_address = blob->next_instruction_to_fetch;
    struct hart * hartptr = (struct hart *)blob->opaque;
    PRECHECK_TRANSLATION_CACHE(mret_instruction, blob);
    BEGIN_TRANSLATION(mret_instruction);
    __asm__ volatile("movq %%r12, %%rdi;"
                     "movq $mret_callback, %%rax;"
                     SAVE_GUEST_CONTEXT_SWITCH_REGS()
                     "call *%%rax;"
                     RESTORE_GUEST_CONTEXT_SWITCH_REGS()
                     TRAP_TO_VMM(mret_instruction)
                     :
                     :
                     :"memory");
        BEGIN_PARAM_SCHEMA()
            PARAM32()
        END_PARAM_SCHEMA()
    END_TRANSLATION(mret_instruction);
        BEGIN_PARAM(mret_instruction)
            instruction_linear_address
        END_PARAM()
    COMMIT_TRANSLATION(mret_instruction, hartptr, instruction_linear_address);
    blob->is_to_stop = 1;
}

__attribute__((unused)) static void
sret_callback(struct hart * hartptr)
{
    assert_hart_running_in_smode(hartptr);
    adjust_mstatus_upon_sret(hartptr);
    adjust_pc_upon_sret(hartptr);
    flush_translation_cache(hartptr);

}

void
riscv_sret_translator(struct decoding * dec, struct prefetch_blob * blob,
                      uint32_t instruction)
{
    uint32_t instruction_linear_address = blob->next_instruction_to_fetch;
    struct hart * hartptr = (struct hart *)blob->opaque;
    PRECHECK_TRANSLATION_CACHE(sret_instruction, blob);
    BEGIN_TRANSLATION(sret_instruction);
    __asm__ volatile("movq %%r12, %%rdi;"
                     "movq $sret_callback, %%rax;"
                     SAVE_GUEST_CONTEXT_SWITCH_REGS()
                     "call *%%rax;"
                     RESTORE_GUEST_CONTEXT_SWITCH_REGS()
                     TRAP_TO_VMM(sret_instruction)
                     :
                     :
                     :"memory");
        BEGIN_PARAM_SCHEMA()
            PARAM32()
        END_PARAM_SCHEMA()
    END_TRANSLATION(sret_instruction);
        BEGIN_PARAM(sret_instruction)
            instruction_linear_address
        END_PARAM()
    COMMIT_TRANSLATION(sret_instruction, hartptr, instruction_linear_address);
    blob->is_to_stop = 1;
}

__attribute__((unused)) static void
sfence_vma_callback(struct hart * hartptr)
{
    // flush tlb cache
    invalidate_tlb(hartptr->itlb, hartptr->itlb_cap);
    invalidate_tlb(hartptr->dtlb, hartptr->dtlb_cap);
    // and finlally, flush translation cache
    flush_translation_cache(hartptr);
}

void
riscv_sfence_vma_translator(struct decoding * dec, struct prefetch_blob * blob,
                            uint32_t instruction)
{
    uint32_t instruction_linear_address = blob->next_instruction_to_fetch;
    struct hart * hartptr = (struct hart *)blob->opaque;
    PRECHECK_TRANSLATION_CACHE(sfence_vma_instruction, blob);
    BEGIN_TRANSLATION(sfence_vma_instruction);
    __asm__ volatile("movq %%r12, %%rdi;"
                     "movq $sfence_vma_callback, %%rax;"
                     SAVE_GUEST_CONTEXT_SWITCH_REGS()
                     "call *%%rax;"
                     RESTORE_GUEST_CONTEXT_SWITCH_REGS()
                     PROCEED_TO_NEXT_INSTRUCTION()
                     TRAP_TO_VMM(sfence_vma_instruction)
                     :
                     :
                     :"memory");
        BEGIN_PARAM_SCHEMA()
            PARAM32()
        END_PARAM_SCHEMA()
    END_TRANSLATION(sfence_vma_instruction);
        BEGIN_PARAM(sfence_vma_instruction)
            instruction_linear_address
        END_PARAM()
    COMMIT_TRANSLATION(sfence_vma_instruction, hartptr, instruction_linear_address);
    blob->is_to_stop = 1;
}

#include <hart_exception.h>
__attribute__((unused)) static void
ecall_callback(struct hart * hartptr)
{
    __not_reach();
    uint8_t exception = EXCEPTION_ECALL_FROM_MMODE;
    switch(hartptr->privilege_level)
    {
        case PRIVILEGE_LEVEL_MACHINE:
            exception = EXCEPTION_ECALL_FROM_MMODE;
            break;
        case PRIVILEGE_LEVEL_SUPERVISOR:
            exception = EXCEPTION_ECALL_FROM_SMODE;
            break;
        case PRIVILEGE_LEVEL_USER:
            exception = EXCEPTION_ECALL_FROM_UMODE;
            break;
        default:
            __not_reach();
            break;
    }
    raise_exception(hartptr, exception);
    
}

static void
riscv_ecall_translator(struct decoding * dec, struct prefetch_blob * blob,
                       uint32_t instruction)
{
    uint32_t instruction_linear_address = blob->next_instruction_to_fetch;
    struct hart * hartptr = (struct hart *)blob->opaque;
    PRECHECK_TRANSLATION_CACHE(ecall_instruction, blob);
    BEGIN_TRANSLATION(ecall_instruction);
    __asm__ volatile("movq %%r12, %%rdi;"
                     "movq $ecall_callback, %%rax;"
                     SAVE_GUEST_CONTEXT_SWITCH_REGS()
                     "call *%%rax;"
                     RESTORE_GUEST_CONTEXT_SWITCH_REGS()
                     PROCEED_TO_NEXT_INSTRUCTION()
                     TRAP_TO_VMM(ecall_instruction)
                     :
                     :
                     :"memory");
        BEGIN_PARAM_SCHEMA()
            PARAM32()
        END_PARAM_SCHEMA()
    END_TRANSLATION(ecall_instruction);
        BEGIN_PARAM(ecall_instruction)
            instruction_linear_address
        END_PARAM()
    COMMIT_TRANSLATION(ecall_instruction, hartptr, instruction_linear_address);
    blob->is_to_stop = 1;
}

#include <hart_interrupt.h>
__attribute__((unused)) static void
wfi_callback(struct hart * hartptr)
{
    //hartptr->pc += 4;
    // VMM YIELDS CPU until next interrupt comes
    //ASSERT(is_interrupt_deliverable(hartptr, INTERRUPT_SUPERVISOR_TIMER));
    //deliver_interrupt(hartptr, INTERRUPT_MACHINE_TIMER);
    //dump_hart(hartptr);
    __not_reach();
}
void
riscv_wfi_translator(struct decoding * dec, struct prefetch_blob * blob,
                     uint32_t instruction)
{
    ASSERT(0x8 == (dec->imm >> 5));
    uint32_t instruction_linear_address = blob->next_instruction_to_fetch;
    struct hart * hartptr = (struct hart *)blob->opaque;
    PRECHECK_TRANSLATION_CACHE(wfi_instruction, blob);
    BEGIN_TRANSLATION(wfi_instruction);
    __asm__ volatile("movq %%r12, %%rdi;"
                     "movq $wfi_callback, %%rax;"
                     SAVE_GUEST_CONTEXT_SWITCH_REGS()
                     "call *%%rax;"
                     RESTORE_GUEST_CONTEXT_SWITCH_REGS()
                     PROCEED_TO_NEXT_INSTRUCTION()
                     TRAP_TO_VMM(wfi_instruction)
                     :
                     :
                     :"memory");
        BEGIN_PARAM_SCHEMA()
            PARAM32()
        END_PARAM_SCHEMA()
    END_TRANSLATION(wfi_instruction);
        BEGIN_PARAM(wfi_instruction)
            instruction_linear_address
        END_PARAM()
    COMMIT_TRANSLATION(wfi_instruction, hartptr, instruction_linear_address);
    blob->is_to_stop = 1;
}
static void
riscv_funct3_000_translator(struct decoding * dec, struct prefetch_blob * blob,
                            uint32_t instruction)
{
#if 0
    if (((dec->imm >> 5) & 0x7f) == 0x9) {
        riscv_sfence_vma_translator(dec, blob, instruction);
    } else if (dec->rs2_index == 0x5) {
        riscv_wfi_translator(dec, blob, instruction);
    } else if (dec->rs2_index == 0x1) {
        riscv_ebreak_translator(dec, blob, instruction); 
    } else if (dec->rs2_index == 0x2) {
        if ((dec->imm >> 5) == 0x18) {
            riscv_mret_translator(dec, blob, instruction);
        } else if ((dec->imm >> 5) == 0x8) {
            riscv_sret_translator(dec, blob, instruction);
        } else {
            __not_reach();
        }
    } else 
    #endif
    if (dec->rs2_index == 0x0) {
        riscv_ecall_translator(dec, blob, instruction); 
    } else {
        log_fatal("can not translate:0x%x at 0x%x\n", instruction, blob->next_instruction_to_fetch);
        __not_reach();
    }
}


static instruction_sub_translator per_funct3_handlers[8];


void
riscv_supervisor_level_instructions_translation_entry(struct prefetch_blob * blob,
                                                      uint32_t instruction)
{
    struct decoding dec;
    // FIXED: only funct3:000 is encoded with type-S, others are not.
    if (!((instruction >> 12) & 0x7)) {
        instruction_decoding_per_type(&dec, instruction, ENCODING_TYPE_S);
    } else {
        instruction_decoding_per_type(&dec, instruction, ENCODING_TYPE_I);
    }
    ASSERT(per_funct3_handlers[dec.funct3]);
    per_funct3_handlers[dec.funct3](&dec, blob, instruction);
}


void
riscv_generic_csr_instructions_translator(struct decoding * dec,
                                          struct prefetch_blob * blob,
                                          uint32_t instruction);

__attribute__((constructor)) static void
supervisor_level_constructor(void)
{
    memset(per_funct3_handlers, 0x0, sizeof(per_funct3_handlers));
    per_funct3_handlers[0x0] = riscv_funct3_000_translator;
    //per_funct3_handlers[0x1] = riscv_generic_csr_instructions_translator;
    //per_funct3_handlers[0x2] = riscv_generic_csr_instructions_translator;
    //per_funct3_handlers[0x3] = riscv_generic_csr_instructions_translator;
    //per_funct3_handlers[0x5] = riscv_generic_csr_instructions_translator;
    //per_funct3_handlers[0x6] = riscv_generic_csr_instructions_translator;
    //per_funct3_handlers[0x7] = riscv_generic_csr_instructions_translator;
}


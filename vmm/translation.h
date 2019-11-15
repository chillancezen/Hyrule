/*
 * Copyright (c) 2019 Jie Zheng
 */
#ifndef _TRANSLATION_H
#define _TRANSLATION_H
#include <vm.h>
#include <stdlib.h>

enum RISCV_OPCODE {
    RISCV_OPCODE_LUI = 0x37,
    RISCV_OPCODE_AUIPC = 0x17,
    RISCV_OPCODE_JAL = 0x6f,
    RISCV_OPCODE_JARL = 0x67,
    RISCV_OPCODE_BRANCH = 0x63,
    RISCV_OPCODE_LOAD = 0x03,
    RISCV_OPCODE_STORE = 0x23,
    RISCV_OPCODE_OP_IMM = 0x13,
    RISCV_OPCODE_OP = 0x33,
    RISCV_OPCODE_FENCE = 0x0f
};

enum INSTRUCTION_ENCODING_TYPE {
    ENCODING_TYPE_UNKNOWN = 0,
    ENCODING_TYPE_U = 1,
    ENCODING_TYPE_I,
};

struct decoding {
    enum INSTRUCTION_ENCODING_TYPE instr_encoding_type;
    uint8_t opcode;
    uint8_t rd_index;
    uint32_t imm;
};

void
vmresume(struct hart * hartptr);

// before entering translation cache, the RBX is set to the address of the hart
// registers group
#define BEGIN_TRANSLATION(indicator)                                           \
__asm__ volatile ("movq $" #indicator "_translation_end, %%rdx;"               \
                  "jmpq *%%rdx;"                                               \
                  :                                                            \
                  :                                                            \
                  :"%rdx");                                                    \
__asm__ volatile (#indicator  "_translation_begin:\n")


#define DEBUG_TRANSLATION

#if defined(DEBUG_TRANSLATION)
#define TRANS_DEBUG(...) printf(__VA_ARGS__)
#else
#define TRANS_DEBUG(...) 
#endif

#define END_TRANSLATION(indicator)                                             \
__asm__ volatile (#indicator  "_translation_end:\n")

#define COMMIT_TRANSLATION(indicator, hart_instance, instruction_linear_addr)  \
{                                                                              \
    int __instruction_block_len = TRANSLATION_SIZE(indicator);                 \
    void * __instruction_block_begin = TRANSLATION_BEGIN_ADDR(indicator);      \
    assert(!add_translation_item(hart_instance, instruction_linear_addr,       \
                                 __instruction_block_begin,                    \
                                 __instruction_block_len));                    \
    struct program_counter_mapping_item * __item=                              \
        search_translation_item(hart_instance, instruction_linear_addr);       \
    assert(__item && __item->guest_pc == instruction_linear_addr);             \
    int __nr_param = (int)(sizeof(indicator##_params)/                         \
                           sizeof(indicator##_params[0]));                     \
    void * __instruction_block_end = hart_instance->translation_cache +        \
                                     __item->tc_offset +                       \
                                     __instruction_block_len;                  \
    void * __ptr = __instruction_block_end - (__nr_param * sizeof(uint32_t));  \
    int __index = 0;                                                           \
    for (; __index < __nr_param; __index++) {                                  \
        *(__index + ((uint32_t *)__ptr)) = indicator##_params[__index];        \
    }                                                                          \
    __ptr = hart_instance->translation_cache + __item->tc_offset;              \
    TRANS_DEBUG("%s at 0x%x {len:%d, tc:0x%x}: ", #indicator,                  \
                instruction_linear_addr, __instruction_block_len,              \
                (unsigned int)(uint64_t)__ptr);                                \
    for (__index = 0; __index < __instruction_block_len; __index++) {          \
        TRANS_DEBUG("%02x ", ((uint8_t *)__ptr)[__index]);                     \
    }                                                                          \
    TRANS_DEBUG("\n");                                                         \
}

#define PIC_PARAM(index) "(11f + 4 * "#index")(%%rip)"
#define END_INSTRUCTION(indicator)                                             \
                     "jmp "#indicator"_translation_end;"


#define BEGIN_PARAM_SCHEMA()                                                   \
__asm__ volatile(".align 1;"                                                   \
                 "11:"

#define PARAM32()                                                              \
                 ".int 0xcccccccc;"

#define END_PARAM_SCHEMA()                                                     \
                 );


#define BEGIN_PARAM(indicator)                                                 \
__attribute__((unused)) uint32_t indicator##_params[] = {                      \


#define END_PARAM()                                                            \
};

#define TRANSLATION_BEGIN_ADDR(indicator) ({                                   \
    uint64_t translation_begin_addr = 0;                                       \
    __asm__ volatile("movq $" #indicator "_translation_begin, %%rax;"          \
                     :"=a"(translation_begin_addr)                             \
                     :                                                         \
                     :"memory");                                               \
    (void *)translation_begin_addr;                                            \
})

#define TRANSLATION_END_ADDR(indicator) ({                                     \
    uint64_t translation_end_addr = 0;                                         \
    __asm__ volatile("movq $" #indicator "_translation_end, %%rax;"            \
                     :"=a"(translation_end_addr)                               \
                     :                                                         \
                     :"memory");                                               \
    (void *)translation_end_addr;                                              \
})

#define TRANSLATION_SIZE(indicator)                                            \
({                                                                             \
    uint64_t translation_begin_addr = 0;                                       \
    uint64_t translation_end_addr = 0;                                         \
    __asm__ volatile("movq $" #indicator "_translation_begin, %%rax;"          \
                     "movq $" #indicator "_translation_end, %%rdx;"            \
                     :"=a"(translation_begin_addr),                            \
                      "=d"(translation_end_addr)                               \
                     :                                                         \
                     :"memory");                                               \
    (int)(translation_end_addr - translation_begin_addr);                      \
})


#endif

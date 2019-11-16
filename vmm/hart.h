/*
 * Copyright (c) 2019 Jie Zheng
 */
#ifndef _HART_H
#define _HART_H

#include <stdint.h>
#include <assert.h>

#define XLEN 32

#if XLEN == 32
#define REGISTER_TYPE uint32_t
#elif XLEN == 64
#define REGISTER_TYPE uint64_t
#endif


struct integer_register_profile {
    REGISTER_TYPE zero;
    REGISTER_TYPE ra;
    REGISTER_TYPE sp;
    REGISTER_TYPE gp;
    REGISTER_TYPE tp;
    REGISTER_TYPE t0, t1, t2;
    REGISTER_TYPE s0, s1;
    REGISTER_TYPE a0, a1, a2, a3, a4, a5, a6, a7;
    REGISTER_TYPE s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
    REGISTER_TYPE t3, t4, t5, t6;
}__attribute__((packed));



struct virtual_machine;

struct program_counter_mapping_item {
    uint32_t guest_pc;
    uint32_t tc_offset;
}__attribute__((packed));

#define TRANSLATION_CACHE_SIZE (4096 * 1)
#define MAX_INSTRUCTIONS_TOTRANSLATE 256
// reserve a small trunk of space to transfer control to vmm
#define RESERVED_CACHE_LENGTH 32

struct hart {
    struct integer_register_profile registers __attribute__((aligned(64)));
    REGISTER_TYPE pc;
    int hart_id;
    struct virtual_machine * vmptr;

    int nr_translated_instructions;
    void * pc_mappings;

    void * translation_cache;
    int translation_cache_ptr;
}__attribute__((aligned(64)));

struct prefetch_blob {
    // The guest address of instruction to fetch and translate in the next round
    uint32_t next_instruction_to_fetch;
    // indicating whether to stop fetch, there are several reasons to stop:
    // 1. translation cache is full
    // 2. encounter a jump/branch instruction which is considered as a terminator
    //    of a translation unit.
    // 3. the target instruction is already in the translation cache
    uint8_t is_to_stop;

    // The pointer of current hart.
    void * opaque;
};

static inline int
unoccupied_cache_size(struct hart * hart_instance)
{
    int ret = 0;
    if (hart_instance->nr_translated_instructions <
        MAX_INSTRUCTIONS_TOTRANSLATE) {
        ret = TRANSLATION_CACHE_SIZE - hart_instance->translation_cache_ptr -
              RESERVED_CACHE_LENGTH;
    }
    assert(ret >= 0);
    return ret;
}

void
hart_init(struct hart * hart_instance, int hart_id);

void
flush_translation_cache(struct hart * hart_instance);


int
add_translation_item(struct hart * hart_instance,
                     uint32_t guest_instruction_address,
                     const void * translation_instruction_block,
                     int instruction_block_length);

struct program_counter_mapping_item *
search_translation_item(struct hart * hart_instance,
                        uint32_t guest_instruction_address);

#endif

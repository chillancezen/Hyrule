/*
 * Copyright (c) 2019-2020 Jie Zheng
 */

.extern offset_of_vmm_stack
.section .text
.global vmm_entry_point
vmm_entry_point:
    // FIXED: switch stack during context switching, or the stack of the vmm
    // may be potentially exhausted.
    movq $offset_of_vmm_stack, %rsi
    movq (%rsi), %rdx
    addq %r12, %rdx
    movq (%rdx), %rdx
    movq %rdx, %rsp

    .extern vmexit
    pushq %r12
    movq %r12, %rdi
    call vmexit

    .extern vmpanic
    popq %r12
    movq %r12, %rdi
    call vmpanic




.global vmm_jumper_begin
.global vmm_jumper_end
vmm_jumper_begin:
    movq $vmm_entry_point, %rax
    jmpq *%rax
vmm_jumper_end:




.global translation_slow_path
translation_slow_path:
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8


.global user_space_trap
user_space_trap:
    pushfq
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
    pushq %rsi
    pushq %rdi
    pushq %rbp
    // Now rsp holds the cpu_state. let's pass it to handler in C.
    .extern userspace_trap_handler
    movq %rsp, %rdi
    call userspace_trap_handler 
user_space_trap_resume:
    movq %rax, %rsp
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rax
    popq %rbx
    popq %rcx
    popq %rdx
    popq %r8
    popq %r9
    popq %r10
    popq %r11
    popq %r12
    popq %r13
    popq %r14
    popq %r15
    popfq
    retq

    

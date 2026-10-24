    .section .note.GNU-stack,"",@progbits
    .text
    .globl check_stack_align
check_stack_align:
    # At function entry right after call (return address on stack):
    # rsp + 8 should be aligned to 16 bytes.
    # Therefore, (rsp + 8) & 15 should be 0.
    leaq 8(%rsp), %rax
    andq $15, %rax
    ret

    .globl run_callee_saved_probe
run_callee_saved_probe:
    # Save real caller callee-saved registers on stack
    pushq %rbx
    pushq %rbp
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Align stack to 16 bytes for call:
    # 6 pushes (48 bytes) + return address (8 bytes) = 56 bytes.
    # subq $8, %rsp makes it 64 bytes (aligned to 16).
    subq $8, %rsp

    # Set distinct sentinel values in callee-saved registers
    movabsq $0x1111111111111111, %rbx
    movabsq $0x2222222222222222, %rbp
    movabsq $0x3333333333333333, %r12
    movabsq $0x4444444444444444, %r13
    movabsq $0x5555555555555555, %r14
    movabsq $0x6666666666666666, %r15

    # Set argument registers for zcc_workload(1, 2, 3, 4, 5, 6, 7, 8)
    movq $1, %rdi
    movq $2, %rsi
    movq $3, %rdx
    movq $4, %rcx
    movq $5, %r8
    movq $6, %r9
    # Stack arguments: 7 and 8
    pushq $8
    pushq $7
    # 2 pushes = 16 bytes, preserving 16-byte alignment

    call zcc_workload
    addq $16, %rsp

    # Check for corruption against sentinels and build bitmask in %rsi
    xorq %rsi, %rsi

    movabsq $0x1111111111111111, %rcx
    cmpq %rcx, %rbx
    je .Lrbx_ok
    orq $1, %rsi
.Lrbx_ok:
    movabsq $0x2222222222222222, %rcx
    cmpq %rcx, %rbp
    je .Lrbp_ok
    orq $2, %rsi
.Lrbp_ok:
    movabsq $0x3333333333333333, %rcx
    cmpq %rcx, %r12
    je .Lr12_ok
    orq $4, %rsi
.Lr12_ok:
    movabsq $0x4444444444444444, %rcx
    cmpq %rcx, %r13
    je .Lr13_ok
    orq $8, %rsi
.Lr13_ok:
    movabsq $0x5555555555555555, %rcx
    cmpq %rcx, %r14
    je .Lr14_ok
    orq $16, %rsi
.Lr14_ok:
    movabsq $0x6666666666666666, %rcx
    cmpq %rcx, %r15
    je .Lr15_ok
    orq $32, %rsi
.Lr15_ok:

    # Clean up alignment padding
    addq $8, %rsp

    # Restore real caller callee-saved registers
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbp
    popq %rbx

    # Return mask in %rax
    movq %rsi, %rax
    ret

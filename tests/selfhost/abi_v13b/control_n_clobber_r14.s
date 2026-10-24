    .section .note.GNU-stack,"",@progbits
    .text
    .globl zcc_workload
zcc_workload:
    # Deliberately clobber callee-saved %r14 without restoring
    movabsq $0xDEADBEEFCAFEBABE, %r14
    movq $42, %rax
    ret

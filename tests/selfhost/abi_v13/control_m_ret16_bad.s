    .section .note.GNU-stack,"",@progbits
    .text
    .globl abi_ret_u16
abi_ret_u16:
    # Surgical ABI mutation: correct first eightbyte placed in %rax (%rdi -> %rax),
    # but corrupted second eightbyte placed in %rdx (xorq %rdx, %rdx).
    movq %rdi, %rax
    xorq %rdx, %rdx
    ret

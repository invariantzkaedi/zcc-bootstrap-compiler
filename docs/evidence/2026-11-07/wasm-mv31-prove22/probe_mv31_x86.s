# ZCC asm begin
    .section .note.GNU-stack,"",@progbits
    .file 1 "probe_mv31_post.c"
    .text
    .globl test_savg_f
test_savg_f:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .file 1 "/tmp/probe_mv31_post.c"
    .loc 1 1
    movslq -16(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    xorl %eax, %ecx
    sarl $1, %ecx
    andl %esi, %eax
    addl %ecx, %eax
    movslq %eax, %rax
    cltq
.Lfunc_end_100:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_savg_c
test_savg_c:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 2
    movslq -16(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    xorl %eax, %ecx
    sarl $1, %ecx
    orl %esi, %eax
    subl %ecx, %eax
    movslq %eax, %rax
    cltq
.Lfunc_end_101:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_uavg_f
test_uavg_f:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3
    movl -16(%rbp), %eax
    pushq %rax
    movl -8(%rbp), %eax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    xorl %eax, %ecx
    shrl $1, %ecx
    andl %esi, %eax
    addl %ecx, %eax
    movl %eax, %eax
    movl %eax, %eax
.Lfunc_end_102:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_uavg_c
test_uavg_c:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 4
    movl -16(%rbp), %eax
    pushq %rax
    movl -8(%rbp), %eax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    xorl %eax, %ecx
    shrl $1, %ecx
    orl %esi, %eax
    subl %ecx, %eax
    movl %eax, %eax
    movl %eax, %eax
.Lfunc_end_103:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_usad32
test_usad32:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 5
    movl -16(%rbp), %eax
    pushq %rax
    movl -8(%rbp), %eax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    subq %rcx, %rax
    movq %rax, %rdx
    sarq $63, %rdx
    xorq %rdx, %rax
    subq %rdx, %rax
    movl %eax, %eax
    movl %eax, %eax
.Lfunc_end_104:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .section .rodata
    .align 4
.Lf32_one:
    .long 0x3F800000
    .align 8
.Lf64_one:
    .quad 0x3FF0000000000000
    .align 16
.Lf32_signmask:
    .long 0x80000000
    .long 0
    .long 0
    .long 0
    .align 16
.Lf64_signmask:
    .quad 0x8000000000000000
    .quad 0

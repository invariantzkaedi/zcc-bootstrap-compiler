# ZCC asm begin
    .section .note.GNU-stack,"",@progbits
    .file 1 "mv23_inspect.c"
    .text
    .globl test_mv23
test_mv23:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $384, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    movq %rdx, -24(%rbp)
    movq %rcx, -32(%rbp)
    .file 1 "/tmp/mv23_inspect.c"
    .loc 1 1
    .loc 1 2
    movq $-16, %rax
    movl %eax, %eax
    movq %rax, %rdi
    xorl %eax, %eax
    movl %edi, %eax
    sarl $31, %eax
    xorl %edi, %eax
    addl %eax, %eax
    orl $1, %eax
    bsrl %eax, %eax
    xorl $31, %eax
    cltq
    movq %rax, %r11
    leaq -40(%rbp), %rax
    movl %r11d, (%rax)
    .loc 1 3
    movslq -8(%rbp), %rax
    movl %eax, %eax
    movq %rax, %rdi
    xorl %eax, %eax
    movl %edi, %eax
    sarl $31, %eax
    xorl %edi, %eax
    addl %eax, %eax
    orl $1, %eax
    bsrl %eax, %eax
    xorl $31, %eax
    cltq
    movq %rax, %r11
    leaq -48(%rbp), %rax
    movl %r11d, (%rax)
    .loc 1 4
    movq -16(%rbp), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rdi, %rax
    sarq $63, %rax
    xorq %rdi, %rax
    addq %rax, %rax
    orq $1, %rax
    bsrq %rax, %rax
    xorq $63, %rax
    cltq
    movq %rax, %r11
    leaq -56(%rbp), %rax
    movl %r11d, (%rax)
    .loc 1 5
    movslq -8(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    setg %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L103
    movq -16(%rbp), %rax
    pushq %rax
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    setne %al
    movzbl %al, %eax
    jmp .L104
.L103:
    xorq %rax, %rax
.L104:
    testl %eax, %eax
    jne .L101
    movslq -8(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L101
    xorq %rax, %rax
    jmp .L102
.L101:
    movq $1, %rax
.L102:
    movq %rax, %r11
    leaq -64(%rbp), %rax
    movl %r11d, (%rax)
    .loc 1 6
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -72(%rbp)
    movq %r11, %rax
.L105:
    movslq -72(%rbp), %rax
    movq $8, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L106
    .loc 1 7
    movq -32(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $2, %r11
    addq %r11, %rax
    movslq (%rax), %rax
    movq $7, %r11
    addq %r11, %rax
    cltq
    pushq %rax
    movq -24(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $2, %r11
    addq %r11, %rax
    popq %r11
    movl %r11d, (%rax)
    movq %r11, %rax
.L107:
    .loc 1 6
    leaq -72(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L105
.L106:
    .loc 1 9
    movslq -40(%rbp), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    pushq %rax
    movq -24(%rbp), %rax
    xorq %r11, %r11
    shlq $2, %r11
    addq %r11, %rax
    movslq (%rax), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
.Lfunc_end_100:
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

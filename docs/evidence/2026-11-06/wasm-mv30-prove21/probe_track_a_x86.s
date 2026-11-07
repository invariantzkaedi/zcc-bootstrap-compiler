# ZCC asm begin
    .section .note.GNU-stack,"",@progbits
    .file 1 "probe_track_a.c"
    .text
    .globl test_smul_ovf
test_smul_ovf:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .file 1 "/tmp/probe_track_a.c"
    .loc 1 1
    movslq -16(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movslq %edi, %rax
    movslq %esi, %rcx
    imulq %rcx, %rax
    movslq %eax, %rdx
    cmpq %rdx, %rax
    setne %al
    movzbl %al, %eax
    cltq
.Lfunc_end_100:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_umul_ovf
test_umul_ovf:
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
    movl -16(%rbp), %eax
    pushq %rax
    movl -8(%rbp), %eax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    imulq %rcx, %rax
    shrq $32, %rax
    testq %rax, %rax
    setne %al
    movzbl %al, %eax
    cltq
.Lfunc_end_101:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_sadd_sat
test_sadd_sat:
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
    movslq -16(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movslq %edi, %rax
    movslq %esi, %rcx
    addq %rcx, %rax
    movq %rax, %rcx
    sarq $63, %rcx
    movq $2147483647, %rdx
    xorq %rdx, %rcx
    movslq %eax, %rdx
    cmpq %rdx, %rax
    setne %dl
    movzbl %dl, %edx
    negq %rdx
    xorq %rax, %rcx
    andq %rdx, %rcx
    xorq %rcx, %rax
    movslq %eax, %rax
    cltq
.Lfunc_end_102:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_ssub_sat
test_ssub_sat:
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
    movslq -16(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movslq %edi, %rax
    movslq %esi, %rcx
    subq %rcx, %rax
    movq %rax, %rcx
    sarq $63, %rcx
    movq $2147483647, %rdx
    xorq %rdx, %rcx
    movslq %eax, %rdx
    cmpq %rdx, %rax
    setne %dl
    movzbl %dl, %edx
    negq %rdx
    xorq %rax, %rcx
    andq %rdx, %rcx
    xorq %rcx, %rax
    movslq %eax, %rax
    cltq
.Lfunc_end_103:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_uadd_sat
test_uadd_sat:
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
    addq %rcx, %rax
    movq %rax, %rdx
    shrq $32, %rdx
    negq %rdx
    orq %rdx, %rax
    movl %eax, %eax
    movl %eax, %eax
.Lfunc_end_104:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_usub_sat
test_usub_sat:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 6
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
    shrq $63, %rdx
    subq $1, %rdx
    andq %rdx, %rax
    movl %eax, %eax
    movl %eax, %eax
.Lfunc_end_105:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl test_const_fold
test_const_fold:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $304, %rsp
    .loc 1 7
    .loc 1 8
    .loc 1 13
    .loc 1 12
    .loc 1 11
    .loc 1 10
    .loc 1 9
    .loc 1 8
    movq $2, %rax
    pushq %rax
    movq $1073741824, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movslq %edi, %rax
    movslq %esi, %rcx
    imulq %rcx, %rax
    movslq %eax, %rdx
    cmpq %rdx, %rax
    setne %al
    movzbl %al, %eax
    cltq
    pushq %rax
    .loc 1 9
    subq $8, %rsp
    movq $2, %rax
    pushq %rax
    movabsq $2147483648, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    imulq %rcx, %rax
    shrq $32, %rax
    testq %rax, %rax
    setne %al
    movzbl %al, %eax
    addq $8, %rsp
    cltq
    pushq %rax
    movq $1, %rax
    movq %rax, %rcx
    popq %rax
    shll %cl, %eax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    pushq %rax
    .loc 1 10
    subq $8, %rsp
    movq $10, %rax
    pushq %rax
    movq $2147483647, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movslq %edi, %rax
    movslq %esi, %rcx
    addq %rcx, %rax
    movq %rax, %rcx
    sarq $63, %rcx
    movq $2147483647, %rdx
    xorq %rdx, %rcx
    movslq %eax, %rdx
    cmpq %rdx, %rax
    setne %dl
    movzbl %dl, %edx
    negq %rdx
    xorq %rax, %rcx
    andq %rdx, %rcx
    xorq %rcx, %rax
    movslq %eax, %rax
    addq $8, %rsp
    cltq
    movq $2147483647, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L107
    movq $4, %rax
    jmp .L108
.L107:
    xorl %eax, %eax
.L108:
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    pushq %rax
    .loc 1 11
    subq $8, %rsp
    movq $10, %rax
    pushq %rax
    movq $-2147483648, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movslq %edi, %rax
    movslq %esi, %rcx
    subq %rcx, %rax
    movq %rax, %rcx
    sarq $63, %rcx
    movq $2147483647, %rdx
    xorq %rdx, %rcx
    movslq %eax, %rdx
    cmpq %rdx, %rax
    setne %dl
    movzbl %dl, %edx
    negq %rdx
    xorq %rax, %rcx
    andq %rdx, %rcx
    xorq %rcx, %rax
    movslq %eax, %rax
    addq $8, %rsp
    cltq
    movq $-2147483648, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L109
    movq $8, %rax
    jmp .L110
.L109:
    xorl %eax, %eax
.L110:
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    pushq %rax
    .loc 1 12
    subq $8, %rsp
    movq $10, %rax
    pushq %rax
    movabsq $4294967295, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    addq %rcx, %rax
    movq %rax, %rdx
    shrq $32, %rdx
    negq %rdx
    orq %rdx, %rax
    movl %eax, %eax
    addq $8, %rsp
    movl %eax, %eax
    pushq %rax
    movabsq $4294967295, %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L111
    movq $16, %rax
    jmp .L112
.L111:
    xorl %eax, %eax
.L112:
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    pushq %rax
    .loc 1 13
    subq $8, %rsp
    movq $10, %rax
    pushq %rax
    movq $5, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movl %edi, %eax
    movl %esi, %ecx
    subq %rcx, %rax
    movq %rax, %rdx
    shrq $63, %rdx
    subq $1, %rdx
    andq %rdx, %rax
    movl %eax, %eax
    addq $8, %rsp
    movl %eax, %eax
    xorq %r11, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L113
    movq $32, %rax
    jmp .L114
.L113:
    xorl %eax, %eax
.L114:
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
.Lfunc_end_106:
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

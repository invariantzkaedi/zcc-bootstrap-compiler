# ZCC asm begin
    .section .note.GNU-stack,"",@progbits
    .file 1 "probe_mv32.c"
    .text
    .globl f_ssad
f_ssad:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .file 1 "/tmp/probe_mv32.c"
    .loc 1 1
    movslq -16(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movslq %edi, %rax
    movslq %esi, %rcx
    subq %rcx, %rax
    movq %rax, %rdx
    sarq $63, %rdx
    xorq %rdx, %rax
    subq %rdx, %rax
    movl %eax, %eax
    movl %eax, %eax
.Lfunc_end_100:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl f_smin
f_smin:
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
    movslq %edi, %rax
    movslq %esi, %rcx
    subq %rcx, %rax
    movq %rax, %rdx
    sarq $63, %rdx
    andq %rdx, %rax
    addq %rcx, %rax
    cltq
.Lfunc_end_101:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl f_smax
f_smax:
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
    subq %rcx, %rax
    movq %rax, %rdx
    sarq $63, %rdx
    notq %rdx
    andq %rdx, %rax
    addq %rcx, %rax
    cltq
.Lfunc_end_102:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl f_umin
f_umin:
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
    subq %rcx, %rax
    movq %rax, %rdx
    sarq $63, %rdx
    andq %rdx, %rax
    addq %rcx, %rax
    movl %eax, %eax
.Lfunc_end_103:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl f_umax
f_umax:
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
    notq %rdx
    andq %rdx, %rax
    addq %rcx, %rax
    movl %eax, %eax
.Lfunc_end_104:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl main
main:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $304, %rsp
    .loc 1 6
    .loc 1 7
    movq $2147483647, %rax
    pushq %rax
    movq $-2147483648, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_107
    subq $8, %rsp
    call f_ssad
    addq $8, %rsp
    jmp .Lcall_end_107
.Lcall_aligned_107:
    call f_ssad
.Lcall_end_107:
    movl %eax, %eax
    pushq %rax
    movabsq $4294967295, %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L106
    movq $1, %rax
    jmp .Lfunc_end_105
.L106:
    .loc 1 8
    movq $20, %rax
    pushq %rax
    movq $-10, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_111
    subq $8, %rsp
    call f_smin
    addq $8, %rsp
    jmp .Lcall_end_111
.Lcall_aligned_111:
    call f_smin
.Lcall_end_111:
    cltq
    movq $-10, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L109
    movq $20, %rax
    pushq %rax
    movq $-10, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_112
    subq $8, %rsp
    call f_smax
    addq $8, %rsp
    jmp .Lcall_end_112
.Lcall_aligned_112:
    call f_smax
.Lcall_end_112:
    cltq
    movq $20, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L109
    xorq %rax, %rax
    jmp .L110
.L109:
    movq $1, %rax
.L110:
    testl %eax, %eax
    je .L108
    movq $2, %rax
    jmp .Lfunc_end_105
.L108:
    .loc 1 9
    movabsq $4294967295, %rax
    pushq %rax
    movq $10, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_116
    subq $8, %rsp
    call f_umin
    addq $8, %rsp
    jmp .Lcall_end_116
.Lcall_aligned_116:
    call f_umin
.Lcall_end_116:
    movl %eax, %eax
    movq $10, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L114
    movabsq $4294967295, %rax
    pushq %rax
    movq $10, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_117
    subq $8, %rsp
    call f_umax
    addq $8, %rsp
    jmp .Lcall_end_117
.Lcall_aligned_117:
    call f_umax
.Lcall_end_117:
    movl %eax, %eax
    pushq %rax
    movabsq $4294967295, %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L114
    xorq %rax, %rax
    jmp .L115
.L114:
    movq $1, %rax
.L115:
    testl %eax, %eax
    je .L113
    movq $3, %rax
    jmp .Lfunc_end_105
.L113:
    .loc 1 10
    xorl %eax, %eax
    jmp .Lfunc_end_105
    xorl %eax, %eax
.Lfunc_end_105:
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

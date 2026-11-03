# ZCC asm begin
    .section .note.GNU-stack,"",@progbits
    .file 1 "x86_mv02_imm64.c"
    .text
    .globl classify
classify:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    .file 1 "tests/x86_mv02_imm64.c"
    .loc 1 2
    .loc 1 3
    movq -8(%rbp), %rax
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, %rax
    movq %xmm15, %r11
    je .L102
    movq %r11, %xmm14
    movabsq $-4294967296, %r11
    cmpq %r11, %rax
    movq %xmm14, %r11
    je .L103
    jmp .L104
.L102:
    .loc 1 4
    jmp .Luser_classify___zc_100000000_4
.L103:
    .loc 1 5
    jmp .Luser_classify___zc_ffffffff00000000_5
.L104:
    .loc 1 6
    jmp .Luser_classify___zc_def_6
    .loc 1 3
    .loc 1 4
.Luser_classify___zc_100000000_4:
    movq $7, %rax
    jmp .Lfunc_end_100
    .loc 1 5
.Luser_classify___zc_ffffffff00000000_5:
    movq $9, %rax
    jmp .Lfunc_end_100
    .loc 1 6
.Luser_classify___zc_def_6:
    movq $3, %rax
    jmp .Lfunc_end_100
.L101:
.Lfunc_end_100:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_live_r11
classify_live_r11:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    .loc 1 12
    .loc 1 13
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -16(%rbp)
    movq %r11, %rax
    .loc 1 14
    movq $13374242, %r11
    .loc 1 15
    movq -8(%rbp), %rax
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, %rax
    movq %xmm15, %r11
    je .L107
    movq %r11, %xmm14
    movabsq $-4294967296, %r11
    cmpq %r11, %rax
    movq %xmm14, %r11
    je .L108
    jmp .L109
.L107:
    .loc 1 16
    jmp .Luser_classify_live_r11___zc_100000000_16
.L108:
    .loc 1 19
    jmp .Luser_classify_live_r11___zc_ffffffff00000000_19
.L109:
    .loc 1 22
    jmp .Luser_classify_live_r11___zc_def_22
    .loc 1 15
    .loc 1 16
.Luser_classify_live_r11___zc_100000000_16:
    .loc 1 17
    movq %r11, %rax
    movq %rax, -16(%rbp)
    .loc 1 18
    movq -16(%rbp), %rax
    movq $13374242, %r11
    cmpq %r11, %rax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L110
    movq $7, %rax
    jmp .L111
.L110:
    movq $-1, %rax
.L111:
    jmp .Lfunc_end_105
    .loc 1 19
.Luser_classify_live_r11___zc_ffffffff00000000_19:
    .loc 1 20
    movq %r11, %rax
    movq %rax, -16(%rbp)
    .loc 1 21
    movq -16(%rbp), %rax
    movq $13374242, %r11
    cmpq %r11, %rax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L112
    movq $9, %rax
    jmp .L113
.L112:
    movq $-1, %rax
.L113:
    jmp .Lfunc_end_105
    .loc 1 22
.Luser_classify_live_r11___zc_def_22:
    .loc 1 23
    movq %r11, %rax
    movq %rax, -16(%rbp)
    .loc 1 24
    movq -16(%rbp), %rax
    movq $13374242, %r11
    cmpq %r11, %rax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L114
    movq $3, %rax
    jmp .L115
.L114:
    movq $-1, %rax
.L115:
    jmp .Lfunc_end_105
.L106:
.Lfunc_end_105:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_alias_r11
classify_alias_r11:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 29
    .loc 1 30
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 32
    movq $999888777, %r10
    .loc 1 33
    movabsq $4294967296, %r11
    .loc 1 34
    movq %r10, %xmm15
    movabsq $4294967296, %r10
    cmpq %r10, %r11
    movq %xmm15, %r10
    .loc 1 35
    sete %al
    movzbq %al, %rax
    movq %rax, -8(%rbp)
    .loc 1 36
    movq %r10, %rax
    movq %rax, -16(%rbp)
    .loc 1 37
    movq %r11, %rax
    movq %rax, -24(%rbp)
    .loc 1 43
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L117
    movq $-10, %rax
    jmp .Lfunc_end_116
.L117:
    .loc 1 44
    movq -16(%rbp), %rax
    movq $999888777, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L118
    movq $-11, %rax
    jmp .Lfunc_end_116
.L118:
    .loc 1 45
    movq -24(%rbp), %rax
    pushq %rax
    movabsq $4294967296, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L119
    movq $-12, %rax
    jmp .Lfunc_end_116
.L119:
    .loc 1 46
    movq $11, %rax
.Lfunc_end_116:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_rsp_relative
classify_rsp_relative:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    .loc 1 50
    .loc 1 51
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -16(%rbp)
    movq %r11, %rax
    .loc 1 53
    subq $16, %rsp
    .loc 1 54
    movq $111222333, (%rsp)
    .loc 1 55
    movabsq $4294967296, %rax
    .loc 1 56
    movq %rax, 8(%rsp)
    .loc 1 57
    movq $555666777, %r11
    .loc 1 58
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, 8(%rsp)
    movq %xmm15, %r11
    .loc 1 59
    sete %al
    .loc 1 60
    addq $16, %rsp
    .loc 1 61
    movzbq %al, %rax
    movq %rax, -8(%rbp)
    .loc 1 62
    movq %r11, %rax
    movq %rax, -16(%rbp)
    .loc 1 67
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L121
    movq $-20, %rax
    jmp .Lfunc_end_120
.L121:
    .loc 1 68
    movq -16(%rbp), %rax
    movq $555666777, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L122
    movq $-21, %rax
    jmp .Lfunc_end_120
.L122:
    .loc 1 69
    movq $22, %rax
.Lfunc_end_120:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mem_r11_base
classify_mem_r11_base:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    .loc 1 73
    .loc 1 74
    .loc 1 75
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -32(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -40(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 76
    movq $111, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 77
    movabsq $4294967296, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movq $1, %r11
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    .loc 1 79
    leaq -16(%rbp), %rax
    movq %rax, %r11
    leaq -48(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 80
    movq -48(%rbp), %rax
    movq %rax, %r11
    leaq -56(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 81
    movq -48(%rbp), %rax
    movq %rax, %r11
    .loc 1 82
    movq $777888999, %r10
    .loc 1 83
    movq %r10, %xmm15
    movabsq $4294967296, %r10
    cmpq %r10, 8(%r11)
    movq %xmm15, %r10
    .loc 1 84
    sete %al
    movzbq %al, %rax
    movq %rax, -24(%rbp)
    .loc 1 85
    movq %r10, %rax
    movq %rax, -32(%rbp)
    .loc 1 86
    movq %r11, %rax
    movq %rax, -40(%rbp)
    .loc 1 87
    movq -40(%rbp), %rax
    pushq %rax
    movq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L124
    movq $-32, %rax
    jmp .Lfunc_end_123
.L124:
    .loc 1 92
    movq -24(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L125
    movq $-30, %rax
    jmp .Lfunc_end_123
.L125:
    .loc 1 93
    movq -32(%rbp), %rax
    movq $777888999, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L126
    movq $-31, %rax
    jmp .Lfunc_end_123
.L126:
    .loc 1 94
    movq $33, %rax
.Lfunc_end_123:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mem_r11_r10_sib
classify_mem_r11_r10_sib:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $384, %rsp
    .loc 1 98
    .loc 1 99
    .loc 1 100
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -32(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -40(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -48(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -56(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 101
    movq $10, %rax
    pushq %rax
    leaq -24(%rbp), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    .loc 1 102
    movq $20, %rax
    pushq %rax
    leaq -24(%rbp), %rax
    movq $1, %r11
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 103
    movabsq $4294967296, %rax
    pushq %rax
    leaq -24(%rbp), %rax
    movq $2, %r11
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    .loc 1 105
    leaq -24(%rbp), %rax
    movq %rax, %r11
    leaq -64(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 106
    movq -64(%rbp), %rax
    movq %rax, %r11
    leaq -72(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 107
    movq -64(%rbp), %rax
    movq %rax, %r11
    .loc 1 108
    movq $1, %r10
    .loc 1 109
    movq $888999111, %r9
    .loc 1 110
    movq %r9, %xmm15
    movabsq $4294967296, %r9
    cmpq %r9, 8(%r11, %r10, 8)
    movq %xmm15, %r9
    .loc 1 111
    sete %al
    movzbq %al, %rax
    movq %rax, -32(%rbp)
    .loc 1 112
    movq %r9, %rax
    movq %rax, -40(%rbp)
    .loc 1 113
    movq %r10, %rax
    movq %rax, -48(%rbp)
    .loc 1 114
    movq %r11, %rax
    movq %rax, -56(%rbp)
    .loc 1 115
    movq -56(%rbp), %rax
    pushq %rax
    movq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L128
    movq $-43, %rax
    jmp .Lfunc_end_127
.L128:
    .loc 1 121
    movq -32(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L129
    movq $-40, %rax
    jmp .Lfunc_end_127
.L129:
    .loc 1 122
    movq -40(%rbp), %rax
    movq $888999111, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L130
    movq $-41, %rax
    jmp .Lfunc_end_127
.L130:
    .loc 1 123
    movq -48(%rbp), %rax
    movq $1, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L131
    movq $-42, %rax
    jmp .Lfunc_end_127
.L131:
    .loc 1 124
    movq $44, %rax
.Lfunc_end_127:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_rsp_indexed
classify_rsp_indexed:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    .loc 1 128
    .loc 1 129
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -16(%rbp)
    movq %r11, %rax
    .loc 1 131
    subq $24, %rsp
    .loc 1 132
    movq $0, (%rsp)
    .loc 1 133
    movq $0, 8(%rsp)
    .loc 1 134
    movabsq $4294967296, %rax
    .loc 1 135
    movq %rax, 16(%rsp)
    .loc 1 136
    movq $1, %rcx
    .loc 1 137
    movq $333444555, %r11
    .loc 1 138
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, 8(%rsp, %rcx, 8)
    movq %xmm15, %r11
    .loc 1 139
    sete %al
    .loc 1 140
    addq $24, %rsp
    .loc 1 141
    movzbq %al, %rax
    movq %rax, -8(%rbp)
    .loc 1 142
    movq %r11, %rax
    movq %rax, -16(%rbp)
    .loc 1 147
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L133
    movq $-50, %rax
    jmp .Lfunc_end_132
.L133:
    .loc 1 148
    movq -16(%rbp), %rax
    movq $333444555, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L134
    movq $-51, %rax
    jmp .Lfunc_end_132
.L134:
    .loc 1 149
    movq $55, %rax
.Lfunc_end_132:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_redzone_rsp
classify_redzone_rsp:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $352, %rsp
    .loc 1 153
    .loc 1 154
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -32(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -40(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .loc 1 156
    movabsq $4294967296, %rax
    .loc 1 157
    movq %rax, -8(%rsp)
    .loc 1 158
    movq %rax, -128(%rsp)
    .loc 1 159
    movabsq $123456789012345, %rax
    .loc 1 160
    movq %rax, -120(%rsp)
    .loc 1 161
    movq $777888999, %r11
    .loc 1 162
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, -8(%rsp)
    movq %xmm15, %r11
    .loc 1 163
    sete %al
    .loc 1 164
    movzbq %al, %rax
    movq %rax, -8(%rbp)
    .loc 1 165
    movq %r11, %xmm14
    movabsq $4294967296, %r11
    cmpq %r11, -128(%rsp)
    movq %xmm14, %r11
    .loc 1 166
    sete %al
    .loc 1 167
    movzbq %al, %rax
    movq %rax, -16(%rbp)
    .loc 1 168
    movq -8(%rsp), %rax
    movq %rax, -24(%rbp)
    .loc 1 169
    movq -120(%rsp), %rax
    movq %rax, -32(%rbp)
    .loc 1 170
    movq -128(%rsp), %rax
    movq %rax, -40(%rbp)
    .loc 1 171
    movq %r11, %rax
    movq %rax, -48(%rbp)
    .loc 1 180
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L136
    movq $-60, %rax
    jmp .Lfunc_end_135
.L136:
    .loc 1 181
    movq -16(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L137
    movq $-61, %rax
    jmp .Lfunc_end_135
.L137:
    .loc 1 182
    movq -24(%rbp), %rax
    pushq %rax
    movabsq $4294967296, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L138
    movq $-62, %rax
    jmp .Lfunc_end_135
.L138:
    .loc 1 183
    movq -32(%rbp), %rax
    pushq %rax
    movabsq $123456789012345, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L139
    movq $-65, %rax
    jmp .Lfunc_end_135
.L139:
    .loc 1 184
    movq -40(%rbp), %rax
    pushq %rax
    movabsq $4294967296, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L140
    movq $-63, %rax
    jmp .Lfunc_end_135
.L140:
    .loc 1 185
    movq -48(%rbp), %rax
    movq $777888999, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L141
    movq $-64, %rax
    jmp .Lfunc_end_135
.L141:
    .loc 1 186
    movq $66, %rax
.Lfunc_end_135:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_direct_rsp
classify_direct_rsp:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 190
    .loc 1 191
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 194
    pushq %rbx
    movq $9, %rax
    movabsq $4294963200, %rdi
    movq $8192, %rsi
    movq $3, %rdx
    movq $0x32, %r10
    movq $-1, %r8
    xorq %r9, %r9
    syscall
    movq %rax, %rbx
    movq %rbx, -8(%rbp)
    popq %rbx
    .loc 1 203
    movq -8(%rbp), %rax
    pushq %rax
    movabsq $4294963200, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L143
    movq $-70, %rax
    jmp .Lfunc_end_142
.L143:
    .loc 1 205
    movq %rsp, %r10
    .loc 1 206
    movq $555666777, %r11
    .loc 1 207
    movabsq $4294967296, %rsp
    .loc 1 208
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, %rsp
    movq %xmm15, %r11
    .loc 1 209
    sete %al
    .loc 1 210
    movq %r10, %rsp
    .loc 1 211
    movzbq %al, %rax
    movq %rax, -16(%rbp)
    .loc 1 212
    movq %r11, %rax
    movq %rax, -24(%rbp)
    .loc 1 215
    movq $11, %rax
    movabsq $4294963200, %rdi
    movq $8192, %rsi
    syscall
    .loc 1 223
    movq -16(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L144
    movq $-71, %rax
    jmp .Lfunc_end_142
.L144:
    .loc 1 224
    movq -24(%rbp), %rax
    movq $555666777, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L145
    movq $-72, %rax
    jmp .Lfunc_end_142
.L145:
    .loc 1 225
    movq $77, %rax
.Lfunc_end_142:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_frame_scratch_alias
classify_frame_scratch_alias:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $384, %rsp
    .loc 1 238
    .loc 1 239
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -32(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -40(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 240
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -48(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -56(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -64(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -72(%rbp)
    movq %r11, %rax
    .loc 1 242
    movq $777888999, %r11
    .loc 1 243
    movabsq $4294967296, %rax
    .loc 1 244
    movq %rax, 32(%rsp)
    .loc 1 245
    movq %rax, 24(%rsp)
    .loc 1 247
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, 32(%rsp)
    movq %xmm15, %r11
    .loc 1 248
    sete %al
    .loc 1 249
    movzbq %al, %rax
    movq %rax, -8(%rbp)
    .loc 1 251
    movq %r11, %xmm14
    movabsq $4294967296, %r11
    cmpq %r11, -352(%rbp)
    movq %xmm14, %r11
    .loc 1 252
    sete %al
    .loc 1 253
    movzbq %al, %rax
    movq %rax, -16(%rbp)
    .loc 1 255
    movq %r11, %xmm13
    movabsq $4294967296, %r11
    cmpq %r11, 24(%rsp)
    movq %xmm13, %r11
    .loc 1 256
    sete %al
    .loc 1 257
    movzbq %al, %rax
    movq %rax, -24(%rbp)
    .loc 1 262
    movq $1, %rcx
    .loc 1 263
    movq %r11, %xmm12
    movabsq $4294967296, %r11
    cmpq %r11, 24(%rsp, %rcx, 8)
    movq %xmm12, %r11
    .loc 1 264
    sete %dl
    .loc 1 265
    movq $3, %rcx
    .loc 1 266
    movq %r11, %xmm12
    movabsq $4294967296, %r11
    cmpq %r11, 8(%rsp, %rcx, 8)
    movq %xmm12, %r11
    .loc 1 267
    sete %al
    .loc 1 268
    andb %dl, %al
    .loc 1 269
    movzbq %al, %rax
    movq %rax, -32(%rbp)
    .loc 1 274
    movq $1, %rcx
    .loc 1 275
    movq %r11, %xmm12
    movabsq $4294967296, %r11
    cmpq %r11, 16(%rsp, %rcx, 8)
    movq %xmm12, %r11
    .loc 1 276
    sete %dl
    .loc 1 277
    movq $-1, %rcx
    .loc 1 278
    movq %r11, %xmm12
    movabsq $4294967296, %r11
    cmpq %r11, 32(%rsp, %rcx, 8)
    movq %xmm12, %r11
    .loc 1 279
    sete %al
    .loc 1 280
    andb %dl, %al
    .loc 1 281
    movzbq %al, %rax
    movq %rax, -40(%rbp)
    .loc 1 283
    movq 32(%rsp), %rax
    movq %rax, -48(%rbp)
    .loc 1 284
    movq -352(%rbp), %rax
    movq %rax, -56(%rbp)
    .loc 1 285
    movq 24(%rsp), %rax
    movq %rax, -64(%rbp)
    .loc 1 286
    movq %r11, %rax
    movq %rax, -72(%rbp)
    .loc 1 298
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L147
    movq $-80, %rax
    jmp .Lfunc_end_146
.L147:
    .loc 1 299
    movq -16(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L148
    movq $-81, %rax
    jmp .Lfunc_end_146
.L148:
    .loc 1 300
    movq -24(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L149
    movq $-82, %rax
    jmp .Lfunc_end_146
.L149:
    .loc 1 301
    movq -32(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L150
    movq $-87, %rax
    jmp .Lfunc_end_146
.L150:
    .loc 1 302
    movq -40(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L151
    movq $-88, %rax
    jmp .Lfunc_end_146
.L151:
    .loc 1 303
    movq -48(%rbp), %rax
    pushq %rax
    movabsq $4294967296, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L152
    movq $-83, %rax
    jmp .Lfunc_end_146
.L152:
    .loc 1 304
    movq -56(%rbp), %rax
    pushq %rax
    movabsq $4294967296, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L153
    movq $-84, %rax
    jmp .Lfunc_end_146
.L153:
    .loc 1 305
    movq -64(%rbp), %rax
    pushq %rax
    movabsq $4294967296, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L154
    movq $-85, %rax
    jmp .Lfunc_end_146
.L154:
    .loc 1 306
    movq -72(%rbp), %rax
    movq $777888999, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L155
    movq $-86, %rax
    jmp .Lfunc_end_146
.L155:
    .loc 1 307
    movq $88, %rax
.Lfunc_end_146:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_variadic_scratch
classify_variadic_scratch:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $560, %rsp
    movq %rdi, -8(%rbp)
    movq %rdi, -560(%rbp)
    movq %rsi, -552(%rbp)
    movq %rdx, -544(%rbp)
    movq %rcx, -536(%rbp)
    movq %r8, -528(%rbp)
    movq %r9, -520(%rbp)
    movsd %xmm0, -512(%rbp)
    movsd %xmm1, -496(%rbp)
    movsd %xmm2, -480(%rbp)
    movsd %xmm3, -464(%rbp)
    movsd %xmm4, -448(%rbp)
    movsd %xmm5, -432(%rbp)
    movsd %xmm6, -416(%rbp)
    movsd %xmm7, -400(%rbp)
    .loc 1 311
    .loc 1 312
    movabsq $4294967296, %rax
    movq %rax, %r11
    leaq -56(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 313
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -64(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -72(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 315
    movq -56(%rbp), %rax
    .loc 1 316
    movq $333444555, %r11
    .loc 1 317
    movq %r11, %xmm15
    movabsq $4294967296, %r11
    cmpq %r11, %rax
    movq %xmm15, %r11
    .loc 1 318
    sete %al
    .loc 1 319
    movzbq %al, %rax
    movq %rax, -64(%rbp)
    .loc 1 320
    movq %r11, %rax
    movq %rax, -72(%rbp)
    .loc 1 325
    movq -64(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L157
    movq $-90, %rax
    jmp .Lfunc_end_156
.L157:
    .loc 1 326
    movq -72(%rbp), %rax
    movq $333444555, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L158
    movq $-91, %rax
    jmp .Lfunc_end_156
.L158:
    .loc 1 327
    movq $90, %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
.Lfunc_end_156:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_xmm15_128bit_liveness
classify_xmm15_128bit_liveness:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    .loc 1 339
    .loc 1 340
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 341
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -32(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -40(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -48(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 343
    movabsq $0x1122334455667788, %rax
    .loc 1 344
    movq %rax, -96(%rbp)
    .loc 1 345
    movabsq $0x66554433221100ff, %rax
    .loc 1 346
    movq %rax, -88(%rbp)
    .loc 1 347
    movaps -96(%rbp), %xmm15
    .loc 1 349
    movabsq $0x2233445566778899, %rax
    .loc 1 350
    movq %rax, -112(%rbp)
    .loc 1 351
    movabsq $0x7766554433221100, %rax
    .loc 1 352
    movq %rax, -104(%rbp)
    .loc 1 353
    movaps -112(%rbp), %xmm14
    .loc 1 355
    movq $777888999, %r11
    .loc 1 356
    movabsq $4294967296, %rax
    .loc 1 357
    movq %rax, 32(%rsp)
    .loc 1 358
    movq %rax, 24(%rsp)
    .loc 1 361
    movq $-1, %rcx
    .loc 1 362
    movq %r11, %xmm13
    movabsq $4294967296, %r11
    cmpq %r11, 32(%rsp, %rcx, 8)
    movq %xmm13, %r11
    .loc 1 363
    sete %al
    .loc 1 364
    movzbq %al, %rax
    movq %rax, -8(%rbp)
    .loc 1 367
    movaps %xmm14, -128(%rbp)
    .loc 1 368
    movq -128(%rbp), %rax
    .loc 1 369
    movq %rax, -48(%rbp)
    .loc 1 370
    movq -120(%rbp), %rax
    .loc 1 371
    movq %rax, -56(%rbp)
    .loc 1 374
    movq $3, %rcx
    .loc 1 375
    movq %r11, %xmm12
    movabsq $4294967296, %r11
    cmpq %r11, 8(%rsp, %rcx, 8)
    movq %xmm12, %r11
    .loc 1 376
    sete %al
    .loc 1 377
    movzbq %al, %rax
    movq %rax, -16(%rbp)
    .loc 1 380
    movaps %xmm15, -144(%rbp)
    .loc 1 381
    movq -144(%rbp), %rax
    .loc 1 382
    movq %rax, -32(%rbp)
    .loc 1 383
    movq -136(%rbp), %rax
    .loc 1 384
    movq %rax, -40(%rbp)
    .loc 1 385
    movq %r11, %rax
    movq %rax, -24(%rbp)
    .loc 1 395
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L160
    movq $-100, %rax
    jmp .Lfunc_end_159
.L160:
    .loc 1 396
    movq -16(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L161
    movq $-101, %rax
    jmp .Lfunc_end_159
.L161:
    .loc 1 397
    movq -24(%rbp), %rax
    movq $777888999, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L162
    movq $-102, %rax
    jmp .Lfunc_end_159
.L162:
    .loc 1 398
    movq -32(%rbp), %rax
    pushq %rax
    movabsq $1234605616436508552, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L163
    movq $-103, %rax
    jmp .Lfunc_end_159
.L163:
    .loc 1 399
    movq -40(%rbp), %rax
    pushq %rax
    movabsq $7373874951294615807, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L164
    movq $-104, %rax
    jmp .Lfunc_end_159
.L164:
    .loc 1 400
    movq -48(%rbp), %rax
    pushq %rax
    movabsq $2464388554683811993, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L165
    movq $-105, %rax
    jmp .Lfunc_end_159
.L165:
    .loc 1 401
    movq -56(%rbp), %rax
    pushq %rax
    movabsq $8603657889541918976, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L166
    movq $-106, %rax
    jmp .Lfunc_end_159
.L166:
    .loc 1 402
    movq $108, %rax
.Lfunc_end_159:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv10_backward_loop_xmm15
classify_mv10_backward_loop_xmm15:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 411
    .loc 1 412
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 414
    movabsq $0x1122334455667788, %rax
    .loc 1 415
    movq %rax, -64(%rbp)
    .loc 1 416
    movabsq $0x66554433221100ff, %rax
    .loc 1 417
    movq %rax, -56(%rbp)
    .loc 1 418
    movaps -64(%rbp), %xmm15
    .loc 1 419
    xorq %rcx, %rcx
    .loc 1 420
    movq $777888999, %r11
    .loc 1 421
    .Lmv10_loop:
    .loc 1 422
    movaps %xmm15, -80(%rbp)
    .loc 1 423
    movabsq $4294967296, %rdx
    .loc 1 424
    movq %r11, %xmm14
    movabsq $4294967296, %r11
    cmpq %r11, %rdx
    movq %xmm14, %r11
    .loc 1 425
    addq $1, %rcx
    .loc 1 426
    cmpq $2, %rcx
    .loc 1 427
    jl .Lmv10_loop
    .loc 1 428
    movq -80(%rbp), %rax
    .loc 1 429
    movq %rax, -8(%rbp)
    .loc 1 430
    movq -72(%rbp), %rax
    .loc 1 431
    movq %rax, -16(%rbp)
    .loc 1 432
    movq %r11, %rax
    movq %rax, -24(%rbp)
    .loc 1 438
    movq -8(%rbp), %rax
    pushq %rax
    movabsq $1234605616436508552, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L168
    movq $-110, %rax
    jmp .Lfunc_end_167
.L168:
    .loc 1 439
    movq -16(%rbp), %rax
    pushq %rax
    movabsq $7373874951294615807, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L169
    movq $-111, %rax
    jmp .Lfunc_end_167
.L169:
    .loc 1 440
    movq -24(%rbp), %rax
    movq $777888999, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L170
    movq $-112, %rax
    jmp .Lfunc_end_167
.L170:
    .loc 1 441
    movq $118, %rax
.Lfunc_end_167:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv10_xmm12_and_eflags
classify_mv10_xmm12_and_eflags:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 452
    .loc 1 453
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    movq $99, %rax
    cltq
    movq %rax, %r11
    movq %r11, -32(%rbp)
    movq %r11, %rax
    .loc 1 455
    movabsq $0x33445566778899aa, %rax
    .loc 1 456
    movq %rax, -96(%rbp)
    .loc 1 457
    movabsq $0x554433221100ffee, %rax
    .loc 1 458
    movq %rax, -88(%rbp)
    .loc 1 459
    movaps -96(%rbp), %xmm15
    .loc 1 460
    movaps -96(%rbp), %xmm14
    .loc 1 461
    movaps -96(%rbp), %xmm13
    .loc 1 463
    movabsq $4294967296, %rax
    .loc 1 464
    movq %rax, 32(%rsp)
    .loc 1 465
    movq $3, %rcx
    .loc 1 466
    movq %r11, %xmm12
    movabsq $4294967296, %r11
    cmpq %r11, 8(%rsp, %rcx, 8)
    movq %xmm12, %r11
    .loc 1 467
    sete %al
    .loc 1 468
    movzbq %al, %rax
    movq %rax, -8(%rbp)
    .loc 1 470
    movaps %xmm15, -112(%rbp)
    .loc 1 471
    movaps %xmm14, -128(%rbp)
    .loc 1 472
    movaps %xmm13, -144(%rbp)
    .loc 1 473
    movq -144(%rbp), %rax
    .loc 1 474
    movq %rax, -16(%rbp)
    .loc 1 475
    movq -136(%rbp), %rax
    .loc 1 476
    movq %rax, -24(%rbp)
    .loc 1 479
    movq $77, %rcx
    .loc 1 480
    cmpq $99, %rcx
    .loc 1 481
    movq $0, %rax
    .loc 1 482
    sete %al
    .loc 1 483
    movq %rax, -32(%rbp)
    .loc 1 490
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L172
    movq $-120, %rax
    jmp .Lfunc_end_171
.L172:
    .loc 1 491
    movq -16(%rbp), %rax
    pushq %rax
    movabsq $3694171492931115434, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L173
    movq $-121, %rax
    jmp .Lfunc_end_171
.L173:
    .loc 1 492
    movq -24(%rbp), %rax
    pushq %rax
    movabsq $6144092013047381998, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L174
    movq $-122, %rax
    jmp .Lfunc_end_171
.L174:
    .loc 1 493
    movq -32(%rbp), %rax
    pushq %rax
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L175
    movq $-123, %rax
    jmp .Lfunc_end_171
.L175:
    .loc 1 494
    movq $128, %rax
.Lfunc_end_171:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv11_rmw_and_u64_imm
classify_mv11_rmw_and_u64_imm:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 506
    .loc 1 507
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 509
    movq $50, -64(%rbp)
    .loc 1 510
    movq $70, %r11
    .loc 1 511
    movq $999, %rax
    .loc 1 512
    movq %r11, %rax
    .loc 1 513
    addq -64(%rbp), %rax
    .loc 1 514
    movq %rax, -8(%rbp)
    .loc 1 516
    movq $0x55, %r11
    .loc 1 517
    movq $0x0f, %rcx
    .loc 1 518
    movq $0x1234, %rax
    .loc 1 519
    movq %r11, %rax
    .loc 1 520
    xorq %rcx, %rax
    .loc 1 521
    movq %rax, -16(%rbp)
    .loc 1 523
    movabsq $0x8877665544332211, %rax
    .loc 1 524
    movq %rax, -24(%rbp)
    .loc 1 530
    movq -8(%rbp), %rax
    movq $120, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L177
    movq $-130, %rax
    jmp .Lfunc_end_176
.L177:
    .loc 1 531
    movq -16(%rbp), %rax
    movq $90, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L178
    movq $-131, %rax
    jmp .Lfunc_end_176
.L178:
    .loc 1 532
    movq -24(%rbp), %rax
    pushq %rax
    movabsq $-8613303245920329199, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L179
    movq $-132, %rax
    jmp .Lfunc_end_176
.L179:
    .loc 1 533
    movq $138, %rax
.Lfunc_end_176:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl mv12_consume_escaped_ptr
mv12_consume_escaped_ptr:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $512, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    movq %rdx, -24(%rbp)
    .loc 1 547
    .loc 1 548
    .loc 1 549
    .loc 1 550
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -200(%rbp)
    movq %r11, %rax
.L181:
    movslq -200(%rbp), %rax
    movq $16, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L182
    movq $999999, %rax
    pushq %rax
    movslq -200(%rbp), %rax
    cltq
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    pushq %rax
    movq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    pushq %rax
    movq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    pushq %rax
    leaq -192(%rbp), %rax
    pushq %rax
    movslq -200(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L183:
    leaq -200(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L181
.L182:
    .loc 1 551
    leaq -192(%rbp), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    movq $1000029, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L184
    movq $-1, %rax
    jmp .Lfunc_end_180
.L184:
    .loc 1 552
    movq -8(%rbp), %rax
    movq (%rax), %rax
.Lfunc_end_180:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl mv12_tco_caller
mv12_tco_caller:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    movq %rdi, -8(%rbp)
    .loc 1 555
    .loc 1 556
    movq $42, %rax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 557
    leaq -16(%rbp), %rax
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 558
    movslq -8(%rbp), %rax
    testl %eax, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L186
    .loc 1 559
    xorl %eax, %eax
    jmp .Lfunc_end_185
.L186:
    .loc 1 561
    movq $20, %rax
    cltq
    pushq %rax
    movq $10, %rax
    cltq
    pushq %rax
    movq -24(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_187
    subq $8, %rsp
    call mv12_consume_escaped_ptr
    addq $8, %rsp
    jmp .Lcall_end_187
.Lcall_aligned_187:
    call mv12_consume_escaped_ptr
.Lcall_end_187:
.Lfunc_end_185:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv12_boost_and_tco
classify_mv12_boost_and_tco:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 564
    .loc 1 565
    movq $1, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_189
    subq $8, %rsp
    call mv12_tco_caller
    addq $8, %rsp
    jmp .Lcall_end_189
.Lcall_aligned_189:
    call mv12_tco_caller
.Lcall_end_189:
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 566
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 568
    movq $50, -64(%rbp)
    .loc 1 569
    movq $70, %rdx
    .loc 1 570
    movq $999, %rax
    .loc 1 571
    movq %rdx, %rax
    .loc 1 572
    addq -64(%rbp), %rax
    .loc 1 573
    movq %rax, -16(%rbp)
    .loc 1 575
    movq $88, %r11
    .loc 1 576
    movq $999, %rax
    .loc 1 577
    movq %r11, %rax
    .loc 1 578
    pxor %xmm0, %xmm0
    .loc 1 579
    movq %rax, -24(%rbp)
    .loc 1 584
    movq -8(%rbp), %rax
    movq $42, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L190
    movq $-140, %rax
    jmp .Lfunc_end_188
.L190:
    .loc 1 585
    movq -16(%rbp), %rax
    movq $120, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L191
    movq $-141, %rax
    jmp .Lfunc_end_188
.L191:
    .loc 1 586
    movq -24(%rbp), %rax
    movq $88, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L192
    movq $-142, %rax
    jmp .Lfunc_end_188
.L192:
    .loc 1 587
    movq $148, %rax
.Lfunc_end_188:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl mv13_check_rsp_alignment
mv13_check_rsp_alignment:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    .loc 1 599
    .loc 1 600
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -8(%rbp)
    movq %r11, %rax
    .loc 1 602
    movq %rbp, %rax
    movq %rax, -8(%rbp)
    .loc 1 604
    movq -8(%rbp), %rax
    pushq %rax
    movq $15, %rax
    popq %r11
    andq %r11, %rax
    pushq %rax
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L194
    xorl %eax, %eax
    jmp .L195
.L194:
    movq $-150, %rax
.L195:
.Lfunc_end_193:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl mv13_pass8_branch_caller
mv13_pass8_branch_caller:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    .loc 1 607
    .loc 1 608
    xorl %eax, %eax
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movl %r11d, (%rax)
    .loc 1 610
    movl -8(%rbp), %eax
    .loc 1 611
    testl %eax, %eax
    .loc 1 612
    jne .Lmv13_pass8_join
    .loc 1 613
    pushq %rax
    .loc 1 614
    jmp .Lmv13_pass8_done
    .loc 1 615
    .Lmv13_pass8_join:
    .loc 1 616
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_197
    subq $8, %rsp
    call mv13_check_rsp_alignment
    addq $8, %rsp
    jmp .Lcall_end_197
.Lcall_aligned_197:
    call mv13_check_rsp_alignment
.Lcall_end_197:
    cltq
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
    .loc 1 617
    .Lmv13_pass8_done:
    .loc 1 622
    movslq -16(%rbp), %rax
.Lfunc_end_196:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv13_pass8_and_pass5c
classify_mv13_pass8_and_pass5c:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    .loc 1 625
    .loc 1 626
    movq $1, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_199
    subq $8, %rsp
    call mv13_pass8_branch_caller
    addq $8, %rsp
    jmp .Lcall_end_199
.Lcall_aligned_199:
    call mv13_pass8_branch_caller
.Lcall_end_199:
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movl %r11d, (%rax)
    .loc 1 627
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 629
    movq $0x42424242, %rax
    movq %rax, %xmm0
    xorl %eax, %eax
    movq %xmm0, %rax
    addsd %xmm0, %xmm0
    .loc 1 630
    movq %rax, -16(%rbp)
    .loc 1 634
    movslq -8(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L200
    movq $-151, %rax
    jmp .Lfunc_end_198
.L200:
    .loc 1 635
    movq -16(%rbp), %rax
    movq $1111638594, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L201
    movq $-152, %rax
    jmp .Lfunc_end_198
.L201:
    .loc 1 636
    movq $158, %rax
.Lfunc_end_198:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv14_pass6_and_hex_imm
classify_mv14_pass6_and_hex_imm:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 648
    .loc 1 649
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 651
    movabsq $0x11223344deadbeef, %rax
    movq $5, %r11
    movl %eax, %eax
    .loc 1 652
    movq %rax, -8(%rbp)
    .loc 1 654
    movq $0xfffffffffffffff0, %rax
    movl %eax, %eax
    .loc 1 655
    movq %rax, -16(%rbp)
    .loc 1 657
    movq $0x10, %r11
    shlq $3, %r11
    .loc 1 658
    movq %r11, %rax
    movq %rax, -24(%rbp)
    .loc 1 664
    movq -8(%rbp), %rax
    pushq %rax
    movabsq $3735928559, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L203
    movq $-160, %rax
    jmp .Lfunc_end_202
.L203:
    .loc 1 665
    movq -16(%rbp), %rax
    pushq %rax
    movabsq $4294967280, %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L204
    movq $-161, %rax
    jmp .Lfunc_end_202
.L204:
    .loc 1 666
    movq -24(%rbp), %rax
    movq $128, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L205
    movq $-162, %rax
    jmp .Lfunc_end_202
.L205:
    .loc 1 667
    movq $168, %rax
.Lfunc_end_202:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv15_pass5c_pass6_and_addq_hex
classify_mv15_pass5c_pass6_and_addq_hex:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 681
    .loc 1 682
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -32(%rbp), %rax
    movq %r11, (%rax)
    .loc 1 684
    movq $0x42424242, %rax
    movq %rax, %xmm0
    xorl %eax, %eax
    movq %xmm0, %rax
    addsd %xmm0, %xmm0
    movq $5, %r11
    .loc 1 685
    movq %rax, -8(%rbp)
    .loc 1 687
    movq $42, -32(%rbp)
    movq -32(%rbp), %rax
    movq %rax, %r11
    movq $99, -40(%rbp)
    .loc 1 688
    movq %rax, -16(%rbp)
    .loc 1 689
    movq %r11, %rax
    movq %rax, -24(%rbp)
    .loc 1 691
    movq $100, %rax
    addq $0xfffffffffffffff0, %rax
    .loc 1 692
    movq %rax, -32(%rbp)
    .loc 1 699
    movq -8(%rbp), %rax
    movq $1111638594, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L207
    movq $-170, %rax
    jmp .Lfunc_end_206
.L207:
    .loc 1 700
    movq -16(%rbp), %rax
    movq $42, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L209
    movq -24(%rbp), %rax
    movq $42, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L209
    xorq %rax, %rax
    jmp .L210
.L209:
    movq $1, %rax
.L210:
    testl %eax, %eax
    je .L208
    movq $-171, %rax
    jmp .Lfunc_end_206
.L208:
    .loc 1 701
    movq -32(%rbp), %rax
    movq $84, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L211
    movq $-172, %rax
    jmp .Lfunc_end_206
.L211:
    .loc 1 702
    movq $178, %rax
.Lfunc_end_206:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl classify_mv16_pass0_pass2_leaq_sib_and_addb_hex
classify_mv16_pass0_pass2_leaq_sib_and_addb_hex:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $352, %rsp
    .loc 1 720
    .loc 1 721
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -8(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -16(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -24(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -32(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    leaq -40(%rbp), %rax
    movq %r11, (%rax)
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .loc 1 723
    movq $1, %rcx
    cmpq $2, %rcx
    movabsq $0, %rax
    sete %cl
    movzbq %cl, %rcx
    .loc 1 724
    movq %rcx, %rax
    movq %rax, -8(%rbp)
    .loc 1 726
    movq $77, %rax
    pushq %rax
    movq $99, %rdx
    movq %rax, %r11
    popq %rax
    .loc 1 727
    movq %rdx, %rax
    movq %rax, -16(%rbp)
    .loc 1 728
    movq %r11, %rax
    movq %rax, -24(%rbp)
    .loc 1 730
    movq $100, %rax
    addb $0xfffffffffffffff0, %al
    .loc 1 731
    movq %rax, -32(%rbp)
    .loc 1 733
    movq $1000, %rax
    movq $3, %rcx
    leaq 16(%rax,%rcx,8), %rdx
    .loc 1 734
    movq %rdx, %rax
    movq %rax, -40(%rbp)
    .loc 1 736
    .section .rodata
    .p2align 2
.LS_flit_other_mv16:
    .long 1065353216
    .text
    .section .rodata
    .p2align 2
.LS_flit_victim_mv16:
    .long 0
    .text
    movss .LS_flit_other_mv16(%rip), %xmm0
    leaq .LS_flit_victim_mv16(%rip), %rax
    movslq (%rax), %rax
    .loc 1 737
    movq %rax, -48(%rbp)
    .loc 1 746
    movq -8(%rbp), %rax
    xorq %r11, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L213
    movq $-180, %rax
    jmp .Lfunc_end_212
.L213:
    .loc 1 747
    movq -16(%rbp), %rax
    movq $99, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L215
    movq -24(%rbp), %rax
    movq $77, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L215
    xorq %rax, %rax
    jmp .L216
.L215:
    movq $1, %rax
.L216:
    testl %eax, %eax
    je .L214
    movq $-181, %rax
    jmp .Lfunc_end_212
.L214:
    .loc 1 748
    movq -32(%rbp), %rax
    movq $84, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L217
    movq $-182, %rax
    jmp .Lfunc_end_212
.L217:
    .loc 1 749
    movq -40(%rbp), %rax
    movq $1040, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L218
    movq $-183, %rax
    jmp .Lfunc_end_212
.L218:
    .loc 1 750
    movq -48(%rbp), %rax
    xorq %r11, %r11
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L219
    movq $-184, %rax
    jmp .Lfunc_end_212
.L219:
    .loc 1 751
    movq $188, %rax
.Lfunc_end_212:
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
    .loc 1 754
    .loc 1 755
    movabsq $4294967296, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_222
    subq $8, %rsp
    call classify
    addq $8, %rsp
    jmp .Lcall_end_222
.Lcall_aligned_222:
    call classify
.Lcall_end_222:
    cltq
    movq $7, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L221
    movq $1, %rax
    jmp .Lfunc_end_220
.L221:
    .loc 1 756
    movabsq $-4294967296, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_224
    subq $8, %rsp
    call classify
    addq $8, %rsp
    jmp .Lcall_end_224
.Lcall_aligned_224:
    call classify
.Lcall_end_224:
    cltq
    movq $9, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L223
    movq $2, %rax
    jmp .Lfunc_end_220
.L223:
    .loc 1 757
    xorl %eax, %eax
    cltq
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_226
    subq $8, %rsp
    call classify
    addq $8, %rsp
    jmp .Lcall_end_226
.Lcall_aligned_226:
    call classify
.Lcall_end_226:
    cltq
    movq $3, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L225
    movq $3, %rax
    jmp .Lfunc_end_220
.L225:
    .loc 1 758
    movabsq $4294967296, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_228
    subq $8, %rsp
    call classify_live_r11
    addq $8, %rsp
    jmp .Lcall_end_228
.Lcall_aligned_228:
    call classify_live_r11
.Lcall_end_228:
    cltq
    movq $7, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L227
    movq $4, %rax
    jmp .Lfunc_end_220
.L227:
    .loc 1 759
    movabsq $-4294967296, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_230
    subq $8, %rsp
    call classify_live_r11
    addq $8, %rsp
    jmp .Lcall_end_230
.Lcall_aligned_230:
    call classify_live_r11
.Lcall_end_230:
    cltq
    movq $9, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L229
    movq $5, %rax
    jmp .Lfunc_end_220
.L229:
    .loc 1 760
    xorl %eax, %eax
    cltq
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_232
    subq $8, %rsp
    call classify_live_r11
    addq $8, %rsp
    jmp .Lcall_end_232
.Lcall_aligned_232:
    call classify_live_r11
.Lcall_end_232:
    cltq
    movq $3, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L231
    movq $6, %rax
    jmp .Lfunc_end_220
.L231:
    .loc 1 761
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_234
    subq $8, %rsp
    call classify_alias_r11
    addq $8, %rsp
    jmp .Lcall_end_234
.Lcall_aligned_234:
    call classify_alias_r11
.Lcall_end_234:
    cltq
    movq $11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L233
    movq $7, %rax
    jmp .Lfunc_end_220
.L233:
    .loc 1 762
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_236
    subq $8, %rsp
    call classify_rsp_relative
    addq $8, %rsp
    jmp .Lcall_end_236
.Lcall_aligned_236:
    call classify_rsp_relative
.Lcall_end_236:
    cltq
    movq $22, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L235
    movq $8, %rax
    jmp .Lfunc_end_220
.L235:
    .loc 1 763
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_238
    subq $8, %rsp
    call classify_mem_r11_base
    addq $8, %rsp
    jmp .Lcall_end_238
.Lcall_aligned_238:
    call classify_mem_r11_base
.Lcall_end_238:
    cltq
    movq $33, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L237
    movq $9, %rax
    jmp .Lfunc_end_220
.L237:
    .loc 1 764
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_240
    subq $8, %rsp
    call classify_mem_r11_r10_sib
    addq $8, %rsp
    jmp .Lcall_end_240
.Lcall_aligned_240:
    call classify_mem_r11_r10_sib
.Lcall_end_240:
    cltq
    movq $44, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L239
    movq $10, %rax
    jmp .Lfunc_end_220
.L239:
    .loc 1 765
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_242
    subq $8, %rsp
    call classify_rsp_indexed
    addq $8, %rsp
    jmp .Lcall_end_242
.Lcall_aligned_242:
    call classify_rsp_indexed
.Lcall_end_242:
    cltq
    movq $55, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L241
    movq $11, %rax
    jmp .Lfunc_end_220
.L241:
    .loc 1 766
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_244
    subq $8, %rsp
    call classify_redzone_rsp
    addq $8, %rsp
    jmp .Lcall_end_244
.Lcall_aligned_244:
    call classify_redzone_rsp
.Lcall_end_244:
    cltq
    movq $66, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L243
    movq $12, %rax
    jmp .Lfunc_end_220
.L243:
    .loc 1 767
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_246
    subq $8, %rsp
    call classify_direct_rsp
    addq $8, %rsp
    jmp .Lcall_end_246
.Lcall_aligned_246:
    call classify_direct_rsp
.Lcall_end_246:
    cltq
    movq $77, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L245
    movq $13, %rax
    jmp .Lfunc_end_220
.L245:
    .loc 1 768
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_248
    subq $8, %rsp
    call classify_frame_scratch_alias
    addq $8, %rsp
    jmp .Lcall_end_248
.Lcall_aligned_248:
    call classify_frame_scratch_alias
.Lcall_end_248:
    cltq
    movq $88, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L247
    movq $14, %rax
    jmp .Lfunc_end_220
.L247:
    .loc 1 769
    movq $200, %rax
    pushq %rax
    movq $100, %rax
    pushq %rax
    movq $9, %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_250
    subq $8, %rsp
    call classify_variadic_scratch
    addq $8, %rsp
    jmp .Lcall_end_250
.Lcall_aligned_250:
    call classify_variadic_scratch
.Lcall_end_250:
    cltq
    movq $99, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L249
    movq $15, %rax
    jmp .Lfunc_end_220
.L249:
    .loc 1 770
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_252
    subq $8, %rsp
    call classify_xmm15_128bit_liveness
    addq $8, %rsp
    jmp .Lcall_end_252
.Lcall_aligned_252:
    call classify_xmm15_128bit_liveness
.Lcall_end_252:
    cltq
    movq $108, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L251
    movq $16, %rax
    jmp .Lfunc_end_220
.L251:
    .loc 1 771
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_254
    subq $8, %rsp
    call classify_mv10_backward_loop_xmm15
    addq $8, %rsp
    jmp .Lcall_end_254
.Lcall_aligned_254:
    call classify_mv10_backward_loop_xmm15
.Lcall_end_254:
    cltq
    movq $118, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L253
    movq $17, %rax
    jmp .Lfunc_end_220
.L253:
    .loc 1 772
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_256
    subq $8, %rsp
    call classify_mv10_xmm12_and_eflags
    addq $8, %rsp
    jmp .Lcall_end_256
.Lcall_aligned_256:
    call classify_mv10_xmm12_and_eflags
.Lcall_end_256:
    cltq
    movq $128, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L255
    movq $18, %rax
    jmp .Lfunc_end_220
.L255:
    .loc 1 773
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_258
    subq $8, %rsp
    call classify_mv11_rmw_and_u64_imm
    addq $8, %rsp
    jmp .Lcall_end_258
.Lcall_aligned_258:
    call classify_mv11_rmw_and_u64_imm
.Lcall_end_258:
    cltq
    movq $138, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L257
    movq $19, %rax
    jmp .Lfunc_end_220
.L257:
    .loc 1 774
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_260
    subq $8, %rsp
    call classify_mv12_boost_and_tco
    addq $8, %rsp
    jmp .Lcall_end_260
.Lcall_aligned_260:
    call classify_mv12_boost_and_tco
.Lcall_end_260:
    cltq
    movq $148, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L259
    movq $20, %rax
    jmp .Lfunc_end_220
.L259:
    .loc 1 775
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_262
    subq $8, %rsp
    call classify_mv13_pass8_and_pass5c
    addq $8, %rsp
    jmp .Lcall_end_262
.Lcall_aligned_262:
    call classify_mv13_pass8_and_pass5c
.Lcall_end_262:
    cltq
    movq $158, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L261
    movq $21, %rax
    jmp .Lfunc_end_220
.L261:
    .loc 1 776
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_264
    subq $8, %rsp
    call classify_mv14_pass6_and_hex_imm
    addq $8, %rsp
    jmp .Lcall_end_264
.Lcall_aligned_264:
    call classify_mv14_pass6_and_hex_imm
.Lcall_end_264:
    cltq
    movq $168, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L263
    movq $22, %rax
    jmp .Lfunc_end_220
.L263:
    .loc 1 777
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_266
    subq $8, %rsp
    call classify_mv15_pass5c_pass6_and_addq_hex
    addq $8, %rsp
    jmp .Lcall_end_266
.Lcall_aligned_266:
    call classify_mv15_pass5c_pass6_and_addq_hex
.Lcall_end_266:
    cltq
    movq $178, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L265
    movq $23, %rax
    jmp .Lfunc_end_220
.L265:
    .loc 1 778
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_268
    subq $8, %rsp
    call classify_mv16_pass0_pass2_leaq_sib_and_addb_hex
    addq $8, %rsp
    jmp .Lcall_end_268
.Lcall_aligned_268:
    call classify_mv16_pass0_pass2_leaq_sib_and_addb_hex
.Lcall_end_268:
    cltq
    movq $188, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L267
    movq $24, %rax
    jmp .Lfunc_end_220
.L267:
    .loc 1 779
    xorl %eax, %eax
    jmp .Lfunc_end_220
    xorl %eax, %eax
.Lfunc_end_220:
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

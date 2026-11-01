# ZCC asm begin
    .file 1 "zcc_m5_2_native_multiseed.c"
    .text
platform_main_begin:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $304, %rsp
    .file 1 "experiments/zcc_eai_006_g3_m5/zcc_m5_2_native_multiseed.c"
    .loc 1 174
.Lfunc_end_204:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
platform_main_end:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 175
.Lfunc_end_205:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
get_time_ns:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    .loc 1 2864
    .loc 1 2865
    .loc 1 2866
    leaq -16(%rbp), %rax
    pushq %rax
    movq $4, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_207
    subq $8, %rsp
    call clock_gettime
    addq $8, %rsp
    jmp .Lcall_end_207
.Lcall_aligned_207:
    call clock_gettime
.Lcall_end_207:
    cltq
    .loc 1 2867
    movq -16(%rbp), %rax
    pushq %rax
    movq $1000000000, %rax
    popq %r11
    imulq %r11, %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
.Lfunc_end_206:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
get_random_binary:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    .loc 1 2871
    .loc 1 2872
    leaq g_rng_state(%rip), %rax
    movq (%rax), %rax
    movq %rax, %r11
    movq %r11, -8(%rbp)
    movq %r11, %rax
    .loc 1 2873
    leaq -8(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -8(%rbp), %rax
    pushq %rax
    movq $13, %rax
    movq %rax, %rcx
    popq %rax
    shlq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2874
    leaq -8(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -8(%rbp), %rax
    pushq %rax
    movq $7, %rax
    movq %rax, %rcx
    popq %rax
    shrq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2875
    leaq -8(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -8(%rbp), %rax
    pushq %rax
    movq $17, %rax
    movq %rax, %rcx
    popq %rax
    shlq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2876
    movq -8(%rbp), %rax
    movq %rax, %r11
    leaq g_rng_state(%rip), %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2877
    movq -8(%rbp), %rax
    pushq %rax
    movq $1, %rax
    popq %r11
    andq %r11, %rax
    testq %rax, %rax
    je .L209
    .section .rodata
    .p2align 3
.LS_flit_0:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_0(%rip), %xmm0
    movq %xmm0, %rax
    jmp .L210
.L209:
    .section .rodata
    .p2align 3
.LS_flit_1:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_1(%rip), %xmm0
    movq %xmm0, %rax
    movabsq $-9223372036854775808, %r11
    xorq %r11, %rax
.L210:
    movq %rax, %xmm0
.Lfunc_end_208:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
generate_u_and_parity_targets:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    movq %rdi, -8(%rbp)
    .loc 1 2880
    .loc 1 2881
    .loc 1 2882
    movq -8(%rbp), %rax
    movq %rax, %r11
    leaq g_rng_state(%rip), %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2883
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L212:
    movslq -16(%rbp), %rax
    movq $1500, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L213
    .loc 1 2884
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_215
    subq $8, %rsp
    call get_random_binary
    addq $8, %rsp
    jmp .Lcall_end_215
.Lcall_aligned_215:
    call get_random_binary
.Lcall_end_215:
    movq %xmm0, %rax
    pushq %rax
    leaq s_u_seq(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L214:
    .loc 1 2883
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L212
.L213:
    .loc 1 2886
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L216:
    movslq -16(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L217
    .loc 1 2887
    movq $100, %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
    .loc 1 2888
    leaq s_u_seq(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq $2, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_u_seq(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq $4, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_y_train(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L218:
    .loc 1 2886
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L216
.L217:
    .loc 1 2890
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L219:
    movslq -16(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L220
    .loc 1 2891
    movq $1100, %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
    .loc 1 2892
    leaq s_u_seq(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq $2, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_u_seq(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq $4, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_y_test(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L221:
    .loc 1 2890
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L219
.L220:
.Lfunc_end_211:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
init_spatial_grating:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $320, %rsp
    .loc 1 2896
    .loc 1 2897
    .loc 1 2898
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -8(%rbp)
    movq %r11, %rax
.L223:
    movslq -8(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L224
    .loc 1 2899
    movslq -8(%rbp), %rax
    movslq %eax, %rax
    cvtsi2sdq %rax, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq $128, %rax
    movslq %eax, %rax
    cvtsi2sdq %rax, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -16(%rbp)
    movq %r11, %rax
    .loc 1 2900
    .section .rodata
    .p2align 3
.LS_flit_2:
    .quad 4602678819172646912
    .text
    movsd .LS_flit_2(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_3:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_3(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_4:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_4(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_5:
    .quad 4614256656552045848
    .text
    movsd .LS_flit_5(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_6:
    .quad 4613937818241073152
    .text
    movsd .LS_flit_6(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_226
    subq $8, %rsp
    call sin
    addq $8, %rsp
    jmp .Lcall_end_226
.Lcall_aligned_226:
    call sin
.Lcall_end_226:
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .loc 1 2901
    .section .rodata
    .p2align 3
.LS_flit_7:
    .quad 4605380978949069210
    .text
    movsd .LS_flit_7(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_8:
    .quad 4596373779694328218
    .text
    movsd .LS_flit_8(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_9:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_9(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_10:
    .quad 4614256656552045848
    .text
    movsd .LS_flit_10(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_11:
    .quad 4619567317775286272
    .text
    movsd .LS_flit_11(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_227
    subq $8, %rsp
    call cos
    addq $8, %rsp
    jmp .Lcall_end_227
.Lcall_aligned_227:
    call cos
.Lcall_end_227:
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    .loc 1 2900
    leaq g_spatial_grating(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L225:
    .loc 1 2898
    leaq -8(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L223
.L224:
.Lfunc_end_222:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
ql_init_reservoir:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $384, %rsp
    .loc 1 2905
    .loc 1 2906
    .loc 1 2907
    .section .rodata
    .p2align 3
.LS_flit_12:
    .quad 0
    .text
    movsd .LS_flit_12(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -16(%rbp)
    movq %r11, %rax
    .loc 1 2908
    movq $64, %rax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
    .loc 1 2909
    .section .rodata
    .p2align 3
.LS_flit_13:
    .quad 4620693217682128896
    .text
    movsd .LS_flit_13(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -32(%rbp)
    movq %r11, %rax
    .loc 1 2911
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -8(%rbp)
    movq %r11, %rax
.L229:
    movslq -8(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L230
    .loc 1 2912
    movslq -8(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movslq %eax, %rax
    cvtsi2sdq %rax, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -40(%rbp)
    movq %r11, %rax
    .loc 1 2913
    .section .rodata
    .p2align 3
.LS_flit_14:
    .quad 4602678819172646912
    .text
    movsd .LS_flit_14(%rip), %xmm0
    movq %xmm0, %rax
    movabsq $-9223372036854775808, %r11
    xorq %r11, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_232
    subq $8, %rsp
    call exp
    addq $8, %rsp
    jmp .Lcall_end_232
.Lcall_aligned_232:
    call exp
.Lcall_end_232:
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .loc 1 2914
    .section .rodata
    .p2align 3
.LS_flit_15:
    .quad 4602678819172646912
    .text
    movsd .LS_flit_15(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 2915
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_233
    subq $8, %rsp
    call cos
    addq $8, %rsp
    jmp .Lcall_end_233
.Lcall_aligned_233:
    call cos
.Lcall_end_233:
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2916
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_234
    subq $8, %rsp
    call sin
    addq $8, %rsp
    jmp .Lcall_end_234
.Lcall_aligned_234:
    call sin
.Lcall_end_234:
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2917
    leaq -16(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L231:
    .loc 1 2911
    leaq -8(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L229
.L230:
    .loc 1 2920
    .section .rodata
    .p2align 3
.LS_flit_16:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_16(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 2921
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -8(%rbp)
    movq %r11, %rax
.L235:
    movslq -8(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L236
    .loc 1 2922
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2923
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2924
    .section .rodata
    .p2align 3
.LS_flit_17:
    .quad 0
    .text
    movsd .LS_flit_17(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_V(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L237:
    .loc 1 2921
    leaq -8(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L235
.L236:
    .loc 1 2927
    movq $123456789, %rax
    movq %rax, %r11
    movq %r11, -72(%rbp)
    movq %r11, %rax
    .loc 1 2928
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -8(%rbp)
    movq %r11, %rax
.L238:
    movslq -8(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L239
    .loc 1 2929
    leaq -72(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -72(%rbp), %rax
    pushq %rax
    movq $13, %rax
    movq %rax, %rcx
    popq %rax
    shlq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2930
    leaq -72(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -72(%rbp), %rax
    pushq %rax
    movq $7, %rax
    movq %rax, %rcx
    popq %rax
    shrq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2931
    leaq -72(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -72(%rbp), %rax
    pushq %rax
    movq $17, %rax
    movq %rax, %rcx
    popq %rax
    shlq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2932
    movq -72(%rbp), %rax
    pushq %rax
    movq $128, %rax
    cltq
    movq %rax, %r11
    popq %rax
    xorq %rdx, %rdx
    divq %r11
    movq %rdx, %rax
    cltq
    pushq %rax
    leaq g_pair_a(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $2, %r11
    addq %r11, %rax
    popq %r11
    movl %r11d, (%rax)
    movq %r11, %rax
    .loc 1 2933
    leaq -72(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -72(%rbp), %rax
    pushq %rax
    movq $13, %rax
    movq %rax, %rcx
    popq %rax
    shlq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2934
    leaq -72(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -72(%rbp), %rax
    pushq %rax
    movq $7, %rax
    movq %rax, %rcx
    popq %rax
    shrq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2935
    leaq -72(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -72(%rbp), %rax
    pushq %rax
    movq $17, %rax
    movq %rax, %rcx
    popq %rax
    shlq %cl, %rax
    movq %rax, %r11
    popq %rax
    xorq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2936
    movq -72(%rbp), %rax
    pushq %rax
    movq $128, %rax
    cltq
    movq %rax, %r11
    popq %rax
    xorq %rdx, %rdx
    divq %r11
    movq %rdx, %rax
    cltq
    pushq %rax
    leaq g_pair_b(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $2, %r11
    addq %r11, %rax
    popq %r11
    movl %r11d, (%rax)
    movq %r11, %rax
.L240:
    .loc 1 2928
    leaq -8(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L238
.L239:
.Lfunc_end_228:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
ql_apply_hamiltonian_scalar:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    movsd %xmm0, -8(%rbp)
    movq %rdi, -16(%rbp)
    movq %rsi, -24(%rbp)
    .loc 1 2947
    .loc 1 2948
    .loc 1 2949
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L242:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L243
    .loc 1 2950
    movslq -32(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L245
    movq $127, %rax
    jmp .L246
.L245:
    movslq -32(%rbp), %rax
    movq $1, %r11
    subq %r11, %rax
    cltq
.L246:
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
    .loc 1 2951
    movslq -32(%rbp), %rax
    movq $127, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L247
    xorl %eax, %eax
    jmp .L248
.L247:
    movslq -32(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
.L248:
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
    .loc 1 2952
    movq -16(%rbp), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq -16(%rbp), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_18:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_18(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq -16(%rbp), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 2953
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movabsq $-9223372036854775808, %r11
    xorq %r11, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq -16(%rbp), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -24(%rbp), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L244:
    .loc 1 2949
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L242
.L243:
.Lfunc_end_241:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
ql_step_verlet_scalar_substeps:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $352, %rsp
    movsd %xmm0, -8(%rbp)
    movsd %xmm1, -16(%rbp)
    movq %rdi, -24(%rbp)
    .loc 1 2957
    .loc 1 2958
    .loc 1 2959
    .section .rodata
    .p2align 3
.LS_flit_19:
    .quad 4602678819172646912
    .text
    movsd .LS_flit_19(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .loc 1 2960
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L250:
    movslq -32(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L251
    .loc 1 2961
    leaq g_H_psi_i(%rip), %rax
    pushq %rax
    leaq g_psi_i(%rip), %rax
    pushq %rax
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rdi
    popq %rsi
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_253
    subq $8, %rsp
    call ql_apply_hamiltonian_scalar
    addq $8, %rsp
    jmp .Lcall_end_253
.Lcall_aligned_253:
    call ql_apply_hamiltonian_scalar
.Lcall_end_253:
    .loc 1 2962
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
.L254:
    movslq -40(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L255
    .loc 1 2963
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_H_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_20:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_20(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L256:
    .loc 1 2962
    leaq -40(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L254
.L255:
    .loc 1 2965
    leaq g_H_psi_r(%rip), %rax
    pushq %rax
    leaq g_psi_r(%rip), %rax
    pushq %rax
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rdi
    popq %rsi
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_257
    subq $8, %rsp
    call ql_apply_hamiltonian_scalar
    addq $8, %rsp
    jmp .Lcall_end_257
.Lcall_aligned_257:
    call ql_apply_hamiltonian_scalar
.Lcall_end_257:
    .loc 1 2966
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
.L258:
    movslq -40(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L259
    .loc 1 2967
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_H_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_21:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_21(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    subsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L260:
    .loc 1 2966
    leaq -40(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L258
.L259:
    .loc 1 2969
    leaq g_H_psi_i(%rip), %rax
    pushq %rax
    leaq g_psi_i(%rip), %rax
    pushq %rax
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rdi
    popq %rsi
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_261
    subq $8, %rsp
    call ql_apply_hamiltonian_scalar
    addq $8, %rsp
    jmp .Lcall_end_261
.Lcall_aligned_261:
    call ql_apply_hamiltonian_scalar
.Lcall_end_261:
    .loc 1 2970
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
.L262:
    movslq -40(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L263
    .loc 1 2971
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_H_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_22:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_22(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L264:
    .loc 1 2970
    leaq -40(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L262
.L263:
.L252:
    .loc 1 2960
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L250
.L251:
.Lfunc_end_249:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
ql_step_verlet_fused_substeps:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $400, %rsp
    movsd %xmm0, -8(%rbp)
    movsd %xmm1, -16(%rbp)
    movq %rdi, -24(%rbp)
    .loc 1 2985
    .loc 1 2986
    .loc 1 2987
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movabsq $-9223372036854775808, %r11
    xorq %r11, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .loc 1 2988
    .section .rodata
    .p2align 3
.LS_flit_23:
    .quad 4602678819172646912
    .text
    movsd .LS_flit_23(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 2989
    .loc 1 2991
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L266:
    movslq -32(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L267
    .loc 1 2993
    leaq g_psi_i(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 2994
    leaq g_psi_i(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    movq $1, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_24:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_24(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 2995
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 2996
    leaq g_psi_r(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 2997
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
.L269:
    movslq -40(%rbp), %rax
    movq $127, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L270
    .loc 1 2998
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 2999
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq $1, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -72(%rbp)
    movq %r11, %rax
    .loc 1 3000
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -80(%rbp)
    movq %r11, %rax
    .loc 1 3001
    leaq -72(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -80(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_25:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_25(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3002
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3003
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L271:
    .loc 1 2997
    leaq -40(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L269
.L270:
    .loc 1 3005
    leaq g_psi_i(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3006
    leaq g_psi_i(%rip), %rax
    movq $126, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_26:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_26(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3007
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3008
    leaq g_psi_r(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3011
    leaq g_psi_r(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3012
    leaq g_psi_r(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_r(%rip), %rax
    movq $1, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_27:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_27(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3013
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3014
    leaq g_psi_i(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    subsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3015
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
.L272:
    movslq -40(%rbp), %rax
    movq $127, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L273
    .loc 1 3016
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3017
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq $1, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -72(%rbp)
    movq %r11, %rax
    .loc 1 3018
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -80(%rbp)
    movq %r11, %rax
    .loc 1 3019
    leaq -72(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -80(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_28:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_28(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3020
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3021
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    subsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L274:
    .loc 1 3015
    leaq -40(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L272
.L273:
    .loc 1 3023
    leaq g_psi_r(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3024
    leaq g_psi_r(%rip), %rax
    movq $126, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_r(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_29:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_29(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3025
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3026
    leaq g_psi_i(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    subsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3029
    leaq g_psi_i(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3030
    leaq g_psi_i(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    movq $1, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_30:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_30(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3031
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3032
    leaq g_psi_r(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3033
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
.L275:
    movslq -40(%rbp), %rax
    movq $127, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L276
    .loc 1 3034
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3035
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq $1, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -72(%rbp)
    movq %r11, %rax
    .loc 1 3036
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -80(%rbp)
    movq %r11, %rax
    .loc 1 3037
    leaq -72(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -80(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_31:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_31(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3038
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3039
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L277:
    .loc 1 3033
    leaq -40(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L275
.L276:
    .loc 1 3041
    leaq g_psi_i(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3042
    leaq g_psi_i(%rip), %rax
    movq $126, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    xorq %r11, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_32:
    .quad 4611686018427387904
    .text
    movsd .LS_flit_32(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3043
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_V(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -64(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3044
    leaq g_psi_r(%rip), %rax
    movq $127, %r11
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L268:
    .loc 1 2991
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L266
.L267:
.Lfunc_end_265:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
    .globl ql_step_verlet_avx512_impl
ql_step_verlet_avx512_impl:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    movsd %xmm0, -8(%rbp)
    movsd %xmm1, -16(%rbp)
    movq %rdi, -24(%rbp)
    .loc 1 3058
    .loc 1 3059
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movabsq $-9223372036854775808, %r11
    xorq %r11, %rax
    movq %rax, %r11
    leaq g_avx_neg_J(%rip), %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3060
    .section .rodata
    .p2align 3
.LS_flit_33:
    .quad 4602678819172646912
    .text
    movsd .LS_flit_33(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    leaq g_avx_half_dt(%rip), %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3061
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    leaq g_avx_dt(%rip), %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3062
    movslq -24(%rbp), %rax
    movq %rax, %r11
    leaq g_avx_substeps(%rip), %rax
    movl %r11d, (%rax)
    movq %r11, %rax
    .loc 1 3064
    pushq %rax
	pushq %rcx
	pushq %rdx
	pushq %rsi
	pushq %rdi
	pushq %r8
	vbroadcastsd g_avx_two(%rip), %zmm5
	vbroadcastsd g_avx_neg_J(%rip), %zmm6
	leaq g_halo(%rip), %rsi
	leaq g_V(%rip), %rdx
	movl g_avx_substeps(%rip), %r8d
	testl %r8d, %r8d
	jle 7f
	0:
	vbroadcastsd g_avx_half_dt(%rip), %zmm7
	leaq g_psi_i(%rip), %rax
	movsd 1016(%rax), %xmm0
	movsd %xmm0, (%rsi)
	movsd (%rax), %xmm0
	movsd %xmm0, 1032(%rsi)
	xorq %rcx, %rcx
	1:
	vmovupd (%rax,%rcx,1), %zmm0
	vmovupd %zmm0, 8(%rsi,%rcx,1)
	addq $64, %rcx
	cmpq $1024, %rcx
	jl 1b
	leaq g_psi_r(%rip), %rdi
	xorq %rcx, %rcx
	2:
	vmovupd (%rsi,%rcx,1), %zmm1
	vmovupd 8(%rsi,%rcx,1), %zmm0
	vmovupd 16(%rsi,%rcx,1), %zmm2
	vaddpd %zmm2, %zmm1, %zmm1
	vmulpd %zmm0, %zmm5, %zmm3
	vsubpd %zmm3, %zmm1, %zmm1
	vmulpd %zmm1, %zmm6, %zmm1
	vmovupd (%rdx,%rcx,1), %zmm4
	vmulpd %zmm0, %zmm4, %zmm4
	vaddpd %zmm4, %zmm1, %zmm1
	vmulpd %zmm1, %zmm7, %zmm1
	vmovupd (%rdi,%rcx,1), %zmm2
	vaddpd %zmm1, %zmm2, %zmm2
	vmovupd %zmm2, (%rdi,%rcx,1)
	addq $64, %rcx
	cmpq $1024, %rcx
	jl 2b
	vbroadcastsd g_avx_dt(%rip), %zmm7
	leaq g_psi_r(%rip), %rax
	movsd 1016(%rax), %xmm0
	movsd %xmm0, (%rsi)
	movsd (%rax), %xmm0
	movsd %xmm0, 1032(%rsi)
	xorq %rcx, %rcx
	3:
	vmovupd (%rax,%rcx,1), %zmm0
	vmovupd %zmm0, 8(%rsi,%rcx,1)
	addq $64, %rcx
	cmpq $1024, %rcx
	jl 3b
	leaq g_psi_i(%rip), %rdi
	xorq %rcx, %rcx
	4:
	vmovupd (%rsi,%rcx,1), %zmm1
	vmovupd 8(%rsi,%rcx,1), %zmm0
	vmovupd 16(%rsi,%rcx,1), %zmm2
	vaddpd %zmm2, %zmm1, %zmm1
	vmulpd %zmm0, %zmm5, %zmm3
	vsubpd %zmm3, %zmm1, %zmm1
	vmulpd %zmm1, %zmm6, %zmm1
	vmovupd (%rdx,%rcx,1), %zmm4
	vmulpd %zmm0, %zmm4, %zmm4
	vaddpd %zmm4, %zmm1, %zmm1
	vmulpd %zmm1, %zmm7, %zmm1
	vmovupd (%rdi,%rcx,1), %zmm2
	vsubpd %zmm1, %zmm2, %zmm2
	vmovupd %zmm2, (%rdi,%rcx,1)
	addq $64, %rcx
	cmpq $1024, %rcx
	jl 4b
	vbroadcastsd g_avx_half_dt(%rip), %zmm7
	leaq g_psi_i(%rip), %rax
	movsd 1016(%rax), %xmm0
	movsd %xmm0, (%rsi)
	movsd (%rax), %xmm0
	movsd %xmm0, 1032(%rsi)
	xorq %rcx, %rcx
	5:
	vmovupd (%rax,%rcx,1), %zmm0
	vmovupd %zmm0, 8(%rsi,%rcx,1)
	addq $64, %rcx
	cmpq $1024, %rcx
	jl 5b
	leaq g_psi_r(%rip), %rdi
	xorq %rcx, %rcx
	6:
	vmovupd (%rsi,%rcx,1), %zmm1
	vmovupd 8(%rsi,%rcx,1), %zmm0
	vmovupd 16(%rsi,%rcx,1), %zmm2
	vaddpd %zmm2, %zmm1, %zmm1
	vmulpd %zmm0, %zmm5, %zmm3
	vsubpd %zmm3, %zmm1, %zmm1
	vmulpd %zmm1, %zmm6, %zmm1
	vmovupd (%rdx,%rcx,1), %zmm4
	vmulpd %zmm0, %zmm4, %zmm4
	vaddpd %zmm4, %zmm1, %zmm1
	vmulpd %zmm1, %zmm7, %zmm1
	vmovupd (%rdi,%rcx,1), %zmm2
	vaddpd %zmm1, %zmm2, %zmm2
	vmovupd %zmm2, (%rdi,%rcx,1)
	addq $64, %rcx
	cmpq $1024, %rcx
	jl 6b
	decl %r8d
	jnz 0b
	7:
	vzeroupper
	popq %r8
	popq %rdi
	popq %rsi
	popq %rdx
	popq %rcx
	popq %rax
	
.Lfunc_end_278:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
ql_measure_and_normalize_state:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    .loc 1 3203
    .loc 1 3204
    .section .rodata
    .p2align 3
.LS_flit_34:
    .quad 0
    .text
    movsd .LS_flit_34(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -8(%rbp)
    movq %r11, %rax
    .loc 1 3205
    .loc 1 3206
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L280:
    movslq -16(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L281
    .loc 1 3207
    leaq -8(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L282:
    .loc 1 3206
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L280
.L281:
    .loc 1 3209
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 3210
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_35:
    .quad 4397347889687374747
    .text
    movsd .LS_flit_35(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L283
    .loc 1 3211
    .section .rodata
    .p2align 3
.LS_flit_36:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_36(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -24(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -32(%rbp)
    movq %r11, %rax
    .loc 1 3212
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L284:
    movslq -16(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L285
    .loc 1 3213
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3214
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L286:
    .loc 1 3212
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L284
.L285:
.L283:
    .loc 1 3217
    leaq -24(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_279:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
step_reservoir_kernel:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $496, %rsp
    movsd %xmm0, -8(%rbp)
    movsd %xmm1, -16(%rbp)
    movsd %xmm2, -24(%rbp)
    movsd %xmm3, -32(%rbp)
    movq %rdi, -40(%rbp)
    movq %rsi, -48(%rbp)
    movq %rdx, -56(%rbp)
    movq %rcx, -64(%rbp)
    movq %r8, -72(%rbp)
    movq %r9, -80(%rbp)
    .loc 1 3231
    .loc 1 3232
    .loc 1 3233
    .section .rodata
    .p2align 3
.LS_flit_37:
    .quad 4604480259023595110
    .text
    movsd .LS_flit_37(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3234
    movq $64, %rax
    movq %rax, %r11
    movl %r11d, -104(%rbp)
    movq %r11, %rax
    .loc 1 3236
    movq -56(%rbp), %rax
    testq %rax, %rax
    je .L288
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_290
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_290
.Lcall_aligned_290:
    call get_time_ns
.Lcall_end_290:
    jmp .L289
.L288:
    xorl %eax, %eax
    cltq
.L289:
    movq %rax, %r11
    movq %r11, -112(%rbp)
    movq %r11, %rax
    .loc 1 3238
    .section .rodata
    .p2align 3
.LS_flit_38:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_38(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -104(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -104(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3239
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -88(%rbp)
    movq %r11, %rax
.L291:
    movslq -88(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L292
    .loc 1 3240
    leaq -24(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -8(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_spatial_grating(%rip), %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_V(%rip), %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L293:
    .loc 1 3239
    leaq -88(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L291
.L292:
    .loc 1 3243
    movq -56(%rbp), %rax
    testq %rax, %rax
    je .L294
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_296
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_296
.Lcall_aligned_296:
    call get_time_ns
.Lcall_end_296:
    jmp .L295
.L294:
    xorl %eax, %eax
    cltq
.L295:
    movq %rax, %r11
    movq %r11, -120(%rbp)
    movq %r11, %rax
    .loc 1 3245
    movslq -48(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L297
    .loc 1 3246
    movslq -40(%rbp), %rax
    pushq %rax
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rdi
    movl $2, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_298
    subq $8, %rsp
    call ql_step_verlet_scalar_substeps
    addq $8, %rsp
    jmp .Lcall_end_298
.Lcall_aligned_298:
    call ql_step_verlet_scalar_substeps
.Lcall_end_298:
    jmp .L299
.L297:
    .loc 1 3247
    movslq -48(%rbp), %rax
    movq $1, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L300
    .loc 1 3248
    movslq -40(%rbp), %rax
    pushq %rax
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rdi
    movl $2, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_301
    subq $8, %rsp
    call ql_step_verlet_fused_substeps
    addq $8, %rsp
    jmp .Lcall_end_301
.Lcall_aligned_301:
    call ql_step_verlet_fused_substeps
.Lcall_end_301:
    jmp .L302
.L300:
    .loc 1 3249
    .loc 1 3250
    leaq g_avx512_dispatch(%rip), %rax
    movq (%rax), %rax
    movq %rax, -472(%rbp)
    movslq -40(%rbp), %rax
    pushq %rax
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rdi
    movl $2, %eax
    movq -472(%rbp), %r10
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_ind_aligned_303
    subq $8, %rsp
    call *%r10
    addq $8, %rsp
    jmp .Lcall_ind_end_303
.Lcall_ind_aligned_303:
    call *%r10
.Lcall_ind_end_303:
.L302:
.L299:
    .loc 1 3253
    movq -56(%rbp), %rax
    testq %rax, %rax
    je .L304
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_306
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_306
.Lcall_aligned_306:
    call get_time_ns
.Lcall_end_306:
    jmp .L305
.L304:
    xorl %eax, %eax
    cltq
.L305:
    movq %rax, %r11
    movq %r11, -128(%rbp)
    movq %r11, %rax
    .loc 1 3255
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_307
    subq $8, %rsp
    call ql_measure_and_normalize_state
    addq $8, %rsp
    jmp .Lcall_end_307
.Lcall_aligned_307:
    call ql_measure_and_normalize_state
.Lcall_end_307:
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -136(%rbp)
    movq %r11, %rax
    .loc 1 3257
    movq -56(%rbp), %rax
    testq %rax, %rax
    je .L308
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_310
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_310
.Lcall_aligned_310:
    call get_time_ns
.Lcall_end_310:
    jmp .L309
.L308:
    xorl %eax, %eax
    cltq
.L309:
    movq %rax, %r11
    movq %r11, -144(%rbp)
    movq %r11, %rax
    .loc 1 3259
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -88(%rbp)
    movq %r11, %rax
.L311:
    movslq -88(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L312
    .loc 1 3260
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -152(%rbp)
    movq %r11, %rax
    .loc 1 3261
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -160(%rbp)
    movq %r11, %rax
    .loc 1 3262
    leaq -152(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_features(%rip), %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -88(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    xorq %r11, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3263
    leaq -160(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_features(%rip), %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -88(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3264
    leaq -152(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -152(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -160(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -160(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_features(%rip), %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -88(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $2, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L313:
    .loc 1 3259
    leaq -88(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L311
.L312:
    .loc 1 3266
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -88(%rbp)
    movq %r11, %rax
.L314:
    movslq -88(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L315
    .loc 1 3267
    leaq g_pair_a(%rip), %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $2, %r11
    addq %r11, %rax
    movslq (%rax), %rax
    movq %rax, %r11
    movl %r11d, -168(%rbp)
    movq %r11, %rax
    .loc 1 3268
    leaq g_pair_b(%rip), %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $2, %r11
    addq %r11, %rax
    movslq (%rax), %rax
    movq %rax, %r11
    movl %r11d, -176(%rbp)
    movq %r11, %rax
    .loc 1 3269
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -168(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_r(%rip), %rax
    pushq %rax
    movslq -176(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -168(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq g_psi_i(%rip), %rax
    pushq %rax
    movslq -176(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_features(%rip), %rax
    pushq %rax
    movq $384, %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L316:
    .loc 1 3266
    leaq -88(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L314
.L315:
    .loc 1 3272
    movq -56(%rbp), %rax
    testq %rax, %rax
    je .L317
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_319
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_319
.Lcall_aligned_319:
    call get_time_ns
.Lcall_end_319:
    jmp .L318
.L317:
    xorl %eax, %eax
    cltq
.L318:
    movq %rax, %r11
    movq %r11, -184(%rbp)
    movq %r11, %rax
    .loc 1 3274
    movq -56(%rbp), %rax
    testq %rax, %rax
    je .L320
    .loc 1 3275
    movq -120(%rbp), %rax
    pushq %rax
    movq -112(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    pushq %rax
    movq -56(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3276
    movq -128(%rbp), %rax
    pushq %rax
    movq -120(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    pushq %rax
    movq -64(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3277
    movq -144(%rbp), %rax
    pushq %rax
    movq -128(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    pushq %rax
    movq -72(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3278
    movq -184(%rbp), %rax
    pushq %rax
    movq -144(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    pushq %rax
    movq -80(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L320:
    .loc 1 3281
    leaq -136(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_287:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
run_reservoir_trajectory:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $400, %rsp
    movq %rdi, -8(%rbp)
    movsd %xmm0, -16(%rbp)
    movsd %xmm1, -24(%rbp)
    movsd %xmm2, -32(%rbp)
    movq %rsi, -40(%rbp)
    movq %rdx, -48(%rbp)
    movq %rcx, -56(%rbp)
    movq %r8, -64(%rbp)
    .loc 1 3293
    .loc 1 3294
    .loc 1 3295
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -88(%rbp)
    movq %r11, %rax
    .loc 1 3296
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -96(%rbp)
    movq %r11, %rax
    .loc 1 3298
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_322
    subq $8, %rsp
    call ql_init_reservoir
    addq $8, %rsp
    jmp .Lcall_end_322
.Lcall_aligned_322:
    call ql_init_reservoir
.Lcall_end_322:
    .loc 1 3300
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -72(%rbp)
    movq %r11, %rax
.L323:
    movslq -72(%rbp), %rax
    movq $1500, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L324
    .loc 1 3301
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    movslq -48(%rbp), %rax
    pushq %rax
    movq $25, %rax
    pushq %rax
    leaq -32(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -24(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -8(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    movl $4, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_326
    subq $8, %rsp
    call step_reservoir_kernel
    addq $8, %rsp
    jmp .Lcall_end_326
.Lcall_aligned_326:
    call step_reservoir_kernel
.Lcall_end_326:
    movq %xmm0, %rax
    .loc 1 3302
    movslq -72(%rbp), %rax
    movq $100, %r11
    cmpl %r11d, %eax
    setge %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L328
    movslq -72(%rbp), %rax
    movq $1100, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    setne %al
    movzbl %al, %eax
    jmp .L329
.L328:
    xorq %rax, %rax
.L329:
    testl %eax, %eax
    je .L327
    .loc 1 3303
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -80(%rbp)
    movq %r11, %rax
.L330:
    movslq -80(%rbp), %rax
    movq $512, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L331
    .loc 1 3304
    leaq g_features(%rip), %rax
    pushq %rax
    movslq -80(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -56(%rbp), %rax
    pushq %rax
    movslq -88(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -80(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L332:
    .loc 1 3303
    leaq -80(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L330
.L331:
    .loc 1 3306
    leaq -88(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L333
.L327:
    .loc 1 3307
    movslq -72(%rbp), %rax
    movq $1100, %r11
    cmpl %r11d, %eax
    setge %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L334
    .loc 1 3308
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -80(%rbp)
    movq %r11, %rax
.L335:
    movslq -80(%rbp), %rax
    movq $512, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L336
    .loc 1 3309
    leaq g_features(%rip), %rax
    pushq %rax
    movslq -80(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -64(%rbp), %rax
    pushq %rax
    movslq -96(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -80(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L337:
    .loc 1 3308
    leaq -80(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L335
.L336:
    .loc 1 3311
    leaq -96(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
.L334:
.L333:
.L325:
    .loc 1 3300
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
    jmp .L323
.L324:
.Lfunc_end_321:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
solve_ridge_subset:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $576, %rsp
    movq %rdi, -8(%rbp)
    movsd %xmm0, -16(%rbp)
    movq %rsi, -24(%rbp)
    movq %rdx, -32(%rbp)
    movq %rcx, -40(%rbp)
    .loc 1 3323
    .loc 1 3324
    .loc 1 3325
    movslq -8(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -72(%rbp)
    movq %r11, %rax
    .loc 1 3327
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L339:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L340
    .loc 1 3328
    .section .rodata
    .p2align 3
.LS_flit_39:
    .quad 0
    .text
    movsd .LS_flit_39(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_vecB(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3329
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -56(%rbp)
    movq %r11, %rax
.L342:
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L343
    .loc 1 3330
    .section .rodata
    .p2align 3
.LS_flit_40:
    .quad 0
    .text
    movsd .LS_flit_40(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3331
    .section .rodata
    .p2align 3
.LS_flit_41:
    .quad 0
    .text
    movsd .LS_flit_41(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L344:
    .loc 1 3329
    leaq -56(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L342
.L343:
.L341:
    .loc 1 3327
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L339
.L340:
    .loc 1 3335
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -64(%rbp)
    movq %r11, %rax
.L345:
    movslq -64(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L346
    .loc 1 3336
    leaq s_y_train(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -80(%rbp)
    movq %r11, %rax
    .loc 1 3337
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L348:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L349
    .loc 1 3338
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3339
    leaq s_vecB(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -80(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3340
    movslq -48(%rbp), %rax
    movq %rax, %r11
    movl %r11d, -56(%rbp)
    movq %r11, %rax
.L351:
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L352
    .loc 1 3341
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L353:
    .loc 1 3340
    leaq -56(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L351
.L352:
    .loc 1 3343
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L350:
    .loc 1 3337
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L348
.L349:
    .loc 1 3345
    leaq s_vecB(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -80(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3346
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_42:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_42(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L347:
    .loc 1 3335
    leaq -64(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L345
.L346:
    .loc 1 3349
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L354:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L355
    .loc 1 3350
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -56(%rbp)
    movq %r11, %rax
.L357:
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L358
    .loc 1 3351
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L359:
    .loc 1 3350
    leaq -56(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L357
.L358:
    .loc 1 3353
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L360
    .loc 1 3354
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -16(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L360:
.L356:
    .loc 1 3349
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L354
.L355:
    .loc 1 3358
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L361:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L362
    .loc 1 3359
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -56(%rbp)
    movq %r11, %rax
.L364:
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L365
    .loc 1 3360
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_matA_orig(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L366:
    .loc 1 3359
    leaq -56(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L364
.L365:
.L363:
    .loc 1 3358
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L361
.L362:
    .loc 1 3364
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L367:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L368
    .loc 1 3365
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -56(%rbp)
    movq %r11, %rax
.L370:
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L371
    .loc 1 3366
    leaq s_matA(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3367
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -64(%rbp)
    movq %r11, %rax
.L373:
    movslq -64(%rbp), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L374
    .loc 1 3368
    leaq -96(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    subsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L375:
    .loc 1 3367
    leaq -64(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L373
.L374:
    .loc 1 3370
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L376
    .loc 1 3371
    leaq s_matA_orig(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_43:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_43(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L377
    leaq s_matA_orig(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    jmp .L378
.L377:
    .section .rodata
    .p2align 3
.LS_flit_44:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_44(%rip), %xmm0
    movq %xmm0, %rax
.L378:
    movq %rax, %r11
    movq %r11, -104(%rbp)
    movq %r11, %rax
    .loc 1 3372
    .section .rodata
    .p2align 3
.LS_flit_45:
    .quad 4397347889687374747
    .text
    movsd .LS_flit_45(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -104(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -112(%rbp)
    movq %r11, %rax
    .loc 1 3373
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -112(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    setbe %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L379
    .loc 1 3374
    movq $-2, %rax
    jmp .Lfunc_end_338
.L379:
    .loc 1 3376
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    jmp .L380
.L376:
    .loc 1 3377
    .loc 1 3378
    leaq -96(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L380:
.L372:
    .loc 1 3365
    leaq -56(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L370
.L371:
.L369:
    .loc 1 3364
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L367
.L368:
    .loc 1 3383
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L381:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L382
    .loc 1 3384
    leaq s_vecB(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -120(%rbp)
    movq %r11, %rax
    .loc 1 3385
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -64(%rbp)
    movq %r11, %rax
.L384:
    movslq -64(%rbp), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L385
    .loc 1 3386
    leaq -120(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_vecY(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    subsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L386:
    .loc 1 3385
    leaq -64(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L384
.L385:
    .loc 1 3388
    leaq -120(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_vecY(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L383:
    .loc 1 3383
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L381
.L382:
    .loc 1 3391
    movslq -72(%rbp), %rax
    movq $1, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L387:
    movslq -48(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    setge %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L388
    .loc 1 3392
    leaq s_vecY(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -128(%rbp)
    movq %r11, %rax
    .loc 1 3393
    movslq -48(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -64(%rbp)
    movq %r11, %rax
.L390:
    movslq -64(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L391
    .loc 1 3394
    leaq -128(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_weights(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    subsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L392:
    .loc 1 3393
    leaq -64(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L390
.L391:
    .loc 1 3396
    leaq -128(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_cholL(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_weights(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L389:
    .loc 1 3391
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    subq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L387
.L388:
    .loc 1 3399
    .section .rodata
    .p2align 3
.LS_flit_46:
    .quad 0
    .text
    movsd .LS_flit_46(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -136(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_47:
    .quad 0
    .text
    movsd .LS_flit_47(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -144(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_48:
    .quad 0
    .text
    movsd .LS_flit_48(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -152(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_49:
    .quad 0
    .text
    movsd .LS_flit_49(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -160(%rbp)
    movq %r11, %rax
    .loc 1 3400
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L393:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L394
    .loc 1 3401
    leaq s_vecB(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movabsq $-9223372036854775808, %r11
    xorq %r11, %rax
    movq %rax, %r11
    movq %r11, -168(%rbp)
    movq %r11, %rax
    .loc 1 3402
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -56(%rbp)
    movq %r11, %rax
.L396:
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -72(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L397
    .loc 1 3403
    leaq -168(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_matA_orig(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_weights(%rip), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3404
    leaq -160(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_matA_orig(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_matA_orig(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    imulq $4104, %r11
    addq %r11, %rax
    pushq %rax
    movslq -56(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L398:
    .loc 1 3402
    leaq -56(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L396
.L397:
    .loc 1 3406
    leaq -136(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -168(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -168(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3407
    leaq -144(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_weights(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_weights(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3408
    leaq -152(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_vecB(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_vecB(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L395:
    .loc 1 3400
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L393
.L394:
    .loc 1 3410
    leaq -160(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -144(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -152(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -176(%rbp)
    movq %r11, %rax
    .loc 1 3411
    leaq -176(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_50:
    .quad 0
    .text
    movsd .LS_flit_50(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    sete %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L399
    .section .rodata
    .p2align 3
.LS_flit_51:
    .quad 0
    .text
    movsd .LS_flit_51(%rip), %xmm0
    movq %xmm0, %rax
    jmp .L400
.L399:
    leaq -136(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -176(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
.L400:
    movq %rax, %r11
    movq %r11, -184(%rbp)
    movq %r11, %rax
    .loc 1 3412
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    setne %al
    setp %r11b
    orb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L404
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_52:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_52(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_53:
    .quad 0
    .text
    movsd .LS_flit_53(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    sete %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L406
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_54:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_54(%rip), %xmm0
    movq %xmm0, %rax
    movabsq $-9223372036854775808, %r11
    xorq %r11, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_55:
    .quad 0
    .text
    movsd .LS_flit_55(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    sete %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L406
    xorq %rax, %rax
    jmp .L407
.L406:
    movq $1, %rax
.L407:
    testl %eax, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    setne %al
    movzbl %al, %eax
    jmp .L405
.L404:
    xorq %rax, %rax
.L405:
    testl %eax, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L402
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_56:
    .quad 4442235333156365461
    .text
    movsd .LS_flit_56(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L402
    xorq %rax, %rax
    jmp .L403
.L402:
    movq $1, %rax
.L403:
    testl %eax, %eax
    je .L401
    .loc 1 3413
    movq $-6, %rax
    jmp .Lfunc_end_338
.L401:
    .loc 1 3416
    .section .rodata
    .p2align 3
.LS_flit_57:
    .quad 0
    .text
    movsd .LS_flit_57(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -192(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_58:
    .quad 0
    .text
    movsd .LS_flit_58(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -200(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_59:
    .quad 0
    .text
    movsd .LS_flit_59(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -208(%rbp)
    movq %r11, %rax
    .loc 1 3417
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -64(%rbp)
    movq %r11, %rax
.L408:
    movslq -64(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L409
    .loc 1 3418
    leaq s_y_test(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -216(%rbp)
    movq %r11, %rax
    .loc 1 3419
    leaq s_weights(%rip), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -224(%rbp)
    movq %r11, %rax
    .loc 1 3420
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
.L411:
    movslq -48(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L412
    .loc 1 3421
    leaq -224(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_weights(%rip), %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -48(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L413:
    .loc 1 3420
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L411
.L412:
    .loc 1 3423
    leaq -216(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -224(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -232(%rbp)
    movq %r11, %rax
    .loc 1 3424
    leaq -192(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -232(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -232(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3425
    leaq -200(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -216(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3426
    leaq -208(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -216(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -216(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L410:
    .loc 1 3417
    leaq -64(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L408
.L409:
    .loc 1 3429
    leaq -192(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq $400, %rax
    movslq %eax, %rax
    cvtsi2sdq %rax, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -240(%rbp)
    movq %r11, %rax
    .loc 1 3430
    leaq -200(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq $400, %rax
    movslq %eax, %rax
    cvtsi2sdq %rax, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -248(%rbp)
    movq %r11, %rax
    .loc 1 3431
    leaq -208(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq $400, %rax
    movslq %eax, %rax
    cvtsi2sdq %rax, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -248(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -248(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -256(%rbp)
    movq %r11, %rax
    .loc 1 3432
    leaq -256(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_60:
    .quad 4427486594234968593
    .text
    movsd .LS_flit_60(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L414
    .section .rodata
    .p2align 3
.LS_flit_61:
    .quad 4607182418800017408
    .text
    movsd .LS_flit_61(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -240(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -256(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    jmp .L415
.L414:
    .section .rodata
    .p2align 3
.LS_flit_62:
    .quad 0
    .text
    movsd .LS_flit_62(%rip), %xmm0
    movq %xmm0, %rax
.L415:
    movq %rax, %r11
    movq %r11, -264(%rbp)
    movq %r11, %rax
    .loc 1 3434
    leaq -264(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -24(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3435
    leaq -240(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -32(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3436
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -40(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3437
    xorl %eax, %eax
.Lfunc_end_338:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
eval_q1_linear_phase:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3440
    .loc 1 3441
    .loc 1 3442
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L417:
    movslq -24(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L418
    .loc 1 3443
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L420:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L421
    .loc 1 3444
    movq -8(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    xorq %r11, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3445
    movq -8(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $128, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L422:
    .loc 1 3443
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L420
.L421:
.L419:
    .loc 1 3442
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L417
.L418:
    .loc 1 3448
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L423:
    movslq -24(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L424
    .loc 1 3449
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L426:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L427
    .loc 1 3450
    movq -16(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    xorq %r11, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3451
    movq -16(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $128, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L428:
    .loc 1 3449
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L426
.L427:
.L425:
    .loc 1 3448
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L423
.L424:
    .loc 1 3454
    .section .rodata
    .p2align 3
.LS_flit_63:
    .quad 0
    .text
    movsd .LS_flit_63(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -40(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_64:
    .quad 0
    .text
    movsd .LS_flit_64(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_65:
    .quad 0
    .text
    movsd .LS_flit_65(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 3455
    leaq -56(%rbp), %rax
    pushq %rax
    leaq -48(%rbp), %rax
    pushq %rax
    leaq -40(%rbp), %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_66:
    .quad 4547007122018943789
    .text
    movsd .LS_flit_66(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq $256, %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rsi
    popq %rdx
    popq %rcx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_430
    subq $8, %rsp
    call solve_ridge_subset
    addq $8, %rsp
    jmp .Lcall_end_430
.Lcall_aligned_430:
    call solve_ridge_subset
.Lcall_end_430:
    cltq
    xorq %r11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L429
    movq $2, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_431
    subq $8, %rsp
    call exit
    addq $8, %rsp
    jmp .Lcall_end_431
.Lcall_aligned_431:
    call exit
.Lcall_end_431:
.L429:
    .loc 1 3456
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_416:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
eval_q2_local_intensity:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3459
    .loc 1 3460
    .loc 1 3461
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L433:
    movslq -24(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L434
    .loc 1 3462
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L436:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L437
    .loc 1 3463
    movq -8(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $2, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L438:
    .loc 1 3462
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L436
.L437:
.L435:
    .loc 1 3461
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L433
.L434:
    .loc 1 3466
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L439:
    movslq -24(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L440
    .loc 1 3467
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L442:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L443
    .loc 1 3468
    movq -16(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $2, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L444:
    .loc 1 3467
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L442
.L443:
.L441:
    .loc 1 3466
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L439
.L440:
    .loc 1 3471
    .section .rodata
    .p2align 3
.LS_flit_67:
    .quad 0
    .text
    movsd .LS_flit_67(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -40(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_68:
    .quad 0
    .text
    movsd .LS_flit_68(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_69:
    .quad 0
    .text
    movsd .LS_flit_69(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 3472
    leaq -56(%rbp), %rax
    pushq %rax
    leaq -48(%rbp), %rax
    pushq %rax
    leaq -40(%rbp), %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_70:
    .quad 4547007122018943789
    .text
    movsd .LS_flit_70(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq $128, %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rsi
    popq %rdx
    popq %rcx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_446
    subq $8, %rsp
    call solve_ridge_subset
    addq $8, %rsp
    jmp .Lcall_end_446
.Lcall_aligned_446:
    call solve_ridge_subset
.Lcall_end_446:
    cltq
    xorq %r11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L445
    movq $2, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_447
    subq $8, %rsp
    call exit
    addq $8, %rsp
    jmp .Lcall_end_447
.Lcall_aligned_447:
    call exit
.Lcall_end_447:
.L445:
    .loc 1 3473
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_432:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
eval_q4_all_quadratic:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3476
    .loc 1 3477
    .loc 1 3478
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L449:
    movslq -24(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L450
    .loc 1 3479
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L452:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L453
    .loc 1 3480
    movq -8(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $2, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L454:
    .loc 1 3479
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L452
.L453:
    .loc 1 3482
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L455:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L456
    .loc 1 3483
    movq -8(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $384, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $128, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L457:
    .loc 1 3482
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L455
.L456:
.L451:
    .loc 1 3478
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L449
.L450:
    .loc 1 3486
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L458:
    movslq -24(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L459
    .loc 1 3487
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L461:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L462
    .loc 1 3488
    movq -16(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $3, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    popq %r11
    imulq %r11, %rax
    cltq
    movq $2, %r11
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L463:
    .loc 1 3487
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L461
.L462:
    .loc 1 3490
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L464:
    movslq -32(%rbp), %rax
    movq $128, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L465
    .loc 1 3491
    movq -16(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $384, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movq $128, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L466:
    .loc 1 3490
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L464
.L465:
.L460:
    .loc 1 3486
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L458
.L459:
    .loc 1 3494
    .section .rodata
    .p2align 3
.LS_flit_71:
    .quad 0
    .text
    movsd .LS_flit_71(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -40(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_72:
    .quad 0
    .text
    movsd .LS_flit_72(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_73:
    .quad 0
    .text
    movsd .LS_flit_73(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 3495
    leaq -56(%rbp), %rax
    pushq %rax
    leaq -48(%rbp), %rax
    pushq %rax
    leaq -40(%rbp), %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_74:
    .quad 4547007122018943789
    .text
    movsd .LS_flit_74(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq $256, %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rsi
    popq %rdx
    popq %rcx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_468
    subq $8, %rsp
    call solve_ridge_subset
    addq $8, %rsp
    jmp .Lcall_end_468
.Lcall_aligned_468:
    call solve_ridge_subset
.Lcall_end_468:
    cltq
    xorq %r11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L467
    movq $2, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_469
    subq $8, %rsp
    call exit
    addq $8, %rsp
    jmp .Lcall_end_469
.Lcall_aligned_469:
    call exit
.Lcall_end_469:
.L467:
    .loc 1 3496
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_448:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
eval_q5_full_candidate:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3499
    .loc 1 3500
    .loc 1 3501
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L471:
    movslq -24(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L472
    .loc 1 3502
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L474:
    movslq -32(%rbp), %rax
    movq $512, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L475
    movq -8(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L476:
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L474
.L475:
.L473:
    .loc 1 3501
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L471
.L472:
    .loc 1 3504
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L477:
    movslq -24(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L478
    .loc 1 3505
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L480:
    movslq -32(%rbp), %rax
    movq $512, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L481
    movq -16(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L482:
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L480
.L481:
.L479:
    .loc 1 3504
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L477
.L478:
    .loc 1 3507
    .section .rodata
    .p2align 3
.LS_flit_75:
    .quad 0
    .text
    movsd .LS_flit_75(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -40(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_76:
    .quad 0
    .text
    movsd .LS_flit_76(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_77:
    .quad 0
    .text
    movsd .LS_flit_77(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 3508
    leaq -56(%rbp), %rax
    pushq %rax
    leaq -48(%rbp), %rax
    pushq %rax
    leaq -40(%rbp), %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_78:
    .quad 4547007122018943789
    .text
    movsd .LS_flit_78(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq $512, %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rsi
    popq %rdx
    popq %rcx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_484
    subq $8, %rsp
    call solve_ridge_subset
    addq $8, %rsp
    jmp .Lcall_end_484
.Lcall_aligned_484:
    call solve_ridge_subset
.Lcall_end_484:
    cltq
    xorq %r11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L483
    movq $2, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_485
    subq $8, %rsp
    call exit
    addq $8, %rsp
    jmp .Lcall_end_485
.Lcall_aligned_485:
    call exit
.Lcall_end_485:
.L483:
    .loc 1 3509
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_470:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
eval_b0_linear_delay:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $368, %rsp
    movq %rdi, -8(%rbp)
    .loc 1 3512
    .loc 1 3513
    .loc 1 3514
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L487:
    movslq -16(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L488
    .loc 1 3515
    movq $100, %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
    .loc 1 3516
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L490:
    movslq -24(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L491
    .loc 1 3517
    movq -8(%rbp), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq $1, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L492:
    .loc 1 3516
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L490
.L491:
.L489:
    .loc 1 3514
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L487
.L488:
    .loc 1 3520
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L493:
    movslq -16(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L494
    .loc 1 3521
    movq $1100, %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
    .loc 1 3522
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L496:
    movslq -24(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L497
    .loc 1 3523
    movq -8(%rbp), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq $1, %r11
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L498:
    .loc 1 3522
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L496
.L497:
.L495:
    .loc 1 3520
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L493
.L494:
    .loc 1 3526
    .section .rodata
    .p2align 3
.LS_flit_79:
    .quad 0
    .text
    movsd .LS_flit_79(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_80:
    .quad 0
    .text
    movsd .LS_flit_80(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_81:
    .quad 0
    .text
    movsd .LS_flit_81(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -64(%rbp)
    movq %r11, %rax
    .loc 1 3527
    leaq -64(%rbp), %rax
    pushq %rax
    leaq -56(%rbp), %rax
    pushq %rax
    leaq -48(%rbp), %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_82:
    .quad 4547007122018943789
    .text
    movsd .LS_flit_82(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq $9, %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rsi
    popq %rdx
    popq %rcx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_500
    subq $8, %rsp
    call solve_ridge_subset
    addq $8, %rsp
    jmp .Lcall_end_500
.Lcall_aligned_500:
    call solve_ridge_subset
.Lcall_end_500:
    cltq
    xorq %r11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L499
    movq $2, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_501
    subq $8, %rsp
    call exit
    addq $8, %rsp
    jmp .Lcall_end_501
.Lcall_aligned_501:
    call exit
.Lcall_end_501:
.L499:
    .loc 1 3528
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_486:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
eval_v1_volterra:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $400, %rsp
    movq %rdi, -8(%rbp)
    .loc 1 3531
    .loc 1 3532
    .loc 1 3533
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L503:
    movslq -16(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L504
    .loc 1 3534
    movq $100, %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -40(%rbp)
    movq %r11, %rax
    .loc 1 3535
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -48(%rbp)
    movq %r11, %rax
    .loc 1 3536
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L506:
    movslq -24(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L507
    .loc 1 3537
    movq -8(%rbp), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L508:
    .loc 1 3536
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L506
.L507:
    .loc 1 3539
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L509:
    movslq -24(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L510
    .loc 1 3540
    movslq -24(%rbp), %rax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L512:
    movslq -32(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L513
    .loc 1 3541
    movq -8(%rbp), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq -8(%rbp), %rax
    pushq %rax
    movslq -40(%rbp), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_train_X(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    leaq -48(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L514:
    .loc 1 3540
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L512
.L513:
.L511:
    .loc 1 3539
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L509
.L510:
.L505:
    .loc 1 3533
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L503
.L504:
    .loc 1 3545
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -16(%rbp)
    movq %r11, %rax
.L515:
    movslq -16(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L516
    .loc 1 3546
    movq $1100, %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    cltq
    movq %rax, %r11
    movl %r11d, -56(%rbp)
    movq %r11, %rax
    .loc 1 3547
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -64(%rbp)
    movq %r11, %rax
    .loc 1 3548
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L518:
    movslq -24(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L519
    .loc 1 3549
    movq -8(%rbp), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    leaq -64(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L520:
    .loc 1 3548
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L518
.L519:
    .loc 1 3551
    movq $1, %rax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L521:
    movslq -24(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L522
    .loc 1 3552
    movslq -24(%rbp), %rax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L524:
    movslq -32(%rbp), %rax
    movq $9, %r11
    cmpl %r11d, %eax
    setle %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L525
    .loc 1 3553
    movq -8(%rbp), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -24(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq -8(%rbp), %rax
    pushq %rax
    movslq -56(%rbp), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    cltq
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_sub_test_X(%rip), %rax
    pushq %rax
    movslq -16(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    leaq -64(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L526:
    .loc 1 3552
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L524
.L525:
.L523:
    .loc 1 3551
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L521
.L522:
.L517:
    .loc 1 3545
    leaq -16(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L515
.L516:
    .loc 1 3557
    .section .rodata
    .p2align 3
.LS_flit_83:
    .quad 0
    .text
    movsd .LS_flit_83(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -72(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_84:
    .quad 0
    .text
    movsd .LS_flit_84(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -80(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_85:
    .quad 0
    .text
    movsd .LS_flit_85(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3558
    leaq -88(%rbp), %rax
    pushq %rax
    leaq -80(%rbp), %rax
    pushq %rax
    leaq -72(%rbp), %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_86:
    .quad 4547007122018943789
    .text
    movsd .LS_flit_86(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq $54, %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rsi
    popq %rdx
    popq %rcx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_528
    subq $8, %rsp
    call solve_ridge_subset
    addq $8, %rsp
    jmp .Lcall_end_528
.Lcall_aligned_528:
    call solve_ridge_subset
.Lcall_end_528:
    cltq
    xorq %r11, %r11
    cmpl %r11d, %eax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L527
    movq $2, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_529
    subq $8, %rsp
    call exit
    addq $8, %rsp
    jmp .Lcall_end_529
.Lcall_aligned_529:
    call exit
.Lcall_end_529:
.L527:
    .loc 1 3559
    leaq -72(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
.Lfunc_end_502:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
compare_u64:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3562
    .loc 1 3563
    movq -8(%rbp), %rax
    movq (%rax), %rax
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 3564
    movq -16(%rbp), %rax
    movq (%rax), %rax
    movq %rax, %r11
    movq %r11, -32(%rbp)
    movq %r11, %rax
    .loc 1 3565
    movq -24(%rbp), %rax
    pushq %rax
    movq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setb %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L531
    movq $-1, %rax
    jmp .Lfunc_end_530
.L531:
    .loc 1 3566
    movq -24(%rbp), %rax
    pushq %rax
    movq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    seta %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L532
    movq $1, %rax
    jmp .Lfunc_end_530
.L532:
    .loc 1 3567
    xorl %eax, %eax
.Lfunc_end_530:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
profile_kernel_10k:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $544, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3570
    .loc 1 3571
    .loc 1 3572
    .section .rodata
    .p2align 3
.LS_flit_87:
    .quad 4627730092099895296
    .text
    movsd .LS_flit_87(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -40(%rbp)
    movq %r11, %rax
    .loc 1 3573
    .section .rodata
    .p2align 3
.LS_flit_88:
    .quad 4636737291354636288
    .text
    movsd .LS_flit_88(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -48(%rbp)
    movq %r11, %rax
    .loc 1 3574
    .section .rodata
    .p2align 3
.LS_flit_89:
    .quad 4551510721646314285
    .text
    movsd .LS_flit_89(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -56(%rbp)
    movq %r11, %rax
    .loc 1 3575
    movq $25, %rax
    movq %rax, %r11
    movl %r11d, -64(%rbp)
    movq %r11, %rax
    .loc 1 3577
    movabsq $88172645463325252, %rax
    movq %rax, %r11
    leaq g_rng_state(%rip), %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3578
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_534
    subq $8, %rsp
    call ql_init_reservoir
    addq $8, %rsp
    jmp .Lcall_end_534
.Lcall_aligned_534:
    call ql_init_reservoir
.Lcall_end_534:
    .loc 1 3580
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -24(%rbp)
    movq %r11, %rax
.L535:
    movslq -24(%rbp), %rax
    movq $100, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L536
    .loc 1 3581
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_538
    subq $8, %rsp
    call get_random_binary
    addq $8, %rsp
    jmp .Lcall_end_538
.Lcall_aligned_538:
    call get_random_binary
.Lcall_end_538:
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -72(%rbp)
    movq %r11, %rax
    .loc 1 3582
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    movslq -8(%rbp), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -72(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    movl $4, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_539
    subq $8, %rsp
    call step_reservoir_kernel
    addq $8, %rsp
    jmp .Lcall_end_539
.Lcall_aligned_539:
    call step_reservoir_kernel
.Lcall_end_539:
    movq %xmm0, %rax
.L537:
    .loc 1 3580
    leaq -24(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L535
.L536:
    .loc 1 3585
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_540
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_540
.Lcall_aligned_540:
    call get_time_ns
.Lcall_end_540:
    movq %rax, %r11
    movq %r11, -80(%rbp)
    movq %r11, %rax
    .loc 1 3586
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L541:
    movslq -32(%rbp), %rax
    movq $10000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L542
    .loc 1 3587
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_544
    subq $8, %rsp
    call get_random_binary
    addq $8, %rsp
    jmp .Lcall_end_544
.Lcall_aligned_544:
    call get_random_binary
.Lcall_end_544:
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -88(%rbp)
    movq %r11, %rax
    .loc 1 3588
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_545
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_545
.Lcall_aligned_545:
    call get_time_ns
.Lcall_end_545:
    movq %rax, %r11
    movq %r11, -96(%rbp)
    movq %r11, %rax
    .loc 1 3589
    movslq -32(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L546
    .loc 1 3590
    .loc 1 3591
    leaq -128(%rbp), %rax
    pushq %rax
    leaq -120(%rbp), %rax
    pushq %rax
    leaq -112(%rbp), %rax
    pushq %rax
    leaq -104(%rbp), %rax
    pushq %rax
    movslq -8(%rbp), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    movl $4, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_547
    subq $8, %rsp
    call step_reservoir_kernel
    addq $8, %rsp
    jmp .Lcall_end_547
.Lcall_aligned_547:
    call step_reservoir_kernel
.Lcall_end_547:
    movq %xmm0, %rax
    .loc 1 3592
    movq -104(%rbp), %rax
    pushq %rax
    leaq g_phase_p1_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3593
    movq -112(%rbp), %rax
    pushq %rax
    leaq g_phase_p2_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3594
    movq -120(%rbp), %rax
    pushq %rax
    leaq g_phase_p3_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3595
    movq -128(%rbp), %rax
    pushq %rax
    leaq g_phase_p4_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    jmp .L548
.L546:
    .loc 1 3596
    .loc 1 3597
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    xorl %eax, %eax
    cltq
    pushq %rax
    movslq -8(%rbp), %rax
    pushq %rax
    movslq -64(%rbp), %rax
    pushq %rax
    leaq -56(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -48(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -40(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -88(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    popq %r9
    movl $4, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_549
    subq $8, %rsp
    call step_reservoir_kernel
    addq $8, %rsp
    jmp .Lcall_end_549
.Lcall_aligned_549:
    call step_reservoir_kernel
.Lcall_end_549:
    movq %xmm0, %rax
.L548:
    .loc 1 3599
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_550
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_550
.Lcall_aligned_550:
    call get_time_ns
.Lcall_end_550:
    movq %rax, %r11
    movq %r11, -136(%rbp)
    movq %r11, %rax
    .loc 1 3600
    movq -136(%rbp), %rax
    pushq %rax
    movq -96(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    pushq %rax
    leaq g_latencies_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.L543:
    .loc 1 3586
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L541
.L542:
    .loc 1 3602
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_551
    subq $8, %rsp
    call get_time_ns
    addq $8, %rsp
    jmp .Lcall_end_551
.Lcall_aligned_551:
    call get_time_ns
.Lcall_end_551:
    movq %rax, %r11
    movq %r11, -144(%rbp)
    movq %r11, %rax
    .loc 1 3604
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -152(%rbp)
    movq %r11, %rax
    .loc 1 3605
    movq $-1, %rax
    movq %rax, %r11
    movq %r11, -160(%rbp)
    movq %r11, %rax
    .loc 1 3606
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -168(%rbp)
    movq %r11, %rax
    .loc 1 3607
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L552:
    movslq -32(%rbp), %rax
    movq $10000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L553
    .loc 1 3608
    leaq g_latencies_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    movq %rax, %r11
    movq %r11, -176(%rbp)
    movq %r11, %rax
    .loc 1 3609
    leaq -152(%rbp), %rax
    pushq %rax
    movq (%rax), %rax
    pushq %rax
    movq -176(%rbp), %rax
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3610
    movq -176(%rbp), %rax
    pushq %rax
    movq -160(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setb %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L555
    movq -176(%rbp), %rax
    movq %rax, %r11
    movq %r11, -160(%rbp)
    movq %r11, %rax
.L555:
    .loc 1 3611
    movq -176(%rbp), %rax
    pushq %rax
    movq -168(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    seta %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L556
    movq -176(%rbp), %rax
    movq %rax, %r11
    movq %r11, -168(%rbp)
    movq %r11, %rax
.L556:
.L554:
    .loc 1 3607
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L552
.L553:
    .loc 1 3614
    movq -152(%rbp), %rax
    testq %rax, %rax
    js .L557
    cvtsi2sdq %rax, %xmm0
    jmp .L558
.L557:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L558:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_90:
    .quad 4666723172467343360
    .text
    movsd .LS_flit_90(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -184(%rbp)
    movq %r11, %rax
    .loc 1 3615
    .section .rodata
    .p2align 3
.LS_flit_91:
    .quad 0
    .text
    movsd .LS_flit_91(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -192(%rbp)
    movq %r11, %rax
    .loc 1 3616
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L559:
    movslq -32(%rbp), %rax
    movq $10000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L560
    .loc 1 3617
    leaq g_latencies_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L562
    cvtsi2sdq %rax, %xmm0
    jmp .L563
.L562:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L563:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -200(%rbp)
    movq %r11, %rax
    .loc 1 3618
    leaq -192(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -200(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -200(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    mulsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L561:
    .loc 1 3616
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L559
.L560:
    .loc 1 3620
    leaq -192(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_92:
    .quad 4666723172467343360
    .text
    movsd .LS_flit_92(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    sqrtsd %xmm0, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -208(%rbp)
    movq %r11, %rax
    .loc 1 3622
    leaq compare_u64(%rip), %rax
    pushq %rax
    movq $8, %rax
    pushq %rax
    movq $10000, %rax
    pushq %rax
    leaq g_latencies_ns(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_564
    subq $8, %rsp
    call qsort
    addq $8, %rsp
    jmp .Lcall_end_564
.Lcall_aligned_564:
    call qsort
.Lcall_end_564:
    .loc 1 3624
    .section .rodata
    .p2align 3
.LS_flit_93:
    .quad 0
    .text
    movsd .LS_flit_93(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -216(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_94:
    .quad 0
    .text
    movsd .LS_flit_94(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -224(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_95:
    .quad 0
    .text
    movsd .LS_flit_95(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -232(%rbp)
    movq %r11, %rax
    .section .rodata
    .p2align 3
.LS_flit_96:
    .quad 0
    .text
    movsd .LS_flit_96(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -240(%rbp)
    movq %r11, %rax
    .loc 1 3625
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -32(%rbp)
    movq %r11, %rax
.L565:
    movslq -32(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L566
    .loc 1 3626
    leaq -216(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_phase_p1_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L568
    cvtsi2sdq %rax, %xmm0
    jmp .L569
.L568:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L569:
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3627
    leaq -224(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_phase_p2_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L570
    cvtsi2sdq %rax, %xmm0
    jmp .L571
.L570:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L571:
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3628
    leaq -232(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_phase_p3_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L572
    cvtsi2sdq %rax, %xmm0
    jmp .L573
.L572:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L573:
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3629
    leaq -240(%rbp), %rax
    pushq %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq g_phase_p4_ns(%rip), %rax
    pushq %rax
    movslq -32(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L574
    cvtsi2sdq %rax, %xmm0
    jmp .L575
.L574:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L575:
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %rax, %xmm0
    movq %r11, %xmm1
    addsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    popq %rax
    movq %r11, (%rax)
    movq %r11, %rax
.L567:
    .loc 1 3625
    leaq -32(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L565
.L566:
    .loc 1 3632
    leaq -184(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_97:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_97(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3633
    leaq -208(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_98:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_98(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $8, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3634
    movq -160(%rbp), %rax
    testq %rax, %rax
    js .L576
    cvtsi2sdq %rax, %xmm0
    jmp .L577
.L576:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L577:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_99:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_99(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $16, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3635
    leaq g_latencies_ns(%rip), %rax
    movq $5000, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L578
    cvtsi2sdq %rax, %xmm0
    jmp .L579
.L578:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L579:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_100:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_100(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $24, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3636
    leaq g_latencies_ns(%rip), %rax
    movq $9000, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L580
    cvtsi2sdq %rax, %xmm0
    jmp .L581
.L580:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L581:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_101:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_101(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $32, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3637
    leaq g_latencies_ns(%rip), %rax
    movq $9500, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L582
    cvtsi2sdq %rax, %xmm0
    jmp .L583
.L582:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L583:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_102:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_102(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $40, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3638
    leaq g_latencies_ns(%rip), %rax
    movq $9900, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L584
    cvtsi2sdq %rax, %xmm0
    jmp .L585
.L584:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L585:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_103:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_103(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $48, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3639
    leaq g_latencies_ns(%rip), %rax
    movq $9990, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    testq %rax, %rax
    js .L586
    cvtsi2sdq %rax, %xmm0
    jmp .L587
.L586:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L587:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_104:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_104(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $56, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3640
    movq -168(%rbp), %rax
    testq %rax, %rax
    js .L588
    cvtsi2sdq %rax, %xmm0
    jmp .L589
.L588:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L589:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_105:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_105(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $64, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3641
    movq -16(%rbp), %rax
    addq $48, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq -16(%rbp), %rax
    addq $24, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $72, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3642
    movq -144(%rbp), %rax
    pushq %rax
    movq -80(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    testq %rax, %rax
    js .L590
    cvtsi2sdq %rax, %xmm0
    jmp .L591
.L590:
    movq %rax, %rdx
    shrq $1, %rdx
    andl $1, %eax
    orq %rax, %rdx
    cvtsi2sdq %rdx, %xmm0
    addsd %xmm0, %xmm0
.L591:
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_106:
    .quad 4696837146684686336
    .text
    movsd .LS_flit_106(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $80, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3643
    .section .rodata
    .p2align 3
.LS_flit_107:
    .quad 4666723172467343360
    .text
    movsd .LS_flit_107(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    movq -16(%rbp), %rax
    addq $80, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_108:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_108(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $88, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3644
    leaq -216(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_109:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_109(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_110:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_110(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $96, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3645
    leaq -224(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_111:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_111(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_112:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_112(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $104, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3646
    leaq -232(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_113:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_113(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_114:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_114(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $112, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3647
    leaq -240(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_115:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_115(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_116:
    .quad 4652007308841189376
    .text
    movsd .LS_flit_116(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -16(%rbp), %rax
    addq $120, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
.Lfunc_end_533:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .text
write_kernel_profile_json:
    .cfi_startproc
    pushq %rbp
    .cfi_def_cfa_offset 16
    .cfi_offset %rbp, -16
    movq %rsp, %rbp
    .cfi_def_cfa_register %rbp
    subq $336, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    movq %rdx, -24(%rbp)
    movq %rcx, -32(%rbp)
    .loc 1 3650
    .loc 1 3651
    movq -16(%rbp), %rax
    pushq %rax
    leaq .LS100(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_593
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_593
.Lcall_aligned_593:
    call fprintf
.Lcall_end_593:
    cltq
    .loc 1 3652
    movq $10000, %rax
    pushq %rax
    leaq .LS101(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_594
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_594
.Lcall_aligned_594:
    call fprintf
.Lcall_end_594:
    cltq
    .loc 1 3653
    movq -24(%rbp), %rax
    addq $80, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS102(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_595
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_595
.Lcall_aligned_595:
    call fprintf
.Lcall_end_595:
    cltq
    .loc 1 3654
    movq -24(%rbp), %rax
    addq $88, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS103(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_596
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_596
.Lcall_aligned_596:
    call fprintf
.Lcall_end_596:
    cltq
    .loc 1 3655
    movq -24(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS104(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_597
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_597
.Lcall_aligned_597:
    call fprintf
.Lcall_end_597:
    cltq
    .loc 1 3656
    movq -24(%rbp), %rax
    addq $8, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS105(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_598
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_598
.Lcall_aligned_598:
    call fprintf
.Lcall_end_598:
    cltq
    .loc 1 3657
    movq -24(%rbp), %rax
    addq $16, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS106(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_599
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_599
.Lcall_aligned_599:
    call fprintf
.Lcall_end_599:
    cltq
    .loc 1 3658
    movq -24(%rbp), %rax
    addq $24, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS107(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_600
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_600
.Lcall_aligned_600:
    call fprintf
.Lcall_end_600:
    cltq
    .loc 1 3659
    movq -24(%rbp), %rax
    addq $32, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS108(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_601
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_601
.Lcall_aligned_601:
    call fprintf
.Lcall_end_601:
    cltq
    .loc 1 3660
    movq -24(%rbp), %rax
    addq $40, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS109(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_602
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_602
.Lcall_aligned_602:
    call fprintf
.Lcall_end_602:
    cltq
    .loc 1 3661
    movq -24(%rbp), %rax
    addq $48, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS110(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_603
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_603
.Lcall_aligned_603:
    call fprintf
.Lcall_end_603:
    cltq
    .loc 1 3662
    movq -24(%rbp), %rax
    addq $56, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS111(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_604
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_604
.Lcall_aligned_604:
    call fprintf
.Lcall_end_604:
    cltq
    .loc 1 3663
    movq -24(%rbp), %rax
    addq $64, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS112(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_605
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_605
.Lcall_aligned_605:
    call fprintf
.Lcall_end_605:
    cltq
    .loc 1 3664
    movq -24(%rbp), %rax
    addq $72, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS113(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_606
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_606
.Lcall_aligned_606:
    call fprintf
.Lcall_end_606:
    cltq
    .loc 1 3665
    movq -24(%rbp), %rax
    addq $104, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS114(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_607
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_607
.Lcall_aligned_607:
    call fprintf
.Lcall_end_607:
    cltq
    .loc 1 3666
    movslq -32(%rbp), %rax
    testl %eax, %eax
    je .L608
    leaq .LS116(%rip), %rax
    jmp .L609
.L608:
    leaq .LS117(%rip), %rax
.L609:
    pushq %rax
    leaq .LS115(%rip), %rax
    pushq %rax
    movq -8(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_610
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_610
.Lcall_aligned_610:
    call fprintf
.Lcall_end_610:
    cltq
.Lfunc_end_592:
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
    subq $2144, %rsp
    movq %rdi, -8(%rbp)
    movq %rsi, -16(%rbp)
    .loc 1 3669
    .loc 1 3670
    movslq -8(%rbp), %rax
    movq $1, %r11
    cmpl %r11d, %eax
    setg %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L612
    movq -16(%rbp), %rax
    movq $1, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    jmp .L613
.L612:
    leaq .LS118(%rip), %rax
.L613:
    movq %rax, %r11
    movq %r11, -24(%rbp)
    movq %r11, %rax
    .loc 1 3671
    movslq -8(%rbp), %rax
    movq $2, %r11
    cmpl %r11d, %eax
    setg %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L614
    movq -16(%rbp), %rax
    movq $2, %r11
    shlq $3, %r11
    addq %r11, %rax
    movq (%rax), %rax
    jmp .L615
.L614:
    xorl %eax, %eax
    cltq
.L615:
    movq %rax, %r11
    movq %r11, -32(%rbp)
    movq %r11, %rax
    .loc 1 3673
    leaq .LS119(%rip), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_616
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_616
.Lcall_aligned_616:
    call printf
.Lcall_end_616:
    cltq
    .loc 1 3674
    movq -24(%rbp), %rax
    pushq %rax
    leaq .LS120(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_617
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_617
.Lcall_aligned_617:
    call printf
.Lcall_end_617:
    cltq
    .loc 1 3675
    leaq .LS121(%rip), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_618
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_618
.Lcall_aligned_618:
    call printf
.Lcall_end_618:
    cltq
    .loc 1 3676
    leaq .LS122(%rip), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_619
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_619
.Lcall_aligned_619:
    call printf
.Lcall_end_619:
    cltq
    .loc 1 3678
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_620
    subq $8, %rsp
    call init_spatial_grating
    addq $8, %rsp
    jmp .Lcall_end_620
.Lcall_aligned_620:
    call init_spatial_grating
.Lcall_end_620:
    .loc 1 3680
    .loc 1 3681
    .loc 1 3682
    .loc 1 3683
    .loc 1 3684
    .loc 1 3685
    .section .rodata
    .p2align 3
.LS_flit_117:
    .quad 0
    .text
    movsd .LS_flit_117(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1224(%rbp)
    movq %r11, %rax
    .loc 1 3686
    .section .rodata
    .p2align 3
.LS_flit_118:
    .quad 0
    .text
    movsd .LS_flit_118(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1232(%rbp)
    movq %r11, %rax
    .loc 1 3687
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -1240(%rbp)
    movq %r11, %rax
    .loc 1 3689
    .loc 1 3690
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1248(%rbp)
    movq %r11, %rax
.L621:
    movslq -1248(%rbp), %rax
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L622
    .loc 1 3691
    movq $42, %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    cltq
    movq %rax, %r11
    popq %rax
    addq %r11, %rax
    movq %rax, %r11
    movq %r11, -1272(%rbp)
    movq %r11, %rax
    .loc 1 3692
    movq -1272(%rbp), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_624
    subq $8, %rsp
    call generate_u_and_parity_targets
    addq $8, %rsp
    jmp .Lcall_end_624
.Lcall_aligned_624:
    call generate_u_and_parity_targets
.Lcall_end_624:
    .loc 1 3695
    leaq s_test_X_scalar(%rip), %rax
    pushq %rax
    leaq s_train_X_scalar(%rip), %rax
    pushq %rax
    xorl %eax, %eax
    pushq %rax
    movq $25, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_119:
    .quad 4551510721646314285
    .text
    movsd .LS_flit_119(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_120:
    .quad 4636737291354636288
    .text
    movsd .LS_flit_120(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_121:
    .quad 4627730092099895296
    .text
    movsd .LS_flit_121(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_u_seq(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    movl $3, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_625
    subq $8, %rsp
    call run_reservoir_trajectory
    addq $8, %rsp
    jmp .Lcall_end_625
.Lcall_aligned_625:
    call run_reservoir_trajectory
.Lcall_end_625:
    .loc 1 3696
    movslq -1248(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L626
    .loc 1 3697
    leaq s_test_X_fused(%rip), %rax
    pushq %rax
    leaq s_train_X_fused(%rip), %rax
    pushq %rax
    movq $1, %rax
    pushq %rax
    movq $25, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_122:
    .quad 4551510721646314285
    .text
    movsd .LS_flit_122(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_123:
    .quad 4636737291354636288
    .text
    movsd .LS_flit_123(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_124:
    .quad 4627730092099895296
    .text
    movsd .LS_flit_124(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_u_seq(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    movl $3, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_627
    subq $8, %rsp
    call run_reservoir_trajectory
    addq $8, %rsp
    jmp .Lcall_end_627
.Lcall_aligned_627:
    call run_reservoir_trajectory
.Lcall_end_627:
.L626:
    .loc 1 3699
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    pushq %rax
    movq $2, %rax
    pushq %rax
    movq $25, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_125:
    .quad 4551510721646314285
    .text
    movsd .LS_flit_125(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_126:
    .quad 4636737291354636288
    .text
    movsd .LS_flit_126(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_127:
    .quad 4627730092099895296
    .text
    movsd .LS_flit_127(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_u_seq(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    movl $3, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_628
    subq $8, %rsp
    call run_reservoir_trajectory
    addq $8, %rsp
    jmp .Lcall_end_628
.Lcall_aligned_628:
    call run_reservoir_trajectory
.Lcall_end_628:
    .loc 1 3701
    .section .rodata
    .p2align 3
.LS_flit_128:
    .quad 0
    .text
    movsd .LS_flit_128(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1280(%rbp)
    movq %r11, %rax
    .loc 1 3702
    .section .rodata
    .p2align 3
.LS_flit_129:
    .quad 0
    .text
    movsd .LS_flit_129(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1288(%rbp)
    movq %r11, %rax
    .loc 1 3703
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    movq %r11, -1296(%rbp)
    movq %r11, %rax
    .loc 1 3705
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1256(%rbp)
    movq %r11, %rax
.L629:
    movslq -1256(%rbp), %rax
    movq $1000, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L630
    .loc 1 3706
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1264(%rbp)
    movq %r11, %rax
.L632:
    movslq -1264(%rbp), %rax
    movq $512, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L633
    .loc 1 3707
    movslq -1248(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L635
    .loc 1 3708
    leaq s_train_X_scalar(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_train_X_fused(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    pxor %xmm1, %xmm1
    pcmpeqd %xmm1, %xmm1
    psrlq $1, %xmm1
    andpd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1304(%rbp)
    movq %r11, %rax
    .loc 1 3709
    leaq -1304(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1280(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L636
    leaq -1304(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1280(%rbp)
    movq %r11, %rax
.L636:
.L635:
    .loc 1 3711
    leaq s_train_X_scalar(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_train_X_avx(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    pxor %xmm1, %xmm1
    pcmpeqd %xmm1, %xmm1
    psrlq $1, %xmm1
    andpd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1312(%rbp)
    movq %r11, %rax
    .loc 1 3712
    leaq -1312(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1288(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L637
    leaq -1312(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1288(%rbp)
    movq %r11, %rax
.L637:
    .loc 1 3713
    .loc 1 3714
    movq $8, %rax
    pushq %rax
    leaq s_train_X_scalar(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    leaq -1320(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_638
    subq $8, %rsp
    call memcpy
    addq $8, %rsp
    jmp .Lcall_end_638
.Lcall_aligned_638:
    call memcpy
.Lcall_end_638:
    .loc 1 3715
    movq $8, %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    leaq -1328(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_639
    subq $8, %rsp
    call memcpy
    addq $8, %rsp
    jmp .Lcall_end_639
.Lcall_aligned_639:
    call memcpy
.Lcall_end_639:
    .loc 1 3716
    movq -1320(%rbp), %rax
    pushq %rax
    movq -1328(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    seta %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L640
    movq -1320(%rbp), %rax
    pushq %rax
    movq -1328(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    jmp .L641
.L640:
    movq -1328(%rbp), %rax
    pushq %rax
    movq -1320(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
.L641:
    movq %rax, %r11
    movq %r11, -1336(%rbp)
    movq %r11, %rax
    .loc 1 3717
    movq -1336(%rbp), %rax
    pushq %rax
    movq -1296(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    seta %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L642
    movq -1336(%rbp), %rax
    movq %rax, %r11
    movq %r11, -1296(%rbp)
    movq %r11, %rax
.L642:
.L634:
    .loc 1 3706
    leaq -1264(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L632
.L633:
.L631:
    .loc 1 3705
    leaq -1256(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L629
.L630:
    .loc 1 3720
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1256(%rbp)
    movq %r11, %rax
.L643:
    movslq -1256(%rbp), %rax
    movq $400, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L644
    .loc 1 3721
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1264(%rbp)
    movq %r11, %rax
.L646:
    movslq -1264(%rbp), %rax
    movq $512, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L647
    .loc 1 3722
    movslq -1248(%rbp), %rax
    xorq %r11, %r11
    cmpl %r11d, %eax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L649
    .loc 1 3723
    leaq s_test_X_scalar(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_test_X_fused(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    pxor %xmm1, %xmm1
    pcmpeqd %xmm1, %xmm1
    psrlq $1, %xmm1
    andpd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1344(%rbp)
    movq %r11, %rax
    .loc 1 3724
    leaq -1344(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1280(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L650
    leaq -1344(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1280(%rbp)
    movq %r11, %rax
.L650:
.L649:
    .loc 1 3726
    leaq s_test_X_scalar(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    subsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    pxor %xmm1, %xmm1
    pcmpeqd %xmm1, %xmm1
    psrlq $1, %xmm1
    andpd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1352(%rbp)
    movq %r11, %rax
    .loc 1 3727
    leaq -1352(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1288(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L651
    leaq -1352(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1288(%rbp)
    movq %r11, %rax
.L651:
    .loc 1 3728
    .loc 1 3729
    movq $8, %rax
    pushq %rax
    leaq s_test_X_scalar(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    leaq -1360(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_652
    subq $8, %rsp
    call memcpy
    addq $8, %rsp
    jmp .Lcall_end_652
.Lcall_aligned_652:
    call memcpy
.Lcall_end_652:
    .loc 1 3730
    movq $8, %rax
    pushq %rax
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    movslq -1256(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $12, %r11
    addq %r11, %rax
    pushq %rax
    movslq -1264(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    pushq %rax
    leaq -1368(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_653
    subq $8, %rsp
    call memcpy
    addq $8, %rsp
    jmp .Lcall_end_653
.Lcall_aligned_653:
    call memcpy
.Lcall_end_653:
    .loc 1 3731
    movq -1360(%rbp), %rax
    pushq %rax
    movq -1368(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    seta %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L654
    movq -1360(%rbp), %rax
    pushq %rax
    movq -1368(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
    jmp .L655
.L654:
    movq -1368(%rbp), %rax
    pushq %rax
    movq -1360(%rbp), %rax
    movq %rax, %r11
    popq %rax
    subq %r11, %rax
.L655:
    movq %rax, %r11
    movq %r11, -1376(%rbp)
    movq %r11, %rax
    .loc 1 3732
    movq -1376(%rbp), %rax
    pushq %rax
    movq -1296(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    seta %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L656
    movq -1376(%rbp), %rax
    movq %rax, %r11
    movq %r11, -1296(%rbp)
    movq %r11, %rax
.L656:
.L648:
    .loc 1 3721
    leaq -1264(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L646
.L647:
.L645:
    .loc 1 3720
    leaq -1256(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L643
.L644:
    .loc 1 3736
    leaq -1280(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -960(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3737
    leaq -1288(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1088(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3738
    movq -1296(%rbp), %rax
    pushq %rax
    leaq -1216(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3739
    leaq -1280(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1224(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L657
    leaq -1280(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1224(%rbp)
    movq %r11, %rax
.L657:
    .loc 1 3740
    leaq -1288(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1232(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    seta %al
    setnp %r11b
    andb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L658
    leaq -1288(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1232(%rbp)
    movq %r11, %rax
.L658:
    .loc 1 3741
    movq -1296(%rbp), %rax
    pushq %rax
    movq -1240(%rbp), %rax
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    seta %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L659
    movq -1296(%rbp), %rax
    movq %rax, %r11
    movq %r11, -1240(%rbp)
    movq %r11, %rax
.L659:
    .loc 1 3744
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_660
    subq $8, %rsp
    call eval_q1_linear_phase
    addq $8, %rsp
    jmp .Lcall_end_660
.Lcall_aligned_660:
    call eval_q1_linear_phase
.Lcall_end_660:
    movq %xmm0, %rax
    pushq %rax
    leaq -192(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3745
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_661
    subq $8, %rsp
    call eval_q2_local_intensity
    addq $8, %rsp
    jmp .Lcall_end_661
.Lcall_aligned_661:
    call eval_q2_local_intensity
.Lcall_end_661:
    movq %xmm0, %rax
    pushq %rax
    leaq -320(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3746
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_662
    subq $8, %rsp
    call eval_q4_all_quadratic
    addq $8, %rsp
    jmp .Lcall_end_662
.Lcall_aligned_662:
    call eval_q4_all_quadratic
.Lcall_end_662:
    movq %xmm0, %rax
    pushq %rax
    leaq -448(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3747
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_663
    subq $8, %rsp
    call eval_q5_full_candidate
    addq $8, %rsp
    jmp .Lcall_end_663
.Lcall_aligned_663:
    call eval_q5_full_candidate
.Lcall_end_663:
    movq %xmm0, %rax
    pushq %rax
    leaq -576(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3748
    leaq s_u_seq(%rip), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_664
    subq $8, %rsp
    call eval_b0_linear_delay
    addq $8, %rsp
    jmp .Lcall_end_664
.Lcall_aligned_664:
    call eval_b0_linear_delay
.Lcall_end_664:
    movq %xmm0, %rax
    pushq %rax
    leaq -704(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3749
    leaq s_u_seq(%rip), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_665
    subq $8, %rsp
    call eval_v1_volterra
    addq $8, %rsp
    jmp .Lcall_end_665
.Lcall_aligned_665:
    call eval_v1_volterra
.Lcall_end_665:
    movq %xmm0, %rax
    pushq %rax
    leaq -832(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    popq %r11
    movq %r11, (%rax)
    movq %r11, %rax
    .loc 1 3751
    .loc 1 3753
    movq -1296(%rbp), %rax
    pushq %rax
    .loc 1 3752
    leaq -832(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -576(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -448(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -320(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -192(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -1272(%rbp), %rax
    pushq %rax
    .loc 1 3751
    leaq .LS123(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rax
    movq %rax, %xmm4
    popq %rdx
    movl $5, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_666
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_666
.Lcall_aligned_666:
    call printf
.Lcall_end_666:
    cltq
.L623:
    .loc 1 3690
    leaq -1248(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L621
.L622:
    .loc 1 3756
    leaq -1224(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_130:
    .quad 0
    .text
    movsd .LS_flit_130(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    setne %al
    setp %r11b
    orb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L670
    leaq -1232(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    .section .rodata
    .p2align 3
.LS_flit_131:
    .quad 0
    .text
    movsd .LS_flit_131(%rip), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    ucomisd %xmm0, %xmm1
    setne %al
    setp %r11b
    orb %r11b, %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L670
    xorq %rax, %rax
    jmp .L671
.L670:
    movq $1, %rax
.L671:
    testl %eax, %eax
    jne .L668
    movq -1240(%rbp), %rax
    pushq %rax
    xorl %eax, %eax
    cltq
    movq %rax, %r11
    popq %rax
    cmpq %r11, %rax
    setne %al
    movzbl %al, %eax
    testl %eax, %eax
    jne .L668
    xorq %rax, %rax
    jmp .L669
.L668:
    movq $1, %rax
.L669:
    testl %eax, %eax
    je .L667
    .loc 1 3757
    .loc 1 3758
    movq -1240(%rbp), %rax
    pushq %rax
    leaq -1232(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1224(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .loc 1 3757
    leaq .LS124(%rip), %rax
    pushq %rax
    leaq stderr(%rip), %rax
    movq (%rax), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rdx
    movl $2, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_672
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_672
.Lcall_aligned_672:
    call fprintf
.Lcall_end_672:
    cltq
    .loc 1 3759
    movq $1, %rax
    jmp .Lfunc_end_611
.L667:
    .loc 1 3763
    movq $42, %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_673
    subq $8, %rsp
    call generate_u_and_parity_targets
    addq $8, %rsp
    jmp .Lcall_end_673
.Lcall_aligned_673:
    call generate_u_and_parity_targets
.Lcall_end_673:
    .loc 1 3764
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    pushq %rax
    movq $2, %rax
    pushq %rax
    movq $25, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_132:
    .quad 4551510721646314285
    .text
    movsd .LS_flit_132(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_133:
    .quad 4636737291354636288
    .text
    movsd .LS_flit_133(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_134:
    .quad 0
    .text
    movsd .LS_flit_134(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_u_seq(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    movl $3, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_674
    subq $8, %rsp
    call run_reservoir_trajectory
    addq $8, %rsp
    jmp .Lcall_end_674
.Lcall_aligned_674:
    call run_reservoir_trajectory
.Lcall_end_674:
    .loc 1 3765
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_675
    subq $8, %rsp
    call eval_q5_full_candidate
    addq $8, %rsp
    jmp .Lcall_end_675
.Lcall_aligned_675:
    call eval_q5_full_candidate
.Lcall_end_675:
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1384(%rbp)
    movq %r11, %rax
    .loc 1 3767
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    pushq %rax
    movq $2, %rax
    pushq %rax
    movq $25, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_135:
    .quad 4551510721646314285
    .text
    movsd .LS_flit_135(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_136:
    .quad 0
    .text
    movsd .LS_flit_136(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .section .rodata
    .p2align 3
.LS_flit_137:
    .quad 4627730092099895296
    .text
    movsd .LS_flit_137(%rip), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq s_u_seq(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rsi
    popq %rdx
    popq %rcx
    popq %r8
    movl $3, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_676
    subq $8, %rsp
    call run_reservoir_trajectory
    addq $8, %rsp
    jmp .Lcall_end_676
.Lcall_aligned_676:
    call run_reservoir_trajectory
.Lcall_end_676:
    .loc 1 3768
    leaq s_test_X_avx(%rip), %rax
    pushq %rax
    leaq s_train_X_avx(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_677
    subq $8, %rsp
    call eval_q5_full_candidate
    addq $8, %rsp
    jmp .Lcall_end_677
.Lcall_aligned_677:
    call eval_q5_full_candidate
.Lcall_end_677:
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1392(%rbp)
    movq %r11, %rax
    .loc 1 3770
    leaq .LS125(%rip), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_678
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_678
.Lcall_aligned_678:
    call printf
.Lcall_end_678:
    cltq
    .loc 1 3771
    leaq -1384(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS126(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_679
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_679
.Lcall_aligned_679:
    call printf
.Lcall_end_679:
    cltq
    .loc 1 3772
    leaq -1392(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS127(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_680
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_680
.Lcall_aligned_680:
    call printf
.Lcall_end_680:
    cltq
    .loc 1 3775
    movq -24(%rbp), %rax
    pushq %rax
    leaq .LS128(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_681
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_681
.Lcall_aligned_681:
    call printf
.Lcall_end_681:
    cltq
    .loc 1 3776
    .loc 1 3777
    leaq -1536(%rbp), %rax
    pushq %rax
    xorl %eax, %eax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_682
    subq $8, %rsp
    call profile_kernel_10k
    addq $8, %rsp
    jmp .Lcall_end_682
.Lcall_aligned_682:
    call profile_kernel_10k
.Lcall_end_682:
    .loc 1 3778
    leaq -1664(%rbp), %rax
    pushq %rax
    movq $1, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_683
    subq $8, %rsp
    call profile_kernel_10k
    addq $8, %rsp
    jmp .Lcall_end_683
.Lcall_aligned_683:
    call profile_kernel_10k
.Lcall_end_683:
    .loc 1 3779
    leaq -1792(%rbp), %rax
    pushq %rax
    movq $2, %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_684
    subq $8, %rsp
    call profile_kernel_10k
    addq $8, %rsp
    jmp .Lcall_end_684
.Lcall_aligned_684:
    call profile_kernel_10k
.Lcall_end_684:
    .loc 1 3781
    leaq -1536(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1664(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1800(%rbp)
    movq %r11, %rax
    .loc 1 3782
    leaq -1536(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1792(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1808(%rbp)
    movq %r11, %rax
    .loc 1 3783
    leaq -1512(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1768(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1816(%rbp)
    movq %r11, %rax
    .loc 1 3784
    leaq -1432(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    subq $8, %rsp
    movsd %xmm0, (%rsp)
    leaq -1688(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    movq %rax, %xmm0
    movsd (%rsp), %xmm1
    addq $8, %rsp
    divsd %xmm0, %xmm1
    movsd %xmm1, %xmm0
    movq %xmm0, %rax
    movq %rax, %r11
    movq %r11, -1824(%rbp)
    movq %r11, %rax
    .loc 1 3786
    .loc 1 3787
    leaq -1448(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1432(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1472(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1488(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1512(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1536(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .loc 1 3786
    leaq .LS129(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rax
    movq %rax, %xmm4
    popq %rax
    movq %rax, %xmm5
    movl $6, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_685
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_685
.Lcall_aligned_685:
    call printf
.Lcall_end_685:
    cltq
    .loc 1 3788
    .loc 1 3789
    leaq -1800(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1576(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1560(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1600(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1616(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1640(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1664(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .loc 1 3788
    leaq .LS130(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rax
    movq %rax, %xmm4
    popq %rax
    movq %rax, %xmm5
    popq %rax
    movq %rax, %xmm6
    movl $7, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_686
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_686
.Lcall_aligned_686:
    call printf
.Lcall_end_686:
    cltq
    .loc 1 3790
    .loc 1 3791
    leaq -1824(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1808(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1704(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1688(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1728(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1744(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1768(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq -1792(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    .loc 1 3790
    leaq .LS131(%rip), %rax
    movq %rax, %rdi
    popq %rax
    movq %rax, %xmm0
    popq %rax
    movq %rax, %xmm1
    popq %rax
    movq %rax, %xmm2
    popq %rax
    movq %rax, %xmm3
    popq %rax
    movq %rax, %xmm4
    popq %rax
    movq %rax, %xmm5
    popq %rax
    movq %rax, %xmm6
    popq %rax
    movq %rax, %xmm7
    movl $8, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_687
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_687
.Lcall_aligned_687:
    call printf
.Lcall_end_687:
    cltq
    .loc 1 3793
    movq -32(%rbp), %rax
    testq %rax, %rax
    je .L688
    .loc 1 3794
    leaq .LS132(%rip), %rax
    pushq %rax
    movq -32(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_689
    subq $8, %rsp
    call fopen
    addq $8, %rsp
    jmp .Lcall_end_689
.Lcall_aligned_689:
    call fopen
.Lcall_end_689:
    movq %rax, %r11
    movq %r11, -1832(%rbp)
    movq %r11, %rax
    .loc 1 3795
    movq -1832(%rbp), %rax
    testq %rax, %rax
    sete %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L690
    .loc 1 3796
    movq -32(%rbp), %rax
    pushq %rax
    leaq .LS133(%rip), %rax
    pushq %rax
    leaq stderr(%rip), %rax
    movq (%rax), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_691
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_691
.Lcall_aligned_691:
    call fprintf
.Lcall_end_691:
    cltq
    .loc 1 3797
    movq $1, %rax
    jmp .Lfunc_end_611
.L690:
    .loc 1 3799
    leaq .LS134(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_692
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_692
.Lcall_aligned_692:
    call fprintf
.Lcall_end_692:
    cltq
    .loc 1 3800
    movq -24(%rbp), %rax
    pushq %rax
    leaq .LS135(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_693
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_693
.Lcall_aligned_693:
    call fprintf
.Lcall_end_693:
    cltq
    .loc 1 3801
    leaq .LS136(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_694
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_694
.Lcall_aligned_694:
    call fprintf
.Lcall_end_694:
    cltq
    .loc 1 3802
    leaq .LS137(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_695
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_695
.Lcall_aligned_695:
    call fprintf
.Lcall_end_695:
    cltq
    .loc 1 3803
    leaq .LS138(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_696
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_696
.Lcall_aligned_696:
    call fprintf
.Lcall_end_696:
    cltq
    .loc 1 3804
    movq $10, %rax
    pushq %rax
    leaq .LS139(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_697
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_697
.Lcall_aligned_697:
    call fprintf
.Lcall_end_697:
    cltq
    .loc 1 3805
    movq $1000, %rax
    pushq %rax
    leaq .LS140(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_698
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_698
.Lcall_aligned_698:
    call fprintf
.Lcall_end_698:
    cltq
    .loc 1 3806
    movq $400, %rax
    pushq %rax
    leaq .LS141(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_699
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_699
.Lcall_aligned_699:
    call fprintf
.Lcall_end_699:
    cltq
    .loc 1 3807
    movq $512, %rax
    pushq %rax
    leaq .LS142(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_700
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_700
.Lcall_aligned_700:
    call fprintf
.Lcall_end_700:
    cltq
    .loc 1 3808
    leaq -1224(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS143(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_701
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_701
.Lcall_aligned_701:
    call fprintf
.Lcall_end_701:
    cltq
    .loc 1 3809
    leaq -1232(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS144(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_702
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_702
.Lcall_aligned_702:
    call fprintf
.Lcall_end_702:
    cltq
    .loc 1 3810
    movq -1240(%rbp), %rax
    pushq %rax
    leaq .LS145(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_703
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_703
.Lcall_aligned_703:
    call fprintf
.Lcall_end_703:
    cltq
    .loc 1 3811
    leaq .LS146(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_704
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_704
.Lcall_aligned_704:
    call fprintf
.Lcall_end_704:
    cltq
    .loc 1 3812
    leaq .LS147(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_705
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_705
.Lcall_aligned_705:
    call fprintf
.Lcall_end_705:
    cltq
    .loc 1 3814
    leaq .LS148(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_706
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_706
.Lcall_aligned_706:
    call fprintf
.Lcall_end_706:
    cltq
    .loc 1 3815
    leaq .LS149(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_707
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_707
.Lcall_aligned_707:
    call fprintf
.Lcall_end_707:
    cltq
    .loc 1 3816
    leaq .LS150(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_708
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_708
.Lcall_aligned_708:
    call fprintf
.Lcall_end_708:
    cltq
    .loc 1 3817
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1248(%rbp)
    movq %r11, %rax
.L709:
    movslq -1248(%rbp), %rax
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L710
    movslq -1248(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L712
    leaq .LS152(%rip), %rax
    jmp .L713
.L712:
    leaq .LS153(%rip), %rax
.L713:
    pushq %rax
    leaq -192(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS151(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rdx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_714
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_714
.Lcall_aligned_714:
    call fprintf
.Lcall_end_714:
    cltq
.L711:
    leaq -1248(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L709
.L710:
    .loc 1 3818
    leaq .LS154(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_715
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_715
.Lcall_aligned_715:
    call fprintf
.Lcall_end_715:
    cltq
    .loc 1 3819
    leaq .LS155(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_716
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_716
.Lcall_aligned_716:
    call fprintf
.Lcall_end_716:
    cltq
    .loc 1 3820
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1248(%rbp)
    movq %r11, %rax
.L717:
    movslq -1248(%rbp), %rax
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L718
    movslq -1248(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L720
    leaq .LS157(%rip), %rax
    jmp .L721
.L720:
    leaq .LS158(%rip), %rax
.L721:
    pushq %rax
    leaq -320(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS156(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rdx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_722
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_722
.Lcall_aligned_722:
    call fprintf
.Lcall_end_722:
    cltq
.L719:
    leaq -1248(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L717
.L718:
    .loc 1 3821
    leaq .LS159(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_723
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_723
.Lcall_aligned_723:
    call fprintf
.Lcall_end_723:
    cltq
    .loc 1 3822
    leaq .LS160(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_724
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_724
.Lcall_aligned_724:
    call fprintf
.Lcall_end_724:
    cltq
    .loc 1 3823
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1248(%rbp)
    movq %r11, %rax
.L725:
    movslq -1248(%rbp), %rax
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L726
    movslq -1248(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L728
    leaq .LS162(%rip), %rax
    jmp .L729
.L728:
    leaq .LS163(%rip), %rax
.L729:
    pushq %rax
    leaq -448(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS161(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rdx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_730
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_730
.Lcall_aligned_730:
    call fprintf
.Lcall_end_730:
    cltq
.L727:
    leaq -1248(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L725
.L726:
    .loc 1 3824
    leaq .LS164(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_731
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_731
.Lcall_aligned_731:
    call fprintf
.Lcall_end_731:
    cltq
    .loc 1 3825
    leaq .LS165(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_732
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_732
.Lcall_aligned_732:
    call fprintf
.Lcall_end_732:
    cltq
    .loc 1 3826
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1248(%rbp)
    movq %r11, %rax
.L733:
    movslq -1248(%rbp), %rax
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L734
    movslq -1248(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L736
    leaq .LS167(%rip), %rax
    jmp .L737
.L736:
    leaq .LS168(%rip), %rax
.L737:
    pushq %rax
    leaq -576(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS166(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rdx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_738
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_738
.Lcall_aligned_738:
    call fprintf
.Lcall_end_738:
    cltq
.L735:
    leaq -1248(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L733
.L734:
    .loc 1 3827
    leaq .LS169(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_739
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_739
.Lcall_aligned_739:
    call fprintf
.Lcall_end_739:
    cltq
    .loc 1 3828
    leaq .LS170(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_740
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_740
.Lcall_aligned_740:
    call fprintf
.Lcall_end_740:
    cltq
    .loc 1 3829
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1248(%rbp)
    movq %r11, %rax
.L741:
    movslq -1248(%rbp), %rax
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L742
    movslq -1248(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L744
    leaq .LS172(%rip), %rax
    jmp .L745
.L744:
    leaq .LS173(%rip), %rax
.L745:
    pushq %rax
    leaq -704(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS171(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rdx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_746
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_746
.Lcall_aligned_746:
    call fprintf
.Lcall_end_746:
    cltq
.L743:
    leaq -1248(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L741
.L742:
    .loc 1 3830
    leaq .LS174(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_747
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_747
.Lcall_aligned_747:
    call fprintf
.Lcall_end_747:
    cltq
    .loc 1 3831
    leaq .LS175(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_748
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_748
.Lcall_aligned_748:
    call fprintf
.Lcall_end_748:
    cltq
    .loc 1 3832
    xorl %eax, %eax
    movq %rax, %r11
    movl %r11d, -1248(%rbp)
    movq %r11, %rax
.L749:
    movslq -1248(%rbp), %rax
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L750
    movslq -1248(%rbp), %rax
    movq $1, %r11
    addq %r11, %rax
    cltq
    movq $10, %r11
    cmpl %r11d, %eax
    setl %al
    movzbl %al, %eax
    testl %eax, %eax
    je .L752
    leaq .LS177(%rip), %rax
    jmp .L753
.L752:
    leaq .LS178(%rip), %rax
.L753:
    pushq %rax
    leaq -832(%rbp), %rax
    pushq %rax
    movslq -1248(%rbp), %rax
    movq %rax, %r11
    popq %rax
    shlq $3, %r11
    addq %r11, %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS176(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    popq %rdx
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_754
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_754
.Lcall_aligned_754:
    call fprintf
.Lcall_end_754:
    cltq
.L751:
    leaq -1248(%rbp), %rax
    pushq %rax
    movslq (%rax), %rax
    pushq %rax
    addq $1, %rax
    movq %rax, %r11
    popq %rdx
    popq %rax
    movl %r11d, (%rax)
    movq %rdx, %rax
    jmp .L749
.L750:
    .loc 1 3833
    leaq .LS179(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_755
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_755
.Lcall_aligned_755:
    call fprintf
.Lcall_end_755:
    cltq
    .loc 1 3834
    leaq .LS180(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_756
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_756
.Lcall_aligned_756:
    call fprintf
.Lcall_end_756:
    cltq
    .loc 1 3836
    leaq .LS181(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_757
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_757
.Lcall_aligned_757:
    call fprintf
.Lcall_end_757:
    cltq
    .loc 1 3837
    leaq -1384(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS182(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_758
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_758
.Lcall_aligned_758:
    call fprintf
.Lcall_end_758:
    cltq
    .loc 1 3838
    leaq -1392(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS183(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_759
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_759
.Lcall_aligned_759:
    call fprintf
.Lcall_end_759:
    cltq
    .loc 1 3839
    leaq .LS184(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_760
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_760
.Lcall_aligned_760:
    call fprintf
.Lcall_end_760:
    cltq
    .loc 1 3841
    leaq .LS185(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_761
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_761
.Lcall_aligned_761:
    call fprintf
.Lcall_end_761:
    cltq
    .loc 1 3842
    movq $1, %rax
    pushq %rax
    leaq -1536(%rbp), %rax
    pushq %rax
    leaq .LS186(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_762
    subq $8, %rsp
    call write_kernel_profile_json
    addq $8, %rsp
    jmp .Lcall_end_762
.Lcall_aligned_762:
    call write_kernel_profile_json
.Lcall_end_762:
    .loc 1 3843
    movq $1, %rax
    pushq %rax
    leaq -1664(%rbp), %rax
    pushq %rax
    leaq .LS187(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_763
    subq $8, %rsp
    call write_kernel_profile_json
    addq $8, %rsp
    jmp .Lcall_end_763
.Lcall_aligned_763:
    call write_kernel_profile_json
.Lcall_end_763:
    .loc 1 3844
    xorl %eax, %eax
    pushq %rax
    leaq -1792(%rbp), %rax
    pushq %rax
    leaq .LS188(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_764
    subq $8, %rsp
    call write_kernel_profile_json
    addq $8, %rsp
    jmp .Lcall_end_764
.Lcall_aligned_764:
    call write_kernel_profile_json
.Lcall_end_764:
    .loc 1 3845
    leaq .LS189(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_765
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_765
.Lcall_aligned_765:
    call fprintf
.Lcall_end_765:
    cltq
    .loc 1 3847
    leaq .LS190(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_766
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_766
.Lcall_aligned_766:
    call fprintf
.Lcall_end_766:
    cltq
    .loc 1 3848
    leaq -1800(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS191(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_767
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_767
.Lcall_aligned_767:
    call fprintf
.Lcall_end_767:
    cltq
    .loc 1 3849
    leaq -1808(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS192(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_768
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_768
.Lcall_aligned_768:
    call fprintf
.Lcall_end_768:
    cltq
    .loc 1 3850
    leaq -1816(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS193(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_769
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_769
.Lcall_aligned_769:
    call fprintf
.Lcall_end_769:
    cltq
    .loc 1 3851
    leaq -1824(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    leaq .LS194(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_770
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_770
.Lcall_aligned_770:
    call fprintf
.Lcall_end_770:
    cltq
    .loc 1 3852
    leaq .LS195(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_771
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_771
.Lcall_aligned_771:
    call fprintf
.Lcall_end_771:
    cltq
    .loc 1 3853
    leaq .LS196(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_772
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_772
.Lcall_aligned_772:
    call fprintf
.Lcall_end_772:
    cltq
    .loc 1 3854
    leaq .LS197(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_773
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_773
.Lcall_aligned_773:
    call fprintf
.Lcall_end_773:
    cltq
    .loc 1 3855
    leaq .LS198(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_774
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_774
.Lcall_aligned_774:
    call fprintf
.Lcall_end_774:
    cltq
    .loc 1 3856
    leaq .LS199(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_775
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_775
.Lcall_aligned_775:
    call fprintf
.Lcall_end_775:
    cltq
    .loc 1 3857
    leaq .LS200(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_776
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_776
.Lcall_aligned_776:
    call fprintf
.Lcall_end_776:
    cltq
    .loc 1 3858
    leaq .LS201(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_777
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_777
.Lcall_aligned_777:
    call fprintf
.Lcall_end_777:
    cltq
    .loc 1 3859
    leaq .LS202(%rip), %rax
    pushq %rax
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    popq %rsi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_778
    subq $8, %rsp
    call fprintf
    addq $8, %rsp
    jmp .Lcall_end_778
.Lcall_aligned_778:
    call fprintf
.Lcall_end_778:
    cltq
    .loc 1 3860
    movq -1832(%rbp), %rax
    movq %rax, %rdi
    xorl %eax, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_779
    subq $8, %rsp
    call fclose
    addq $8, %rsp
    jmp .Lcall_end_779
.Lcall_aligned_779:
    call fclose
.Lcall_end_779:
    cltq
.L688:
    .loc 1 3863
    .loc 1 3864
    leaq -1824(%rbp), %rax
    movsd (%rax), %xmm0
    movq %xmm0, %rax
    pushq %rax
    movq -24(%rbp), %rax
    pushq %rax
    .loc 1 3863
    leaq .LS203(%rip), %rax
    movq %rax, %rdi
    popq %rsi
    popq %rax
    movq %rax, %xmm0
    movl $1, %eax
    movq %rsp, %r11
    andq $15, %r11
    testq %r11, %r11
    je .Lcall_aligned_780
    subq $8, %rsp
    call printf
    addq $8, %rsp
    jmp .Lcall_end_780
.Lcall_aligned_780:
    call printf
.Lcall_end_780:
    cltq
    .loc 1 3865
    xorl %eax, %eax
    jmp .Lfunc_end_611
    xorl %eax, %eax
.Lfunc_end_611:
    movq %rbp, %rsp
    popq %rbp
    .cfi_def_cfa %rsp, 8
    ret
    .cfi_endproc
    .local crc32_context
    .comm crc32_context, 4, 8
    .comm g_psi_r, 1024, 8
    .comm g_psi_i, 1024, 8
    .comm g_H_psi_r, 1024, 8
    .comm g_H_psi_i, 1024, 8
    .comm g_V, 1024, 8
    .comm g_spatial_grating, 1024, 8
    .comm g_halo, 1040, 8
    .data
    .p2align 3
    .globl g_avx_two
g_avx_two:
    .quad 4611686018427387904
    .data
    .p2align 3
    .globl g_avx_neg_J
g_avx_neg_J:
    .quad -4595641944754880512
    .data
    .p2align 3
    .globl g_avx_half_dt
g_avx_half_dt:
    .quad 4547007122018943789
    .data
    .p2align 3
    .globl g_avx_dt
g_avx_dt:
    .quad 4551510721646314285
    .data
    .p2align 2
    .globl g_avx_substeps
g_avx_substeps:
    .long 25
    .local g_features
    .comm g_features, 4096, 8
    .local g_pair_a
    .comm g_pair_a, 512, 8
    .local g_pair_b
    .comm g_pair_b, 512, 8
    .local s_u_seq
    .comm s_u_seq, 12000, 8
    .local s_y_train
    .comm s_y_train, 8000, 8
    .local s_y_test
    .comm s_y_test, 3200, 8
    .local s_train_X_scalar
    .comm s_train_X_scalar, 4096000, 8
    .local s_test_X_scalar
    .comm s_test_X_scalar, 1638400, 8
    .local s_train_X_fused
    .comm s_train_X_fused, 4096000, 8
    .local s_test_X_fused
    .comm s_test_X_fused, 1638400, 8
    .local s_train_X_avx
    .comm s_train_X_avx, 4096000, 8
    .local s_test_X_avx
    .comm s_test_X_avx, 1638400, 8
    .local s_sub_train_X
    .comm s_sub_train_X, 4096000, 8
    .local s_sub_test_X
    .comm s_sub_test_X, 1638400, 8
    .local s_matA
    .comm s_matA, 2105352, 8
    .local s_matA_orig
    .comm s_matA_orig, 2105352, 8
    .local s_vecB
    .comm s_vecB, 4104, 8
    .local s_cholL
    .comm s_cholL, 2105352, 8
    .local s_vecY
    .comm s_vecY, 4104, 8
    .local s_weights
    .comm s_weights, 4104, 8
    .local g_latencies_ns
    .comm g_latencies_ns, 80000, 8
    .local g_phase_p1_ns
    .comm g_phase_p1_ns, 8000, 8
    .local g_phase_p2_ns
    .comm g_phase_p2_ns, 8000, 8
    .local g_phase_p3_ns
    .comm g_phase_p3_ns, 8000, 8
    .local g_phase_p4_ns
    .comm g_phase_p4_ns, 8000, 8
    .data
    .p2align 3
g_rng_state:
    .quad 42
    .data
    .p2align 3
g_avx512_dispatch:
    .quad ql_step_verlet_avx512_impl
    .section .rodata
.LS100:
.Lstr_0:
    .asciz "    \"%s\": {\n"
.LS101:
.Lstr_1:
    .asciz "      \"benchmark_steps\": %d,\n"
.LS102:
.Lstr_2:
    .asciz "      \"total_wall_ms\": %.4f,\n"
.LS103:
.Lstr_3:
    .asciz "      \"throughput_steps_per_sec\": %.2f,\n"
.LS104:
.Lstr_4:
    .asciz "      \"mean_us\": %.4f,\n"
.LS105:
.Lstr_5:
    .asciz "      \"std_us\": %.4f,\n"
.LS106:
.Lstr_6:
    .asciz "      \"min_us\": %.4f,\n"
.LS107:
.Lstr_7:
    .asciz "      \"p50_us\": %.4f,\n"
.LS108:
.Lstr_8:
    .asciz "      \"p90_us\": %.4f,\n"
.LS109:
.Lstr_9:
    .asciz "      \"p95_us\": %.4f,\n"
.LS110:
.Lstr_10:
    .asciz "      \"p99_us\": %.4f,\n"
.LS111:
.Lstr_11:
    .asciz "      \"p999_us\": %.4f,\n"
.LS112:
.Lstr_12:
    .asciz "      \"wcet_us\": %.4f,\n"
.LS113:
.Lstr_13:
    .asciz "      \"jitter_us\": %.4f,\n"
.LS114:
.Lstr_14:
    .asciz "      \"phase_2_propagation_us\": %.4f\n"
.LS115:
.Lstr_15:
    .asciz "    }%s\n"
.LS116:
.Lstr_16:
    .asciz ","
.LS117:
.Lstr_17:
    .asciz ""
.LS118:
.Lstr_18:
    .asciz "ZCC"
.LS119:
.Lstr_19:
    .asciz "====================================================================================================\n"
.LS120:
.Lstr_20:
    .asciz "  ZCC-EAI-006-G3-M5: NATIVE 10-SEED QUANTUM RESERVOIR & AVX-512 ENGINE (%s)\n"
.LS121:
.Lstr_21:
    .asciz "  Vectors: [1] Native Multi-Seed Generalization (42..51) | [3] AVX-512 Symplectic Verlet Kernel\n"
.LS122:
.Lstr_22:
    .asciz "====================================================================================================\n"
.LS123:
.Lstr_23:
    .asciz "  [Seed %llu] Q1=%+.6f | Q2=%+.6f | Q4=%+.6f | Q5=%+.6f | V1=%+.6f | AVX512_ULP_Diff=%llu\n"
.LS124:
.Lstr_24:
    .asciz "[FATAL][NV-1] Kernel bit-exactness violated: fused=%.17e, avx=%.17e, ulp=%llu\n"
.LS125:
.Lstr_25:
    .asciz "\n[PHYSICAL MECHANISM ABLATION (NATIVE C + AVX-512, SEED 42)]\n"
.LS126:
.Lstr_26:
    .asciz "  * Decoupled Lattice (J=0, V=100): Held-out R^2 = %+.6f\n"
.LS127:
.Lstr_27:
    .asciz "  * Free Wave (J=25, V=0):          Held-out R^2 = %+.6f\n"
.LS128:
.Lstr_28:
    .asciz "\n[10,000-STEP SYMPLECTIC VERLET KERNEL BENCHMARK (%s)]\n"
.LS129:
.Lstr_29:
    .asciz "  * Kernel 0 (Scalar Ref):   Mean=%7.2f us | p50=%7.2f us | p99=%7.2f us | WCET=%7.2f us | Prop(P2)=%7.2f us | %8.1f steps/s\n"
.LS130:
.Lstr_30:
    .asciz "  * Kernel 1 (Fused C):      Mean=%7.2f us | p50=%7.2f us | p99=%7.2f us | WCET=%7.2f us | Prop(P2)=%7.2f us | %8.1f steps/s (%.2fx)\n"
.LS131:
.Lstr_31:
    .asciz "  * Kernel 2 (AVX-512 zmm):  Mean=%7.2f us | p50=%7.2f us | p99=%7.2f us | WCET=%7.2f us | Prop(P2)=%7.2f us | %8.1f steps/s (%.2fx mean, %.2fx P2)\n"
.LS132:
.Lstr_32:
    .asciz "w"
.LS133:
.Lstr_33:
    .asciz "[FATAL] Cannot open output JSON %s\n"
.LS134:
.Lstr_34:
    .asciz "{\n"
.LS135:
.Lstr_35:
    .asciz "  \"toolchain\": \"%s\",\n"
.LS136:
.Lstr_36:
    .asciz "  \"governing_standards\": [\"ZCC-EF-0\", \"ZCC-EF-7\", \"NV-1..NV-5\", \"AV-1..AV-6\"],\n"
.LS137:
.Lstr_37:
    .asciz "  \"equivalence_verification\": {\n"
.LS138:
.Lstr_38:
    .asciz "    \"closed_form_expression\": \"out_j = rn(out_j +/- rn(c_dt * rn(rn((-J) * rn(rn(in_{j-1} + in_{j+1}) - rn(2.0 * in_j))) + rn(V_j * in_j))))\",\n"
.LS139:
.Lstr_39:
    .asciz "    \"seeds_verified\": %d,\n"
.LS140:
.Lstr_40:
    .asciz "    \"rows_per_seed_train\": %d,\n"
.LS141:
.Lstr_41:
    .asciz "    \"rows_per_seed_test\": %d,\n"
.LS142:
.Lstr_42:
    .asciz "    \"features_per_row\": %d,\n"
.LS143:
.Lstr_43:
    .asciz "    \"scalar_vs_fused_max_abs_diff\": %.17e,\n"
.LS144:
.Lstr_44:
    .asciz "    \"scalar_vs_avx512_max_abs_diff\": %.17e,\n"
.LS145:
.Lstr_45:
    .asciz "    \"scalar_vs_avx512_max_ulp_diff\": %llu,\n"
.LS146:
.Lstr_46:
    .asciz "    \"status\": \"PASS\"\n"
.LS147:
.Lstr_47:
    .asciz "  },\n"
.LS148:
.Lstr_48:
    .asciz "  \"native_10seed_r2\": {\n"
.LS149:
.Lstr_49:
    .asciz "    \"seeds\": [42, 43, 44, 45, 46, 47, 48, 49, 50, 51],\n"
.LS150:
.Lstr_50:
    .asciz "    \"Q1_Quantum_LinearPhase\": ["
.LS151:
.Lstr_51:
    .asciz "%.17e%s"
.LS152:
.Lstr_52:
    .asciz ", "
.LS153:
.Lstr_53:
    .asciz ""
.LS154:
.Lstr_54:
    .asciz "],\n"
.LS155:
.Lstr_55:
    .asciz "    \"Q2_Quantum_LocalIntensity\": ["
.LS156:
.Lstr_56:
    .asciz "%.17e%s"
.LS157:
.Lstr_57:
    .asciz ", "
.LS158:
.Lstr_58:
    .asciz ""
.LS159:
.Lstr_59:
    .asciz "],\n"
.LS160:
.Lstr_60:
    .asciz "    \"Q4_Quantum_AllQuadratic\": ["
.LS161:
.Lstr_61:
    .asciz "%.17e%s"
.LS162:
.Lstr_62:
    .asciz ", "
.LS163:
.Lstr_63:
    .asciz ""
.LS164:
.Lstr_64:
    .asciz "],\n"
.LS165:
.Lstr_65:
    .asciz "    \"Q5_Quantum_FullCandidate\": ["
.LS166:
.Lstr_66:
    .asciz "%.17e%s"
.LS167:
.Lstr_67:
    .asciz ", "
.LS168:
.Lstr_68:
    .asciz ""
.LS169:
.Lstr_69:
    .asciz "],\n"
.LS170:
.Lstr_70:
    .asciz "    \"B0_Linear_Delay\": ["
.LS171:
.Lstr_71:
    .asciz "%.17e%s"
.LS172:
.Lstr_72:
    .asciz ", "
.LS173:
.Lstr_73:
    .asciz ""
.LS174:
.Lstr_74:
    .asciz "],\n"
.LS175:
.Lstr_75:
    .asciz "    \"V1_Volterra_Filter\": ["
.LS176:
.Lstr_76:
    .asciz "%.17e%s"
.LS177:
.Lstr_77:
    .asciz ", "
.LS178:
.Lstr_78:
    .asciz ""
.LS179:
.Lstr_79:
    .asciz "]\n"
.LS180:
.Lstr_80:
    .asciz "  },\n"
.LS181:
.Lstr_81:
    .asciz "  \"native_ablation_seed42\": {\n"
.LS182:
.Lstr_82:
    .asciz "    \"decoupled_J0_V100_r2\": %.17e,\n"
.LS183:
.Lstr_83:
    .asciz "    \"free_wave_J25_V0_r2\": %.17e\n"
.LS184:
.Lstr_84:
    .asciz "  },\n"
.LS185:
.Lstr_85:
    .asciz "  \"kernel_benchmarks_10k_steps\": {\n"
.LS186:
.Lstr_86:
    .asciz "kernel_0_scalar_ref"
.LS187:
.Lstr_87:
    .asciz "kernel_1_fused_branchfree_c"
.LS188:
.Lstr_88:
    .asciz "kernel_2_avx512_zmm"
.LS189:
.Lstr_89:
    .asciz "  },\n"
.LS190:
.Lstr_90:
    .asciz "  \"speedup_attribution\": {\n"
.LS191:
.Lstr_91:
    .asciz "    \"fused_vs_scalar_mean_speedup\": %.4f,\n"
.LS192:
.Lstr_92:
    .asciz "    \"avx512_vs_scalar_mean_speedup\": %.4f,\n"
.LS193:
.Lstr_93:
    .asciz "    \"avx512_vs_scalar_p50_speedup\": %.4f,\n"
.LS194:
.Lstr_94:
    .asciz "    \"avx512_vs_scalar_phase2_propagation_speedup\": %.4f,\n"
.LS195:
.Lstr_95:
    .asciz "    \"mechanisms\": [\n"
.LS196:
.Lstr_96:
    .asciz "      \"512-bit AVX-512 SIMD width (8x binary64 lanes per zmm instruction, 16 vector steps per 128-site ring)\",\n"
.LS197:
.Lstr_97:
    .asciz "      \"Elimination of 9,600 redundant hardware divsd (/ HBAR = 1.0) instructions per macro-step\",\n"
.LS198:
.Lstr_98:
    .asciz "      \"Periodic halo padding (g_halo[130]) eliminating 19,200 conditional boundary ternary branches per macro-step\",\n"
.LS199:
.Lstr_99:
    .asciz "      \"In-register 25-substep Verlet loop fusion eliminating 75 function calls and intermediate g_H_psi buffer round-trips\"\n"
.LS200:
.Lstr_100:
    .asciz "    ]\n"
.LS201:
.Lstr_101:
    .asciz "  }\n"
.LS202:
.Lstr_102:
    .asciz "}\n"
.LS203:
.Lstr_103:
    .asciz "\n[VERDICT] Native 10-Seed & AVX-512 Symplectic Verlet Engine (%s): PASS (0 ULP Drift, %.2fx Propagation Speedup)\n"
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

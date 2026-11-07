# ZCC asm begin
    .text
    .arch armv8-a
    .global test_smul_ovf
    .type test_smul_ovf, %function
test_smul_ovf:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .file 1 "/tmp/probe_track_a.c"
    .loc 1 1
    sub x0, x29, #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    smull x2, w0, w1
    sxtw x3, w2
    cmp x2, x3
    cset w0, ne
    b .Lfunc_end_100
.Lfunc_end_100:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_umul_ovf
    .type test_umul_ovf, %function
test_umul_ovf:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 2
    sub x0, x29, #16
    ldr w0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldr w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    umull x2, w0, w1
    lsr x2, x2, #32
    cmp x2, #0
    cset w0, ne
    b .Lfunc_end_101
.Lfunc_end_101:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_sadd_sat
    .type test_sadd_sat, %function
test_sadd_sat:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 3
    sub x0, x29, #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    asr w2, w0, #31
    eor w2, w2, #0x7fffffff
    adds w0, w0, w1
    csel w0, w2, w0, vs
    sxtw x0, w0
    b .Lfunc_end_102
.Lfunc_end_102:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_ssub_sat
    .type test_ssub_sat, %function
test_ssub_sat:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 4
    sub x0, x29, #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    asr w2, w0, #31
    eor w2, w2, #0x7fffffff
    subs w0, w0, w1
    csel w0, w2, w0, vs
    sxtw x0, w0
    b .Lfunc_end_103
.Lfunc_end_103:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_uadd_sat
    .type test_uadd_sat, %function
test_uadd_sat:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 5
    sub x0, x29, #16
    ldr w0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldr w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    adds w0, w0, w1
    csinv w0, w0, wzr, cc
    b .Lfunc_end_104
.Lfunc_end_104:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_usub_sat
    .type test_usub_sat, %function
test_usub_sat:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 6
    sub x0, x29, #16
    ldr w0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldr w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    subs w0, w0, w1
    csel w0, w0, wzr, cs
    b .Lfunc_end_105
.Lfunc_end_105:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_const_fold
    .type test_const_fold, %function
test_const_fold:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    .loc 1 7
    .loc 1 8
    .loc 1 13
    .loc 1 12
    .loc 1 11
    .loc 1 10
    .loc 1 9
    .loc 1 8
    mov x0, #2
    str x0, [sp, #-16]!
    ldr x0, =1073741824
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    smull x2, w0, w1
    sxtw x3, w2
    cmp x2, x3
    cset w0, ne
    str x0, [sp, #-16]!
    .loc 1 9
    mov x0, #2
    str x0, [sp, #-16]!
    ldr x0, =2147483648
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    umull x2, w0, w1
    lsr x2, x2, #32
    cmp x2, #0
    cset w0, ne
    str x0, [sp, #-16]!
    mov x0, #1
    mov x1, x0
    ldr x0, [sp], #16
    lsl x0, x0, x1
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    .loc 1 10
    mov x0, #10
    str x0, [sp, #-16]!
    ldr x0, =2147483647
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    asr w2, w0, #31
    eor w2, w2, #0x7fffffff
    adds w0, w0, w1
    csel w0, w2, w0, vs
    sxtw x0, w0
    str x0, [sp, #-16]!
    ldr x0, =2147483647
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, eq
    cmp x0, #0
    beq .L107
    mov x0, #4
    b .L108
.L107:
    mov x0, #0
.L108:
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    .loc 1 11
    mov x0, #10
    str x0, [sp, #-16]!
    ldr x0, =-2147483648
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    asr w2, w0, #31
    eor w2, w2, #0x7fffffff
    subs w0, w0, w1
    csel w0, w2, w0, vs
    sxtw x0, w0
    str x0, [sp, #-16]!
    ldr x0, =-2147483648
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, eq
    cmp x0, #0
    beq .L109
    mov x0, #8
    b .L110
.L109:
    mov x0, #0
.L110:
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    .loc 1 12
    mov x0, #10
    str x0, [sp, #-16]!
    ldr x0, =4294967295
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    adds w0, w0, w1
    csinv w0, w0, wzr, cc
    str x0, [sp, #-16]!
    ldr x0, =4294967295
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, eq
    cmp x0, #0
    beq .L111
    mov x0, #16
    b .L112
.L111:
    mov x0, #0
.L112:
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    .loc 1 13
    mov x0, #10
    str x0, [sp, #-16]!
    mov x0, #5
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    subs w0, w0, w1
    csel w0, w0, wzr, cs
    str x0, [sp, #-16]!
    mov x0, #0
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, eq
    cmp x0, #0
    beq .L113
    mov x0, #32
    b .L114
.L113:
    mov x0, #0
.L114:
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    b .Lfunc_end_106
.Lfunc_end_106:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret

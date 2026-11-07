# ZCC asm begin
    .text
    .arch armv8-a
    .global test_savg_f
    .type test_savg_f, %function
test_savg_f:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .file 1 "/tmp/probe_mv31_post.c"
    .loc 1 1
    sub x0, x29, #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    eor w2, w0, w1
    asr w2, w2, #1
    and w0, w0, w1
    add w0, w0, w2
    sxtw x0, w0
    b .Lfunc_end_100
.Lfunc_end_100:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_savg_c
    .type test_savg_c, %function
test_savg_c:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 2
    sub x0, x29, #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    eor w2, w0, w1
    asr w2, w2, #1
    orr w0, w0, w1
    sub w0, w0, w2
    sxtw x0, w0
    b .Lfunc_end_101
.Lfunc_end_101:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_uavg_f
    .type test_uavg_f, %function
test_uavg_f:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 3
    sub x0, x29, #16
    ldr w0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldr w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    eor w2, w0, w1
    lsr w2, w2, #1
    and w0, w0, w1
    add w0, w0, w2
    b .Lfunc_end_102
.Lfunc_end_102:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_uavg_c
    .type test_uavg_c, %function
test_uavg_c:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .loc 1 4
    sub x0, x29, #16
    ldr w0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldr w0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    eor w2, w0, w1
    lsr w2, w2, #1
    orr w0, w0, w1
    sub w0, w0, w2
    b .Lfunc_end_103
.Lfunc_end_103:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global test_usad32
    .type test_usad32, %function
test_usad32:
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
    subs w2, w0, w1
    sub w3, w1, w0
    csel w0, w2, w3, cs
    b .Lfunc_end_104
.Lfunc_end_104:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret

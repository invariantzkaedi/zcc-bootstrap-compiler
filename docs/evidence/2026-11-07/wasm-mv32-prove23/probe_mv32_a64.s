# ZCC asm begin
    .text
    .arch armv8-a
    .global f_ssad
    .type f_ssad, %function
f_ssad:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    .file 1 "/tmp/probe_mv32.c"
    .loc 1 1
    sub x0, x29, #16
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    cmp w0, w1
    sub w2, w0, w1
    sub w3, w1, w0
    csel w0, w2, w3, ge
    b .Lfunc_end_100
.Lfunc_end_100:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global f_smin
    .type f_smin, %function
f_smin:
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
    cmp w0, w1
    csel w0, w0, w1, le
    sxtw x0, w0
    b .Lfunc_end_101
.Lfunc_end_101:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global f_smax
    .type f_smax, %function
f_smax:
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
    cmp w0, w1
    csel w0, w0, w1, ge
    sxtw x0, w0
    b .Lfunc_end_102
.Lfunc_end_102:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global f_umin
    .type f_umin, %function
f_umin:
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
    cmp w0, w1
    csel w0, w0, w1, ls
    b .Lfunc_end_103
.Lfunc_end_103:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global f_umax
    .type f_umax, %function
f_umax:
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
    cmp w0, w1
    csel w0, w0, w1, cs
    b .Lfunc_end_104
.Lfunc_end_104:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret
    .text
    .arch armv8-a
    .global main
    .type main, %function
main:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    .loc 1 6
    .loc 1 7
    ldr x0, =2147483647
    str x0, [sp, #-16]!
    ldr x0, =-2147483648
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    bl f_ssad
    str x0, [sp, #-16]!
    ldr x0, =4294967295
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, ne
    cmp x0, #0
    beq .L106
    mov x0, #1
    b .Lfunc_end_105
.L106:
    .loc 1 8
    mov x0, #20
    str x0, [sp, #-16]!
    ldr x0, =-10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    bl f_smin
    str x0, [sp, #-16]!
    ldr x0, =-10
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, ne
    cmp x0, #0
    bne .L108
    mov x0, #20
    str x0, [sp, #-16]!
    ldr x0, =-10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    bl f_smax
    str x0, [sp, #-16]!
    mov x0, #20
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, ne
    cmp x0, #0
    bne .L108
    mov x0, #0
    b .L109
.L108:
    mov x0, #1
.L109:
    cmp x0, #0
    beq .L107
    mov x0, #2
    b .Lfunc_end_105
.L107:
    .loc 1 9
    ldr x0, =4294967295
    str x0, [sp, #-16]!
    mov x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    bl f_umin
    str x0, [sp, #-16]!
    mov x0, #10
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, ne
    cmp x0, #0
    bne .L111
    ldr x0, =4294967295
    str x0, [sp, #-16]!
    mov x0, #10
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    ldr x1, [sp], #16
    bl f_umax
    str x0, [sp, #-16]!
    ldr x0, =4294967295
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, ne
    cmp x0, #0
    bne .L111
    mov x0, #0
    b .L112
.L111:
    mov x0, #1
.L112:
    cmp x0, #0
    beq .L110
    mov x0, #3
    b .Lfunc_end_105
.L110:
    .loc 1 10
    mov x0, #0
    b .Lfunc_end_105
.Lfunc_end_105:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret

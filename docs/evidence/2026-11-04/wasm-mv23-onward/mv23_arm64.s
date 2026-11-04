# ZCC asm begin
    .text
    .arch armv8-a
    .global test_mv23
    .type test_mv23, %function
test_mv23:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #256
    stur x0, [x29, #-8]
    stur x1, [x29, #-16]
    stur x2, [x29, #-24]
    stur x3, [x29, #-32]
    .file 1 "/tmp/mv23_inspect.c"
    .loc 1 1
    .loc 1 2
    ldr x0, =-16
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cls w0, w0
    str x0, [sp, #-16]!
    sub x0, x29, #40
    ldr x1, [sp], #16
    str w1, [x0]
    mov x0, x1
    .loc 1 3
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cls w0, w0
    str x0, [sp, #-16]!
    sub x0, x29, #48
    ldr x1, [sp], #16
    str w1, [x0]
    mov x0, x1
    .loc 1 4
    sub x0, x29, #16
    ldr x0, [x0]
    str x0, [sp, #-16]!
    ldr x0, [sp], #16
    cls x0, x0
    str x0, [sp, #-16]!
    sub x0, x29, #56
    ldr x1, [sp], #16
    str w1, [x0]
    mov x0, x1
    .loc 1 5
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    mov x0, #0
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, gt
    cmp x0, #0
    beq .L103
    sub x0, x29, #16
    ldr x0, [x0]
    str x0, [sp, #-16]!
    mov x0, #0
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, lt
    cmp x0, #0
    cset x0, ne
    b .L104
.L103:
    mov x0, #0
.L104:
    cmp x0, #0
    bne .L101
    sub x0, x29, #8
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    mov x0, #0
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, eq
    cmp x0, #0
    bne .L101
    mov x0, #0
    b .L102
.L101:
    mov x0, #1
.L102:
    str x0, [sp, #-16]!
    sub x0, x29, #64
    ldr x1, [sp], #16
    str w1, [x0]
    mov x0, x1
    .loc 1 6
    mov x0, #0
    str x0, [sp, #-16]!
    sub x0, x29, #72
    ldr x1, [sp], #16
    str w1, [x0]
    mov x0, x1
.L105:
    sub x0, x29, #72
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    mov x0, #8
    mov x1, x0
    ldr x0, [sp], #16
    cmp x0, x1
    cset x0, lt
    cmp x0, #0
    beq .L106
    .loc 1 7
    sub x0, x29, #32
    ldr x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #72
    ldrsw x0, [x0]
    mov x1, x0
    ldr x0, [sp], #16
    mov x3, #4
    mul x1, x3, x1
    add x0, x0, x1
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    mov x0, #7
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    sub x0, x29, #24
    ldr x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #72
    ldrsw x0, [x0]
    mov x1, x0
    ldr x0, [sp], #16
    mov x3, #4
    mul x1, x3, x1
    add x0, x0, x1
    ldr x1, [sp], #16
    str w1, [x0]
    mov x0, x1
.L107:
    .loc 1 6
    sub x0, x29, #72
    str x0, [sp, #-16]!
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    mov x3, #1
    add x0, x0, x3
    mov x1, x0
    ldr x3, [sp], #16
    ldr x0, [sp], #16
    str x1, [x0]
    mov x0, x3
    b .L105
.L106:
    .loc 1 9
    sub x0, x29, #40
    ldrsw x0, [x0]
    str x0, [sp, #-16]!
    sub x0, x29, #48
    ldrsw x0, [x0]
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    sub x0, x29, #56
    ldrsw x0, [x0]
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    sub x0, x29, #64
    ldrsw x0, [x0]
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    str x0, [sp, #-16]!
    sub x0, x29, #24
    ldr x0, [x0]
    str x0, [sp, #-16]!
    mov x0, #0
    mov x1, x0
    ldr x0, [sp], #16
    mov x3, #4
    mul x1, x3, x1
    add x0, x0, x1
    ldrsw x0, [x0]
    mov x1, x0
    ldr x0, [sp], #16
    add x0, x0, x1
    b .Lfunc_end_100
.Lfunc_end_100:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret

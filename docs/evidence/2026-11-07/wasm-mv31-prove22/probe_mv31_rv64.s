# ZCC asm begin
    .text
    .option pic
    .globl test_savg_f
    .type test_savg_f, @function
test_savg_f:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .file 1 "/tmp/probe_mv31_post.c"
    .loc 1 1
    addi a0, s0, -16
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -8
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    sext.w t0, a0
    sext.w t1, a1
    xor t2, t0, t1
    srai t2, t2, 1
    and a0, t0, t1
    add a0, a0, t2
    sext.w a0, a0
    j .Lfunc_end_100
.Lfunc_end_100:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_savg_c
    .type test_savg_c, @function
test_savg_c:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 2
    addi a0, s0, -16
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -8
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    sext.w t0, a0
    sext.w t1, a1
    xor t2, t0, t1
    srai t2, t2, 1
    or a0, t0, t1
    sub a0, a0, t2
    sext.w a0, a0
    j .Lfunc_end_101
.Lfunc_end_101:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_uavg_f
    .type test_uavg_f, @function
test_uavg_f:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 3
    addi a0, s0, -16
    lwu a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -8
    lwu a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    slli t0, a0, 32
    srli t0, t0, 32
    slli t1, a1, 32
    srli t1, t1, 32
    xor t2, t0, t1
    srli t2, t2, 1
    and a0, t0, t1
    add a0, a0, t2
    slli a0, a0, 32
    srli a0, a0, 32
    j .Lfunc_end_102
.Lfunc_end_102:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_uavg_c
    .type test_uavg_c, @function
test_uavg_c:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 4
    addi a0, s0, -16
    lwu a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -8
    lwu a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    slli t0, a0, 32
    srli t0, t0, 32
    slli t1, a1, 32
    srli t1, t1, 32
    xor t2, t0, t1
    srli t2, t2, 1
    or a0, t0, t1
    sub a0, a0, t2
    slli a0, a0, 32
    srli a0, a0, 32
    j .Lfunc_end_103
.Lfunc_end_103:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_usad32
    .type test_usad32, @function
test_usad32:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 5
    addi a0, s0, -16
    lwu a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -8
    lwu a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    slli t0, a0, 32
    srli t0, t0, 32
    slli t1, a1, 32
    srli t1, t1, 32
    sub a0, t0, t1
    srai t2, a0, 63
    xor a0, a0, t2
    sub a0, a0, t2
    slli a0, a0, 32
    srli a0, a0, 32
    j .Lfunc_end_104
.Lfunc_end_104:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

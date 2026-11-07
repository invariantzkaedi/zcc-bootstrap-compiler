# ZCC asm begin
    .text
    .option pic
    .globl f_ssad
    .type f_ssad, @function
f_ssad:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .file 1 "/tmp/probe_mv32.c"
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
    sub a0, t0, t1
    srai t2, a0, 63
    xor a0, a0, t2
    sub a0, a0, t2
    slli a0, a0, 32
    srli a0, a0, 32
    j .Lfunc_end_100
.Lfunc_end_100:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl f_smin
    .type f_smin, @function
f_smin:
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
    slt t2, t0, t1
    neg t2, t2
    xor t3, t0, t1
    and t3, t3, t2
    xor a0, t1, t3
    j .Lfunc_end_101
.Lfunc_end_101:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl f_smax
    .type f_smax, @function
f_smax:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 3
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
    slt t2, t1, t0
    neg t2, t2
    xor t3, t0, t1
    and t3, t3, t2
    xor a0, t1, t3
    j .Lfunc_end_102
.Lfunc_end_102:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl f_umin
    .type f_umin, @function
f_umin:
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
    sltu t2, t0, t1
    neg t2, t2
    xor t3, t0, t1
    and t3, t3, t2
    xor a0, t1, t3
    j .Lfunc_end_103
.Lfunc_end_103:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl f_umax
    .type f_umax, @function
f_umax:
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
    sltu t2, t1, t0
    neg t2, t2
    xor t3, t0, t1
    and t3, t3, t2
    xor a0, t1, t3
    j .Lfunc_end_104
.Lfunc_end_104:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl main
    .type main, @function
main:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    .loc 1 6
    .loc 1 7
    li a0, 2147483647
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, -2147483648
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    call f_ssad
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 4294967295
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    snez a0, a0
    snez a0, a0
    beqz a0, .L106
    li a0, 1
    j .Lfunc_end_105
.L106:
    .loc 1 8
    li a0, 20
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, -10
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    call f_smin
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, -10
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    snez a0, a0
    bnez a0, .L108
    li a0, 20
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, -10
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    call f_smax
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 20
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    snez a0, a0
    bnez a0, .L108
    li a0, 0
    j .L109
.L108:
    li a0, 1
.L109:
    snez a0, a0
    beqz a0, .L107
    li a0, 2
    j .Lfunc_end_105
.L107:
    .loc 1 9
    li a0, 4294967295
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 10
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    call f_umin
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 10
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    snez a0, a0
    bnez a0, .L111
    li a0, 4294967295
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 10
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    call f_umax
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 4294967295
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    snez a0, a0
    bnez a0, .L111
    li a0, 0
    j .L112
.L111:
    li a0, 1
.L112:
    snez a0, a0
    beqz a0, .L110
    li a0, 3
    j .Lfunc_end_105
.L110:
    .loc 1 10
    li a0, 0
    j .Lfunc_end_105
.Lfunc_end_105:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

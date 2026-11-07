# ZCC asm begin
    .text
    .option pic
    .globl test_smul_ovf
    .type test_smul_ovf, @function
test_smul_ovf:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .file 1 "/tmp/probe_track_a.c"
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
    mul t2, t0, t1
    sext.w t3, t2
    sub t2, t2, t3
    snez a0, t2
    j .Lfunc_end_100
.Lfunc_end_100:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_umul_ovf
    .type test_umul_ovf, @function
test_umul_ovf:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 2
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
    mul t2, t0, t1
    srli t2, t2, 32
    snez a0, t2
    j .Lfunc_end_101
.Lfunc_end_101:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_sadd_sat
    .type test_sadd_sat, @function
test_sadd_sat:
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
    add t4, t0, t1
    srai t2, t4, 63
    li t3, 2147483647
    xor t2, t2, t3
    sext.w a0, t4
    sub t4, t4, a0
    snez t4, t4
    neg t4, t4
    xor t2, t2, a0
    and t2, t2, t4
    xor a0, a0, t2
    sext.w a0, a0
    j .Lfunc_end_102
.Lfunc_end_102:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_ssub_sat
    .type test_ssub_sat, @function
test_ssub_sat:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 4
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
    sub t4, t0, t1
    srai t2, t4, 63
    li t3, 2147483647
    xor t2, t2, t3
    sext.w a0, t4
    sub t4, t4, a0
    snez t4, t4
    neg t4, t4
    xor t2, t2, a0
    and t2, t2, t4
    xor a0, a0, t2
    sext.w a0, a0
    j .Lfunc_end_103
.Lfunc_end_103:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_uadd_sat
    .type test_uadd_sat, @function
test_uadd_sat:
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
    add a0, t0, t1
    srli t2, a0, 32
    neg t2, t2
    or a0, a0, t2
    slli a0, a0, 32
    srli a0, a0, 32
    j .Lfunc_end_104
.Lfunc_end_104:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_usub_sat
    .type test_usub_sat, @function
test_usub_sat:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    .loc 1 6
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
    not t2, t2
    and a0, a0, t2
    j .Lfunc_end_105
.Lfunc_end_105:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
    .text
    .option pic
    .globl test_const_fold
    .type test_const_fold, @function
test_const_fold:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    .loc 1 7
    .loc 1 8
    .loc 1 13
    .loc 1 12
    .loc 1 11
    .loc 1 10
    .loc 1 9
    .loc 1 8
    li a0, 2
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 1073741824
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    sext.w t0, a0
    sext.w t1, a1
    mul t2, t0, t1
    sext.w t3, t2
    sub t2, t2, t3
    snez a0, t2
    addi sp, sp, -16
    sd a0, 0(sp)
    .loc 1 9
    li a0, 2
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 2147483648
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
    mul t2, t0, t1
    srli t2, t2, 32
    snez a0, t2
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 1
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sll a0, a0, a1
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    .loc 1 10
    li a0, 10
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 2147483647
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    sext.w t0, a0
    sext.w t1, a1
    add t4, t0, t1
    srai t2, t4, 63
    li t3, 2147483647
    xor t2, t2, t3
    sext.w a0, t4
    sub t4, t4, a0
    snez t4, t4
    neg t4, t4
    xor t2, t2, a0
    and t2, t2, t4
    xor a0, a0, t2
    sext.w a0, a0
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 2147483647
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    seqz a0, a0
    beqz a0, .L107
    li a0, 4
    j .L108
.L107:
    li a0, 0
.L108:
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    .loc 1 11
    li a0, 10
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, -2147483648
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    ld a1, 0(sp)
    addi sp, sp, 16
    sext.w t0, a0
    sext.w t1, a1
    sub t4, t0, t1
    srai t2, t4, 63
    li t3, 2147483647
    xor t2, t2, t3
    sext.w a0, t4
    sub t4, t4, a0
    snez t4, t4
    neg t4, t4
    xor t2, t2, a0
    and t2, t2, t4
    xor a0, a0, t2
    sext.w a0, a0
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, -2147483648
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    seqz a0, a0
    beqz a0, .L109
    li a0, 8
    j .L110
.L109:
    li a0, 0
.L110:
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    .loc 1 12
    li a0, 10
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 4294967295
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
    add a0, t0, t1
    srli t2, a0, 32
    neg t2, t2
    or a0, a0, t2
    slli a0, a0, 32
    srli a0, a0, 32
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 4294967295
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    seqz a0, a0
    beqz a0, .L111
    li a0, 16
    j .L112
.L111:
    li a0, 0
.L112:
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    .loc 1 13
    li a0, 10
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 5
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
    not t2, t2
    and a0, a0, t2
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 0
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    seqz a0, a0
    beqz a0, .L113
    li a0, 32
    j .L114
.L113:
    li a0, 0
.L114:
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    j .Lfunc_end_106
.Lfunc_end_106:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# ZCC asm begin
    .text
    .option pic
    .globl test_mv23
    .type test_mv23, @function
test_mv23:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, sp
    addi sp, sp, -256
    sd a0, -8(s0)
    sd a1, -16(s0)
    sd a2, -24(s0)
    sd a3, -32(s0)
    .file 1 "/tmp/mv23_inspect.c"
    .loc 1 1
    .loc 1 2
    li a0, -16
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    slli t0, a0, 32
    srai t1, t0, 63
    xor t0, t0, t1
    slli t0, t0, 1
    li t1, 1
    slli t1, t1, 32
    or t0, t0, t1
    li a0, 0
.Lrv_clrsb_loop_101:
    bltz t0, .Lrv_clrsb_done_101
    slli t0, t0, 1
    addi a0, a0, 1
    j .Lrv_clrsb_loop_101
.Lrv_clrsb_done_101:
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -40
    ld a1, 0(sp)
    addi sp, sp, 16
    sw a1, 0(a0)
    mv a0, a1
    .loc 1 3
    addi a0, s0, -8
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    slli t0, a0, 32
    srai t1, t0, 63
    xor t0, t0, t1
    slli t0, t0, 1
    li t1, 1
    slli t1, t1, 32
    or t0, t0, t1
    li a0, 0
.Lrv_clrsb_loop_102:
    bltz t0, .Lrv_clrsb_done_102
    slli t0, t0, 1
    addi a0, a0, 1
    j .Lrv_clrsb_loop_102
.Lrv_clrsb_done_102:
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -48
    ld a1, 0(sp)
    addi sp, sp, 16
    sw a1, 0(a0)
    mv a0, a1
    .loc 1 4
    addi a0, s0, -16
    ld a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    ld a0, 0(sp)
    addi sp, sp, 16
    srai t1, a0, 63
    xor t0, a0, t1
    slli t0, t0, 1
    ori t0, t0, 1
    li a0, 0
.Lrv_clrsb_loop_103:
    bltz t0, .Lrv_clrsb_done_103
    slli t0, t0, 1
    addi a0, a0, 1
    j .Lrv_clrsb_loop_103
.Lrv_clrsb_done_103:
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -56
    ld a1, 0(sp)
    addi sp, sp, 16
    sw a1, 0(a0)
    mv a0, a1
    .loc 1 5
    addi a0, s0, -8
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 0
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    slt a0, a1, a0
    beqz a0, .L106
    addi a0, s0, -16
    ld a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 0
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    slt a0, a0, a1
    snez a0, a0
    j .L107
.L106:
    li a0, 0
.L107:
    bnez a0, .L104
    addi a0, s0, -8
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 0
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    sub a0, a0, a1
    seqz a0, a0
    bnez a0, .L104
    li a0, 0
    j .L105
.L104:
    li a0, 1
.L105:
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -64
    ld a1, 0(sp)
    addi sp, sp, 16
    sw a1, 0(a0)
    mv a0, a1
    .loc 1 6
    li a0, 0
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -72
    ld a1, 0(sp)
    addi sp, sp, 16
    sw a1, 0(a0)
    mv a0, a1
.L108:
    addi a0, s0, -72
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 8
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    slt a0, a0, a1
    beqz a0, .L109
    .loc 1 7
    addi a0, s0, -32
    ld a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -72
    lw a0, 0(a0)
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    li a3, 4
    mul a1, a3, a1
    add a0, a0, a1
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 7
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -24
    ld a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -72
    lw a0, 0(a0)
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    li a3, 4
    mul a1, a3, a1
    add a0, a0, a1
    ld a1, 0(sp)
    addi sp, sp, 16
    sw a1, 0(a0)
    mv a0, a1
.L110:
    .loc 1 6
    addi a0, s0, -72
    addi sp, sp, -16
    sd a0, 0(sp)
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    li a3, 1
    add a0, a0, a3
    mv a1, a0
    ld a3, 0(sp)
    addi sp, sp, 16
    ld a0, 0(sp)
    addi sp, sp, 16
    sd a1, 0(a0)
    mv a0, a3
    j .L108
.L109:
    .loc 1 9
    addi a0, s0, -40
    lw a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -48
    lw a0, 0(a0)
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -56
    lw a0, 0(a0)
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -64
    lw a0, 0(a0)
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    addi sp, sp, -16
    sd a0, 0(sp)
    addi a0, s0, -24
    ld a0, 0(a0)
    addi sp, sp, -16
    sd a0, 0(sp)
    li a0, 0
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    li a3, 4
    mul a1, a3, a1
    add a0, a0, a1
    lw a0, 0(a0)
    mv a1, a0
    ld a0, 0(sp)
    addi sp, sp, 16
    add a0, a0, a1
    j .Lfunc_end_100
.Lfunc_end_100:
    mv sp, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

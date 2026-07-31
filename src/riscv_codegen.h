/*
 * ZCC RISC-V RV64GC Code Generator
 * Header File: src/riscv_codegen.h
 * Target: RISC-V 64-bit (RV64GC / psABI)
 */

#ifndef ZCC_RISCV_CODEGEN_H
#define ZCC_RISCV_CODEGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* RISC-V 64-bit Registers (x0-x31, zero, ra, sp, fp/s0, a0-a7, t0-t6) */
typedef enum {
    RISCV_REG_ZERO = 0,  /* Hardwired zero x0 */
    RISCV_REG_RA   = 1,  /* Return address x1 */
    RISCV_REG_SP   = 2,  /* Stack pointer x2 */
    RISCV_REG_GP   = 3,  /* Global pointer x3 */
    RISCV_REG_TP   = 4,  /* Thread pointer x4 */
    RISCV_REG_T0   = 5,  /* Alternate link / scratch x5 */
    RISCV_REG_T1   = 6,  /* Scratch x6 */
    RISCV_REG_T2   = 7,  /* Scratch x7 */
    RISCV_REG_FP   = 8,  /* Frame pointer / Saved register s0/x8 */
    RISCV_REG_S1   = 9,  /* Saved register s1/x9 */
    RISCV_REG_A0   = 10, /* Function arg / Return value a0/x10 */
    RISCV_REG_A1   = 11, /* Function arg / Return value a1/x11 */
    RISCV_REG_A2   = 12, /* Function arg a2/x12 */
    RISCV_REG_A3   = 13, /* Function arg a3/x13 */
    RISCV_REG_A4   = 14, /* Function arg a4/x14 */
    RISCV_REG_A5   = 15, /* Function arg a5/x15 */
    RISCV_REG_A6   = 16, /* Function arg a6/x16 */
    RISCV_REG_A7   = 17, /* Function arg a7/x17 */
    RISCV_REG_S2   = 18, /* Saved registers s2-s11 */
    RISCV_REG_S3   = 19,
    RISCV_REG_S4   = 20,
    RISCV_REG_S5   = 21,
    RISCV_REG_S6   = 22,
    RISCV_REG_S7   = 23,
    RISCV_REG_S8   = 24,
    RISCV_REG_S9   = 25,
    RISCV_REG_S10  = 26,
    RISCV_REG_S11  = 27,
    RISCV_REG_T3   = 28, /* Scratch registers t3-t6 */
    RISCV_REG_T4   = 29,
    RISCV_REG_T5   = 30,
    RISCV_REG_T6   = 31
} RISCVRegister;

/* Dynamic Assembly String Buffer */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} RISCVAsmBuffer;

/* Function Declarations */
void riscv_asm_init(RISCVAsmBuffer *buf);
void riscv_asm_free(RISCVAsmBuffer *buf);
void riscv_asm_emit_line(RISCVAsmBuffer *buf, const char *line);

const char *riscv_get_reg_name(RISCVRegister reg);

size_t riscv_align_stack_frame(size_t raw_frame_size);

void riscv_emit_prologue(RISCVAsmBuffer *buf, const char *func_name, size_t stack_size);
void riscv_emit_epilogue(RISCVAsmBuffer *buf, size_t stack_size);

void riscv_emit_add(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2);
void riscv_emit_sub(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2);
void riscv_emit_mul(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2);
void riscv_emit_div(RISCVAsmBuffer *buf, RISCVRegister dest, RISCVRegister src1, RISCVRegister src2);

void riscv_emit_ld_stack(RISCVAsmBuffer *buf, RISCVRegister dest, int offset);
void riscv_emit_sd_stack(RISCVAsmBuffer *buf, RISCVRegister src, int offset);

int zcc_emit_riscv_assembly_to_file(const char *filename, const char *func_name, size_t stack_size);

#endif /* ZCC_RISCV_CODEGEN_H */

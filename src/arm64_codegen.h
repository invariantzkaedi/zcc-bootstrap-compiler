/*
 * ZCC ARM64 AAPCS64 Code Generator
 * Header File: src/arm64_codegen.h
 * Target: AArch64 / ARMv8-A (SystemV / AAPCS64 ABI)
 */

#ifndef ZCC_ARM64_CODEGEN_H
#define ZCC_ARM64_CODEGEN_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* AAPCS64 General Purpose Registers (64-bit x0-x30, sp) */
typedef enum {
    ARM64_REG_X0  = 0,
    ARM64_REG_X1  = 1,
    ARM64_REG_X2  = 2,
    ARM64_REG_X3  = 3,
    ARM64_REG_X4  = 4,
    ARM64_REG_X5  = 5,
    ARM64_REG_X6  = 6,
    ARM64_REG_X7  = 7,
    ARM64_REG_X8  = 8,  /* Indirect result location register */
    ARM64_REG_X9  = 9,  /* Temporary / Scratch */
    ARM64_REG_X10 = 10,
    ARM64_REG_X11 = 11,
    ARM64_REG_X12 = 12,
    ARM64_REG_X13 = 13,
    ARM64_REG_X14 = 14,
    ARM64_REG_X15 = 15,
    ARM64_REG_X16 = 16, /* Intra-procedure call scratch IP0 */
    ARM64_REG_X17 = 17, /* Intra-procedure call scratch IP1 */
    ARM64_REG_X18 = 18, /* Platform register */
    ARM64_REG_X19 = 19, /* Callee-saved x19-x28 */
    ARM64_REG_X20 = 20,
    ARM64_REG_X21 = 21,
    ARM64_REG_X22 = 22,
    ARM64_REG_X23 = 23,
    ARM64_REG_X24 = 24,
    ARM64_REG_X25 = 25,
    ARM64_REG_X26 = 26,
    ARM64_REG_X27 = 27,
    ARM64_REG_X28 = 28,
    ARM64_REG_FP  = 29, /* Frame Pointer x29 */
    ARM64_REG_LR  = 30, /* Link Register x30 */
    ARM64_REG_SP  = 31  /* Stack Pointer sp */
} ARM64Register;

/* ARM64 Condition Codes */
typedef enum {
    ARM64_COND_EQ = 0, /* Equal */
    ARM64_COND_NE = 1, /* Not Equal */
    ARM64_COND_LT = 2, /* Less Than (signed) */
    ARM64_COND_LE = 3, /* Less Than or Equal (signed) */
    ARM64_COND_GT = 4, /* Greater Than (signed) */
    ARM64_COND_GE = 5  /* Greater Than or Equal (signed) */
} ARM64Condition;

/* Dynamic Assembly String Buffer */
typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} ARM64AsmBuffer;

/* Function Declarations */
void arm64_asm_init(ARM64AsmBuffer *buf);
void arm64_asm_free(ARM64AsmBuffer *buf);
void arm64_asm_emit_line(ARM64AsmBuffer *buf, const char *line);

const char *arm64_get_reg_name64(ARM64Register reg);
const char *arm64_get_reg_name32(ARM64Register reg);

size_t arm64_align_stack_frame(size_t raw_frame_size);

void arm64_emit_prologue(ARM64AsmBuffer *buf, const char *func_name, size_t stack_size);
void arm64_emit_epilogue(ARM64AsmBuffer *buf, size_t stack_size);

void arm64_emit_add(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2);
void arm64_emit_sub(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2);
void arm64_emit_mul(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2);
void arm64_emit_sdiv(ARM64AsmBuffer *buf, ARM64Register dest, ARM64Register src1, ARM64Register src2);

void arm64_emit_ldr_stack(ARM64AsmBuffer *buf, ARM64Register dest, int offset);
void arm64_emit_str_stack(ARM64AsmBuffer *buf, ARM64Register src, int offset);

int zcc_emit_arm64_assembly_to_file(const char *filename, const char *func_name, size_t stack_size);

#endif /* ZCC_ARM64_CODEGEN_H */

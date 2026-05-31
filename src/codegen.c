#include "elf_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Declare original main from part5.c */
extern int zcc_main(int argc, char **argv);

/* Segment structure */
typedef struct {
    unsigned char *data;
    size_t size;
    size_t cap;
} Segment;

static void seg_append(Segment *seg, const unsigned char *bytes, size_t size) {
    if (seg->size + size > seg->cap) {
        seg->cap = seg->cap ? seg->cap * 2 : 4096;
        seg->data = (unsigned char *)realloc(seg->data, seg->cap);
    }
    memcpy(seg->data + seg->size, bytes, size);
    seg->size += size;
}

/* Label / Symbol tracking */
typedef struct {
    char name[128];
    size_t offset;
    int is_global;
    int segment; /* 1 = .text, 2 = .data, 3 = SHN_COMMON */
    size_t size; /* for COMMON */
} Label;

static Label labels[4096];
static size_t label_count = 0;

/* Relocation tracking */
typedef struct {
    size_t offset;
    char target_name[128];
    int type;
    long long addend;
} Reloc;

static Reloc relocs[4096];
static size_t reloc_count = 0;

static int parse_reg(const char *s) {
    if (strcmp(s, "%rax") == 0 || strcmp(s, "%eax") == 0 || strcmp(s, "%ax") == 0 || strcmp(s, "%al") == 0) return 0;
    if (strcmp(s, "%rcx") == 0 || strcmp(s, "%ecx") == 0 || strcmp(s, "%cx") == 0 || strcmp(s, "%cl") == 0) return 1;
    if (strcmp(s, "%rdx") == 0 || strcmp(s, "%edx") == 0 || strcmp(s, "%dx") == 0 || strcmp(s, "%dl") == 0) return 2;
    if (strcmp(s, "%rbx") == 0 || strcmp(s, "%ebx") == 0 || strcmp(s, "%bx") == 0 || strcmp(s, "%bl") == 0) return 3;
    if (strcmp(s, "%rsp") == 0 || strcmp(s, "%esp") == 0 || strcmp(s, "%sp") == 0 || strcmp(s, "%spl") == 0) return 4;
    if (strcmp(s, "%rbp") == 0 || strcmp(s, "%ebp") == 0 || strcmp(s, "%bp") == 0 || strcmp(s, "%bpl") == 0) return 5;
    if (strcmp(s, "%rsi") == 0 || strcmp(s, "%esi") == 0 || strcmp(s, "%si") == 0 || strcmp(s, "%sil") == 0) return 6;
    if (strcmp(s, "%rdi") == 0 || strcmp(s, "%edi") == 0 || strcmp(s, "%di") == 0 || strcmp(s, "%dil") == 0) return 7;
    if (strcmp(s, "%r8") == 0 || strcmp(s, "%r8d") == 0 || strcmp(s, "%r8w") == 0 || strcmp(s, "%r8b") == 0) return 8;
    if (strcmp(s, "%r9") == 0 || strcmp(s, "%r9d") == 0 || strcmp(s, "%r9w") == 0 || strcmp(s, "%r9b") == 0) return 9;
    if (strcmp(s, "%r10") == 0 || strcmp(s, "%r10d") == 0 || strcmp(s, "%r10w") == 0 || strcmp(s, "%r10b") == 0) return 10;
    if (strcmp(s, "%r11") == 0 || strcmp(s, "%r11d") == 0 || strcmp(s, "%r11w") == 0 || strcmp(s, "%r11b") == 0) return 11;
    if (strcmp(s, "%r12") == 0 || strcmp(s, "%r12d") == 0 || strcmp(s, "%r12w") == 0 || strcmp(s, "%r12b") == 0) return 12;
    if (strcmp(s, "%r13") == 0 || strcmp(s, "%r13d") == 0 || strcmp(s, "%r13w") == 0 || strcmp(s, "%r13b") == 0) return 13;
    if (strcmp(s, "%r14") == 0 || strcmp(s, "%r14d") == 0 || strcmp(s, "%r14w") == 0 || strcmp(s, "%r14b") == 0) return 14;
    if (strcmp(s, "%r15") == 0 || strcmp(s, "%r15d") == 0 || strcmp(s, "%r15w") == 0 || strcmp(s, "%r15b") == 0) return 15;
    return -1;
}

static int parse_mem_operand(const char *s, int *reg, long long *disp) {
    const char *paren = strchr(s, '(');
    if (paren) {
        char reg_name[64];
        const char *end_paren = strchr(paren, ')');
        if (!end_paren) return 0;
        size_t len = end_paren - (paren + 1);
        if (len >= 63) len = 63;
        strncpy(reg_name, paren + 1, len);
        reg_name[len] = '\0';
        *reg = parse_reg(reg_name);
        
        if (paren == s) {
            *disp = 0;
        } else {
            char disp_str[64];
            size_t disp_len = paren - s;
            if (disp_len >= 63) disp_len = 63;
            strncpy(disp_str, s, disp_len);
            disp_str[disp_len] = '\0';
            *disp = atoll(disp_str);
        }
        return 1;
    }
    return 0;
}

/* Helper encoders */
static void encode_movq(Segment *seg, int src_reg, int dst_reg, int is_mem_src, int is_mem_dst, long long disp) {
    unsigned char rex = 0x48;
    unsigned char opcode;
    unsigned char modrm;
    
    if (is_mem_src) {
        if (src_reg & 8) rex |= 0x01;
        if (dst_reg & 8) rex |= 0x04;
    } else {
        if (src_reg & 8) rex |= 0x04;
        if (dst_reg & 8) rex |= 0x01;
    }
    
    if (is_mem_src) {
        opcode = 0x8b;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF;
            d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF;
            d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else if (is_mem_dst) {
        opcode = 0x89;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF;
            d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF;
            d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        opcode = 0x89;
        modrm = (0x03 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_leaq(Segment *seg, int src_reg, int dst_reg, long long disp) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0x8d;
    unsigned char modrm;
    
    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;
    
    if (disp >= -128 && disp <= 127) {
        modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        unsigned char d = (unsigned char)disp;
        seg_append(seg, &d, 1);
    } else {
        modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        unsigned char d[4];
        d[0] = disp & 0xFF;
        d[1] = (disp >> 8) & 0xFF;
        d[2] = (disp >> 16) & 0xFF;
        d[3] = (disp >> 24) & 0xFF;
        seg_append(seg, d, 4);
    }
}

static void encode_movq_imm(Segment *seg, long long imm, int dst_reg) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0xc7;
    unsigned char modrm = 0xc0 | (dst_reg & 7);
    if (dst_reg & 8) rex |= 0x01;
    seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
    unsigned char d[4];
    d[0] = imm & 0xFF;
    d[1] = (imm >> 8) & 0xFF;
    d[2] = (imm >> 16) & 0xFF;
    d[3] = (imm >> 24) & 0xFF;
    seg_append(seg, d, 4);
}

static void encode_rip_relative(Segment *seg, const char *label_name, int reg, int is_load) {
    unsigned char rex = 0x48;
    unsigned char opcode = (is_load == 2) ? 0x8d : (is_load ? 0x8b : 0x89);
    unsigned char modrm = 0x05 | ((reg & 7) << 3);
    if (reg & 8) rex |= 0x04;
    
    seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
    
    Reloc r;
    r.offset = seg->size;
    strcpy(r.target_name, label_name);
    r.type = R_X86_64_PC32;
    r.addend = -4;
    relocs[reloc_count++] = r;
    
    unsigned char zero[4] = {0, 0, 0, 0};
    seg_append(seg, zero, 4);
}

static void encode_jump(Segment *seg, const char *mnemonic, const char *target_label) {
    unsigned char bytes[2];
    size_t size = 0;
    
    if (strcmp(mnemonic, "jmp") == 0) {
        bytes[0] = 0xe9;
        size = 1;
    } else if (strcmp(mnemonic, "je") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x84; size = 2;
    } else if (strcmp(mnemonic, "jne") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x85; size = 2;
    } else if (strcmp(mnemonic, "jl") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8c; size = 2;
    } else if (strcmp(mnemonic, "jle") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8d; size = 2;
    } else if (strcmp(mnemonic, "jg") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8f; size = 2;
    } else if (strcmp(mnemonic, "jge") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8e; size = 2;
    } else if (strcmp(mnemonic, "jb") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x82; size = 2;
    } else if (strcmp(mnemonic, "jbe") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x83; size = 2;
    } else if (strcmp(mnemonic, "ja") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x87; size = 2;
    } else if (strcmp(mnemonic, "jae") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x86; size = 2;
    }
    
    if (size > 0) {
        seg_append(seg, bytes, size);
        
        Reloc r;
        r.offset = seg->size;
        strcpy(r.target_name, target_label);
        r.type = R_X86_64_PC32;
        r.addend = -4;
        relocs[reloc_count++] = r;
        
        unsigned char zero[4] = {0, 0, 0, 0};
        seg_append(seg, zero, 4);
    }
}

static void encode_call(Segment *seg, const char *target) {
    unsigned char opcode = 0xe8;
    seg_append(seg, &opcode, 1);
    
    Reloc r;
    r.offset = seg->size;
    strcpy(r.target_name, target);
    r.type = R_X86_64_PLT32;
    r.addend = -4;
    relocs[reloc_count++] = r;
    
    unsigned char zero[4] = {0, 0, 0, 0};
    seg_append(seg, zero, 4);
}

static void encode_binop(Segment *seg, const char *op, int src_reg, int dst_reg) {
    unsigned char rex = 0x48;
    unsigned char opcode;
    unsigned char modrm;
    
    if (src_reg & 8) rex |= 0x04;
    if (dst_reg & 8) rex |= 0x01;
    
    if (strcmp(op, "addq") == 0) {
        opcode = 0x01;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    } else if (strcmp(op, "subq") == 0) {
        opcode = 0x29;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    } else if (strcmp(op, "cmpq") == 0) {
        opcode = 0x39;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    } else if (strcmp(op, "andq") == 0) {
        opcode = 0x21;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    } else if (strcmp(op, "orq") == 0) {
        opcode = 0x09;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    } else if (strcmp(op, "xorq") == 0) {
        opcode = 0x31;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    } else if (strcmp(op, "imulq") == 0) {
        unsigned char op2[2] = {0x0f, 0xaf};
        modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
        rex = 0x48;
        if (dst_reg & 8) rex |= 0x04;
        if (src_reg & 8) rex |= 0x01;
        seg_append(seg, &rex, 1);
        seg_append(seg, op2, 2);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_shift(Segment *seg, const char *op, int dst_reg) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0xd3;
    unsigned char modrm;
    if (dst_reg & 8) rex |= 0x01;
    if (strcmp(op, "sarq") == 0) {
        modrm = 0xf8 | (dst_reg & 7);
    } else if (strcmp(op, "shrq") == 0) {
        modrm = 0xe8 | (dst_reg & 7);
    } else {
        modrm = 0xe0 | (dst_reg & 7);
    }
    seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
}

static void encode_idivq(Segment *seg, int reg) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0xf7;
    unsigned char modrm = 0xf8 | (reg & 7);
    if (reg & 8) rex |= 0x01;
    seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
}

static void encode_divq(Segment *seg, int reg) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0xf7;
    unsigned char modrm = 0xf0 | (reg & 7);
    if (reg & 8) rex |= 0x01;
    seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
}

static void encode_negq(Segment *seg, int reg) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0xf7;
    unsigned char modrm = 0xd8 | (reg & 7);
    if (reg & 8) rex |= 0x01;
    seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
}

static void encode_movl(Segment *seg, int src_reg, int dst_reg, int is_mem_src, int is_mem_dst, long long disp) {
    unsigned char rex = 0x40;
    unsigned char opcode;
    unsigned char modrm;
    
    if (is_mem_src) {
        if (src_reg & 8) rex |= 0x01;
        if (dst_reg & 8) rex |= 0x04;
    } else {
        if (src_reg & 8) rex |= 0x04;
        if (dst_reg & 8) rex |= 0x01;
    }
    
    if (is_mem_src) {
        opcode = 0x8b;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else if (is_mem_dst) {
        opcode = 0x89;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        opcode = 0x89;
        modrm = (0x03 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
        if (rex != 0x40) seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movl_imm(Segment *seg, long long imm, int dst_reg) {
    unsigned char rex = 0x40;
    unsigned char opcode = 0xc7;
    unsigned char modrm = 0xc0 | (dst_reg & 7);
    if (dst_reg & 8) rex |= 0x01;
    if (rex != 0x40) seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
    unsigned char d[4];
    d[0] = imm & 0xFF;
    d[1] = (imm >> 8) & 0xFF;
    d[2] = (imm >> 16) & 0xFF;
    d[3] = (imm >> 24) & 0xFF;
    seg_append(seg, d, 4);
}

static void encode_movzwq(Segment *seg, int src_reg, int dst_reg, int is_mem_src, long long disp) {
    unsigned char rex = 0x48;
    unsigned char opcode[2] = {0x0f, 0xb7};
    unsigned char modrm;
    
    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;
    
    if (is_mem_src) {
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, opcode, 2);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movslq(Segment *seg, int src_reg, int dst_reg) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0x63;
    unsigned char modrm = 0x00 | ((dst_reg & 7) << 3) | (src_reg & 7);
    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;
    seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
}

static void encode_set(Segment *seg, const char *mnemonic) {
    unsigned char bytes[3] = {0x0f, 0x00, 0xc0};
    if (strcmp(mnemonic, "sete") == 0) bytes[1] = 0x94;
    else if (strcmp(mnemonic, "setne") == 0) bytes[1] = 0x95;
    else if (strcmp(mnemonic, "setl") == 0) bytes[1] = 0x9c;
    else if (strcmp(mnemonic, "setle") == 0) bytes[1] = 0x9d;
    else if (strcmp(mnemonic, "setg") == 0) bytes[1] = 0x9f;
    else if (strcmp(mnemonic, "setge") == 0) bytes[1] = 0x9e;
    else if (strcmp(mnemonic, "setb") == 0) bytes[1] = 0x92;
    else if (strcmp(mnemonic, "setbe") == 0) bytes[1] = 0x93;
    else if (strcmp(mnemonic, "seta") == 0) bytes[1] = 0x97;
    else if (strcmp(mnemonic, "setae") == 0) bytes[1] = 0x96;
    seg_append(seg, bytes, 3);
}

static void encode_movb(Segment *seg, int src_reg, int dst_reg, int is_mem_src, int is_mem_dst, long long disp) {
    unsigned char rex = 0x40;
    unsigned char opcode;
    unsigned char modrm;
    
    if (is_mem_src) {
        if (src_reg & 8) rex |= 0x01;
        if (dst_reg & 8) rex |= 0x04;
    } else {
        if (src_reg & 8) rex |= 0x04;
        if (dst_reg & 8) rex |= 0x01;
    }
    
    if (is_mem_src) {
        opcode = 0x8a;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40 || dst_reg >= 4 || src_reg >= 4) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40 || dst_reg >= 4 || src_reg >= 4) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else if (is_mem_dst) {
        opcode = 0x88;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40 || src_reg >= 4 || dst_reg >= 4) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40 || src_reg >= 4 || dst_reg >= 4) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        opcode = 0x88;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        if (rex != 0x40 || src_reg >= 4 || dst_reg >= 4) seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movw(Segment *seg, int src_reg, int dst_reg, int is_mem_src, int is_mem_dst, long long disp) {
    unsigned char prefix = 0x66;
    unsigned char rex = 0x40;
    unsigned char opcode;
    unsigned char modrm;
    
    if (is_mem_src) {
        if (src_reg & 8) rex |= 0x01;
        if (dst_reg & 8) rex |= 0x04;
    } else {
        if (src_reg & 8) rex |= 0x04;
        if (dst_reg & 8) rex |= 0x01;
    }
    
    seg_append(seg, &prefix, 1);
    
    if (is_mem_src) {
        opcode = 0x8b;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else if (is_mem_dst) {
        opcode = 0x89;
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        opcode = 0x89;
        modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
        if (rex != 0x40) seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movswq(Segment *seg, int src_reg, int dst_reg, int is_mem_src, long long disp) {
    unsigned char rex = 0x48;
    unsigned char opcode[2] = {0x0f, 0xbf};
    unsigned char modrm;
    
    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;
    
    if (is_mem_src) {
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF;
            d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF;
            d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, opcode, 2);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movzbl(Segment *seg, int src_reg, int dst_reg, int is_mem_src, long long disp) {
    unsigned char rex = 0x40;
    unsigned char opcode[2] = {0x0f, 0xb6};
    unsigned char modrm;
    
    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;
    
    if (is_mem_src) {
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF;
            d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF;
            d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
        if (rex != 0x40) seg_append(seg, &rex, 1);
        seg_append(seg, opcode, 2);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movzwl(Segment *seg, int src_reg, int dst_reg, int is_mem_src, long long disp) {
    unsigned char rex = 0x40;
    unsigned char opcode[2] = {0x0f, 0xb7};
    unsigned char modrm;

    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;

    if (is_mem_src) {
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
        if (rex != 0x40) seg_append(seg, &rex, 1);
        seg_append(seg, opcode, 2);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movsbq(Segment *seg, int src_reg, int dst_reg, int is_mem_src, long long disp) {
    unsigned char rex = 0x48;
    unsigned char opcode[2] = {0x0f, 0xbe};
    unsigned char modrm;
    
    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;
    
    if (is_mem_src) {
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF;
            d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF;
            d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, opcode, 2);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movzbq(Segment *seg, int src_reg, int dst_reg, int is_mem_src, long long disp) {
    unsigned char rex = 0x48;
    unsigned char opcode[2] = {0x0f, 0xb6};
    unsigned char modrm;
    
    if (src_reg & 8) rex |= 0x01;
    if (dst_reg & 8) rex |= 0x04;
    
    if (is_mem_src) {
        if (disp >= -128 && disp <= 127) {
            modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            unsigned char d[4];
            d[0] = disp & 0xFF;
            d[1] = (disp >> 8) & 0xFF;
            d[2] = (disp >> 16) & 0xFF;
            d[3] = (disp >> 24) & 0xFF;
            seg_append(seg, d, 4);
        }
    } else {
        modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, opcode, 2);
        seg_append(seg, &modrm, 1);
    }
}

/* Trim helper */
static char *trim(char *s) {
    char *end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return s;
}

/* Two-Pass Assembler Core */
static int assemble(const char *in_s_filename, const char *out_o_filename) {
    FILE *in = fopen(in_s_filename, "r");
    Segment text_seg = { NULL, 0, 0 };
    Segment data_seg = { NULL, 0, 0 };
    char line[1024];
    int current_segment = 1; /* 1 = .text, 2 = .data */

    label_count = 0;
    reloc_count = 0;

    if (!in) {
        return -1;
    }

    /* Pass 1: Parse and encode */
    while (fgets(line, sizeof(line), in)) {
        char *p = strchr(line, '#');
        if (p) *p = '\0';
        p = strchr(line, ';');
        if (p) *p = '\0';
        
        p = trim(line);
        if (*p == '\0') continue;

        /* Is it a label? */
        char *colon = strchr(p, ':');
        if (colon && !strchr(p, '"')) {
            *colon = '\0';
            char *lbl_name = trim(p);
            /* Add or update label */
            size_t idx = 9999;
            size_t i;
            for (i = 0; i < label_count; i++) {
                if (strcmp(labels[i].name, lbl_name) == 0) {
                    idx = i;
                    break;
                }
            }
            if (idx == 9999) {
                idx = label_count++;
                strcpy(labels[idx].name, lbl_name);
                labels[idx].is_global = 0;
            }
            labels[idx].offset = (current_segment == 1) ? text_seg.size : data_seg.size;
            labels[idx].segment = current_segment;
            continue;
        }

        /* Is it a directive? */
        if (*p == '.') {
            char dir[64];
            int n = sscanf(p, "%63s", dir);
            if (n <= 0) continue;
            
            if (strcmp(dir, ".text") == 0) {
                current_segment = 1;
            } else if (strcmp(dir, ".data") == 0 || strcmp(dir, ".section") == 0 || strcmp(dir, ".rodata") == 0) {
                current_segment = 2;
            } else if (strcmp(dir, ".globl") == 0 || strcmp(dir, ".global") == 0) {
                char name[128];
                sscanf(p + strlen(dir), "%127s", name);
                char *gname = trim(name);
                size_t i;
                int found = 0;
                for (i = 0; i < label_count; i++) {
                    if (strcmp(labels[i].name, gname) == 0) {
                        labels[i].is_global = 1;
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    size_t idx = label_count++;
                    strcpy(labels[idx].name, gname);
                    labels[idx].is_global = 1;
                    labels[idx].segment = 0;
                }
            } else if (strcmp(dir, ".comm") == 0 || strcmp(dir, ".common") == 0) {
                char name[128];
                long long size = 0;
                long long align = 8;
                char *args = p + strlen(dir);
                char *comma1 = strchr(args, ',');
                if (comma1) {
                    *comma1 = '\0';
                    strcpy(name, trim(args));
                    char *comma2 = strchr(comma1 + 1, ',');
                    if (comma2) {
                        *comma2 = '\0';
                        size = atoll(comma1 + 1);
                        align = atoll(comma2 + 1);
                    } else {
                        size = atoll(comma1 + 1);
                    }
                    size_t idx = label_count++;
                    strcpy(labels[idx].name, name);
                    labels[idx].is_global = 1;
                    labels[idx].segment = 3; /* Common */
                    labels[idx].offset = align;
                    labels[idx].size = size;
                }
            } else if (strcmp(dir, ".long") == 0) {
                long long val = strtoll(p + 5, NULL, 0);
                unsigned char b[4];
                b[0] = val & 0xFF;
                b[1] = (val >> 8) & 0xFF;
                b[2] = (val >> 16) & 0xFF;
                b[3] = (val >> 24) & 0xFF;
                seg_append(&data_seg, b, 4);
            } else if (strcmp(dir, ".quad") == 0) {
                /* Check if it's a label or constant */
                char target[128];
                long long val = 0;
                char *arg = trim(p + 5);
                if (isalpha((unsigned char)*arg) || *arg == '.' || *arg == '_') {
                    sscanf(arg, "%127s", target);
                    Reloc r;
                    r.offset = data_seg.size;
                    strcpy(r.target_name, target);
                    r.type = R_X86_64_64;
                    r.addend = 0;
                    relocs[reloc_count++] = r;
                    unsigned char zero[8] = {0,0,0,0,0,0,0,0};
                    seg_append(&data_seg, zero, 8);
                } else {
                    val = strtoll(arg, NULL, 0);
                    unsigned char b[8];
                    int i;
                    for (i = 0; i < 8; i++) {
                        b[i] = (val >> (i * 8)) & 0xFF;
                    }
                    seg_append(&data_seg, b, 8);
                }
            } else if (strcmp(dir, ".byte") == 0) {
                int val = (int)strtoll(p + 5, NULL, 0);
                unsigned char b = (unsigned char)val;
                seg_append(&data_seg, &b, 1);
            } else if (strcmp(dir, ".short") == 0) {
                int val = (int)strtoll(p + 6, NULL, 0);
                unsigned char b[2];
                b[0] = val & 0xFF;
                b[1] = (val >> 8) & 0xFF;
                seg_append(&data_seg, b, 2);
            } else if (strcmp(dir, ".zero") == 0) {
                long long size = 0;
                sscanf(p + 5, "%lld", &size);
                if (size > 0) {
                    unsigned char *zeros = (unsigned char *)calloc(1, size);
                    seg_append(&data_seg, zeros, size);
                    free(zeros);
                }
            } else if (strcmp(dir, ".align") == 0) {
                long long align = 0;
                sscanf(p + 6, "%lld", &align);
                if (align > 0) {
                    size_t pad_size = (align - (data_seg.size % align)) % align;
                    if (pad_size > 0) {
                        unsigned char *zeros = (unsigned char *)calloc(1, pad_size);
                        seg_append(&data_seg, zeros, pad_size);
                        free(zeros);
                    }
                }
            } else if (strcmp(dir, ".ascii") == 0 || strcmp(dir, ".asciz") == 0 || strcmp(dir, ".string") == 0) {
                char *str_start = strchr(p, '"');
                if (str_start) {
                    char *src = str_start + 1;
                    while (*src && *src != '"') {
                        if (*src == '\\') {
                            src++;
                            if (*src == 'n') { seg_append(&data_seg, (const unsigned char *)"\n", 1); }
                            else if (*src == 't') { seg_append(&data_seg, (const unsigned char *)"\t", 1); }
                            else if (*src == 'r') { seg_append(&data_seg, (const unsigned char *)"\r", 1); }
                            else if (*src == '\\') { seg_append(&data_seg, (const unsigned char *)"\\", 1); }
                            else if (*src == '"') { seg_append(&data_seg, (const unsigned char *)"\"", 1); }
                            else if (*src == '0') {
                                unsigned char zero = 0;
                                seg_append(&data_seg, &zero, 1);
                            }
                        } else {
                            seg_append(&data_seg, (const unsigned char *)src, 1);
                        }
                        src++;
                    }
                    if (strcmp(dir, ".asciz") == 0 || strcmp(dir, ".string") == 0) {
                        unsigned char zero = 0;
                        seg_append(&data_seg, &zero, 1);
                    }
                }
            }
            continue;
        }

        /* Instruction parsing */
        char mnemonic[64];
        char *args_start = NULL;
        char *space = strchr(p, ' ');
        if (!space) space = strchr(p, '\t');
        
        if (space) {
            size_t mlen = space - p;
            if (mlen >= 63) mlen = 63;
            strncpy(mnemonic, p, mlen);
            mnemonic[mlen] = '\0';
            args_start = space + 1;
        } else {
            strcpy(mnemonic, p);
        }

        /* 0-operand instructions */
        if (strcmp(mnemonic, "ret") == 0) {
            unsigned char b = 0xc3;
            seg_append(&text_seg, &b, 1);
        } else if (strcmp(mnemonic, "leave") == 0) {
            unsigned char b = 0xc9;
            seg_append(&text_seg, &b, 1);
        } else if (strcmp(mnemonic, "cqto") == 0 || strcmp(mnemonic, "cqo") == 0) {
            unsigned char b[2] = {0x48, 0x99};
            seg_append(&text_seg, b, 2);
        } else if (strcmp(mnemonic, "cltq") == 0) {
            unsigned char b[2] = {0x48, 0x98};
            seg_append(&text_seg, b, 2);
        }
        /* 1-operand instructions */
        else if (strcmp(mnemonic, "pushq") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            if (reg == 5) {
                unsigned char b = 0x55;
                seg_append(&text_seg, &b, 1);
            } else if (reg >= 0) {
                unsigned char rex = 0x40 | (reg >> 3);
                unsigned char opcode = 0x50 | (reg & 7);
                if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                seg_append(&text_seg, &opcode, 1);
            }
        } else if (strcmp(mnemonic, "popq") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            if (reg == 5) {
                unsigned char b = 0x5d;
                seg_append(&text_seg, &b, 1);
            } else if (reg >= 0) {
                unsigned char rex = 0x40 | (reg >> 3);
                unsigned char opcode = 0x58 | (reg & 7);
                if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                seg_append(&text_seg, &opcode, 1);
            }
        } else if (strcmp(mnemonic, "call") == 0) {
            char *target = args_start ? trim(args_start) : "";
            encode_call(&text_seg, target);
        } else if (mnemonic[0] == 'j') {
            char *target = args_start ? trim(args_start) : "";
            encode_jump(&text_seg, mnemonic, target);
        } else if (strncmp(mnemonic, "set", 3) == 0 && strlen(mnemonic) <= 5) {
            encode_set(&text_seg, mnemonic);
        } else if (strcmp(mnemonic, "idivq") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            encode_idivq(&text_seg, reg);
        } else if (strcmp(mnemonic, "divq") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            encode_divq(&text_seg, reg);
        } else if (strcmp(mnemonic, "negq") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            encode_negq(&text_seg, reg);
        }
        /* 2-operand instructions */
        else if (args_start) {
            char op1[128], op2[128];
            char *comma = strchr(args_start, ',');
            if (comma) {
                *comma = '\0';
                strcpy(op1, trim(args_start));
                strcpy(op2, trim(comma + 1));
                
                int reg1 = parse_reg(op1);
                int reg2 = parse_reg(op2);
                
                if (strcmp(mnemonic, "movq") == 0) {
                    if (op1[0] == '$') {
                        long long imm = atoll(op1 + 1);
                        encode_movq_imm(&text_seg, imm, reg2);
                    } else if (strstr(op1, "(%rip)")) {
                        char lbl[128];
                        char *rip = strstr(op1, "(%rip)");
                        *rip = '\0';
                        strcpy(lbl, trim(op1));
                        encode_rip_relative(&text_seg, lbl, reg2, 1);
                    } else if (strstr(op2, "(%rip)")) {
                        char lbl[128];
                        char *rip = strstr(op2, "(%rip)");
                        *rip = '\0';
                        strcpy(lbl, trim(op2));
                        encode_rip_relative(&text_seg, lbl, reg1, 0);
                    } else {
                        int is_mem1 = 0, is_mem2 = 0;
                        long long disp1 = 0, disp2 = 0;
                        if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                        if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                        encode_movq(&text_seg, reg1, reg2, is_mem1, is_mem2, is_mem1 ? disp1 : disp2);
                    }
                } else if (strcmp(mnemonic, "movl") == 0) {
                    if (op1[0] == '$') {
                        long long imm = atoll(op1 + 1);
                        encode_movl_imm(&text_seg, imm, reg2);
                    } else {
                        int is_mem1 = 0, is_mem2 = 0;
                        long long disp1 = 0, disp2 = 0;
                        if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                        if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                        encode_movl(&text_seg, reg1, reg2, is_mem1, is_mem2, is_mem1 ? disp1 : disp2);
                    }
                } else if (strcmp(mnemonic, "leaq") == 0) {
                    if (strstr(op1, "(%rip)")) {
                        char lbl[128];
                        char *rip = strstr(op1, "(%rip)");
                        *rip = '\0';
                        strcpy(lbl, trim(op1));
                        encode_rip_relative(&text_seg, lbl, reg2, 2);
                    } else {
                        int is_mem1 = 0;
                        long long disp1 = 0;
                        if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                        encode_leaq(&text_seg, reg1, reg2, disp1);
                    }
                } else if (strcmp(mnemonic, "movb") == 0) {
                    int is_mem1 = 0, is_mem2 = 0;
                    long long disp1 = 0, disp2 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                    encode_movb(&text_seg, reg1, reg2, is_mem1, is_mem2, is_mem1 ? disp1 : disp2);
                } else if (strcmp(mnemonic, "movw") == 0) {
                    int is_mem1 = 0, is_mem2 = 0;
                    long long disp1 = 0, disp2 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                    encode_movw(&text_seg, reg1, reg2, is_mem1, is_mem2, is_mem1 ? disp1 : disp2);
                } else if (strcmp(mnemonic, "movswq") == 0) {
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    encode_movswq(&text_seg, reg1, reg2, is_mem1, disp1);
                } else if (strcmp(mnemonic, "movzbl") == 0) {
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    encode_movzbl(&text_seg, reg1, reg2, is_mem1, disp1);
                } else if (strcmp(mnemonic, "movzwl") == 0) {
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    encode_movzwl(&text_seg, reg1, reg2, is_mem1, disp1);
                } else if (strcmp(mnemonic, "movzwq") == 0) {
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    encode_movzwq(&text_seg, reg1, reg2, is_mem1, disp1);
                } else if (strcmp(mnemonic, "movsbq") == 0) {
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    encode_movsbq(&text_seg, reg1, reg2, is_mem1, disp1);
                } else if (strcmp(mnemonic, "movzbq") == 0) {
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    encode_movzbq(&text_seg, reg1, reg2, is_mem1, disp1);
                } else if (strcmp(mnemonic, "movslq") == 0) {
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    if (is_mem1) {
                        /* movslq (reg1), reg2 -> 48 63 with ModRM */
                        unsigned char rex = 0x48;
                        unsigned char opcode = 0x63;
                        unsigned char modrm = ((reg2 & 7) << 3) | (reg1 & 7);
                        if (reg1 & 8) rex |= 0x01;
                        if (reg2 & 8) rex |= 0x04;
                        seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                    } else {
                        encode_movslq(&text_seg, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "addq") == 0 || strcmp(mnemonic, "subq") == 0 ||
                           strcmp(mnemonic, "cmpq") == 0 || strcmp(mnemonic, "imulq") == 0 ||
                           strcmp(mnemonic, "andq") == 0 || strcmp(mnemonic, "orq") == 0 ||
                           strcmp(mnemonic, "xorq") == 0) {
                    if (op1[0] == '$') {
                        long long imm = atoll(op1 + 1);
                        unsigned char rex = 0x48;
                        unsigned char opcode = 0x81;
                        unsigned char modrm = 0xc0;
                        if (strcmp(mnemonic, "addq") == 0) modrm = 0xc0;
                        else if (strcmp(mnemonic, "subq") == 0) modrm = 0xe8;
                        else if (strcmp(mnemonic, "cmpq") == 0) modrm = 0xf8;
                        modrm |= (reg2 & 7);
                        if (reg2 & 8) rex |= 0x01;
                        
                        if (imm >= -128 && imm <= 127) {
                            opcode = 0x83;
                            seg_append(&text_seg, &rex, 1);
                            seg_append(&text_seg, &opcode, 1);
                            seg_append(&text_seg, &modrm, 1);
                            unsigned char b = (unsigned char)imm;
                            seg_append(&text_seg, &b, 1);
                        } else {
                            seg_append(&text_seg, &rex, 1);
                            seg_append(&text_seg, &opcode, 1);
                            seg_append(&text_seg, &modrm, 1);
                            unsigned char b[4];
                            b[0] = imm & 0xFF;
                            b[1] = (imm >> 8) & 0xFF;
                            b[2] = (imm >> 16) & 0xFF;
                            b[3] = (imm >> 24) & 0xFF;
                            seg_append(&text_seg, b, 4);
                        }
                    } else {
                        encode_binop(&text_seg, mnemonic, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "sarq") == 0 || strcmp(mnemonic, "shlq") == 0 || strcmp(mnemonic, "shrq") == 0) {
                    if (op1[0] == '$') {
                        long long imm = atoll(op1 + 1);
                        unsigned char rex = 0x48;
                        unsigned char opcode = 0xc1;
                        unsigned char modrm;
                        if (reg2 & 8) rex |= 0x01;
                        if (strcmp(mnemonic, "sarq") == 0) {
                            modrm = 0xf8 | (reg2 & 7);
                        } else if (strcmp(mnemonic, "shrq") == 0) {
                            modrm = 0xe8 | (reg2 & 7);
                        } else {
                            modrm = 0xe0 | (reg2 & 7);
                        }
                        seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                        unsigned char b = (unsigned char)imm;
                        seg_append(&text_seg, &b, 1);
                    } else {
                        encode_shift(&text_seg, mnemonic, reg2);
                    }
                } else if (strcmp(mnemonic, "cmpl") == 0) {
                    if (op1[0] == '$') {
                        long long imm = atoll(op1 + 1);
                        unsigned char rex = 0x40;
                        unsigned char opcode = 0x81;
                        unsigned char modrm = 0xf8 | (reg2 & 7);
                        if (reg2 & 8) rex |= 0x01;
                        
                        if (imm >= -128 && imm <= 127) {
                            opcode = 0x83;
                            if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                            seg_append(&text_seg, &opcode, 1);
                            seg_append(&text_seg, &modrm, 1);
                            unsigned char b = (unsigned char)imm;
                            seg_append(&text_seg, &b, 1);
                        } else {
                            if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                            seg_append(&text_seg, &opcode, 1);
                            seg_append(&text_seg, &modrm, 1);
                            unsigned char b[4];
                            b[0] = imm & 0xFF; b[1] = (imm >> 8) & 0xFF;
                            b[2] = (imm >> 16) & 0xFF; b[3] = (imm >> 24) & 0xFF;
                            seg_append(&text_seg, b, 4);
                        }
                    } else {
                        unsigned char rex = 0x40;
                        unsigned char opcode = 0x39;
                        unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
                        if (reg1 & 8) rex |= 0x04;
                        if (reg2 & 8) rex |= 0x01;
                        if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                    }
                }
            }
        }
    }
    fclose(in);
    printf("codegen: PASS 1 DONE, text_seg.size=%d, data_seg.size=%d, labels=%d, relocs=%d\n", (int)text_seg.size, (int)data_seg.size, (int)label_count, (int)reloc_count);

    /* Pass 1.5: Register external symbols that were called but not defined */
    {
        size_t i;
        for (i = 0; i < reloc_count; i++) {
            size_t j;
            int found = 0;
            for (j = 0; j < label_count; j++) {
                if (strcmp(labels[j].name, relocs[i].target_name) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                /* Add external target as an undefined global symbol */
                size_t idx;
                if (label_count >= 4096) {
                    fprintf(stderr, "error: assembler label table limit exceeded (max 4096)\n");
                    free(text_seg.data);
                    free(data_seg.data);
                    return -1;
                }
                idx = label_count++;
                strncpy(labels[idx].name, relocs[i].target_name, sizeof(labels[idx].name) - 1);
                labels[idx].name[sizeof(labels[idx].name) - 1] = '\0';
                labels[idx].is_global = 1;
                labels[idx].segment = 0; /* SHN_UNDEF */
                labels[idx].size = 0;
                labels[idx].offset = 0;
            }
        }
    }

    /* Pass 2: Local relocation resolution */
    size_t i;
    for (i = 0; i < reloc_count; i++) {
        size_t j;
        int found = 0;
        for (j = 0; j < label_count; j++) {
            if (strcmp(labels[j].name, relocs[i].target_name) == 0) {
                /* Local target! */
                if (labels[j].segment == 1) {
                    /* Relative offset to local label */
                    long long target_val = labels[j].offset;
                    long long pc = relocs[i].offset + 4;
                    long long disp = target_val - pc;
                    
                    text_seg.data[relocs[i].offset] = disp & 0xFF;
                    text_seg.data[relocs[i].offset + 1] = (disp >> 8) & 0xFF;
                    text_seg.data[relocs[i].offset + 2] = (disp >> 16) & 0xFF;
                    text_seg.data[relocs[i].offset + 3] = (disp >> 24) & 0xFF;
                    
                    /* Relocation is resolved, mark it as NONE so elf_emit ignores it */
                    relocs[i].type = R_X86_64_NONE;
                }
                found = 1;
                break;
            }
        }
    }

    /* Convert assembler symbols and relocations into elf_emit API structures */
    size_t final_sym_count = label_count;
    zcc_elf_sym_t *elf_syms = (zcc_elf_sym_t *)malloc(sizeof(zcc_elf_sym_t) * final_sym_count);
    for (i = 0; i < final_sym_count; i++) {
        elf_syms[i].name = labels[i].name;
        elf_syms[i].value = labels[i].offset;
        elf_syms[i].size = labels[i].size;
        elf_syms[i].binding = labels[i].is_global ? STB_GLOBAL : STB_LOCAL;
        
        if (labels[i].segment == 1) {
            elf_syms[i].type = STT_FUNC;
            elf_syms[i].shndx = 1; /* .text */
        } else if (labels[i].segment == 2) {
            elf_syms[i].type = STT_OBJECT;
            elf_syms[i].shndx = 1; /* now mapped inside .text */
        } else if (labels[i].segment == 3) {
            elf_syms[i].type = STT_OBJECT;
            elf_syms[i].shndx = SHN_COMMON;
        } else {
            elf_syms[i].type = STT_NOTYPE;
            elf_syms[i].shndx = SHN_UNDEF;
        }
    }

    /* Merge data segment into text segment and adjust displacements */
    size_t orig_text_size = text_seg.size;
    if (data_seg.size > 0) {
        /* Pad text to 16 bytes first */
        size_t pad_size = (16 - (text_seg.size % 16)) % 16;
        if (pad_size > 0) {
            unsigned char pad[16] = {0};
            seg_append(&text_seg, pad, pad_size);
        }
        size_t data_start = text_seg.size;
        seg_append(&text_seg, data_seg.data, data_seg.size);
        
        /* Adjust all data labels */
        for (i = 0; i < final_sym_count; i++) {
            if (labels[i].segment == 2) {
                elf_syms[i].value = data_start + labels[i].offset;
                elf_syms[i].shndx = 1;
            }
        }
        
        /* Adjust any relocation that pointed to data segment */
        for (i = 0; i < reloc_count; i++) {
            if (relocs[i].type == R_X86_64_64) {
                relocs[i].offset += data_start;
            }
        }
    }

    /* Filter out NONE relocations */
    size_t final_reloc_count = 0;
    for (i = 0; i < reloc_count; i++) {
        if (relocs[i].type != R_X86_64_NONE) {
            final_reloc_count++;
        }
    }

    zcc_elf_rela_t *elf_relas = NULL;
    if (final_reloc_count > 0) {
        elf_relas = (zcc_elf_rela_t *)malloc(sizeof(zcc_elf_rela_t) * final_reloc_count);
        size_t r_idx = 0;
        for (i = 0; i < reloc_count; i++) {
            if (relocs[i].type != R_X86_64_NONE) {
                elf_relas[r_idx].offset = relocs[i].offset;
                elf_relas[r_idx].sym_name = relocs[i].target_name;
                elf_relas[r_idx].type = relocs[i].type;
                elf_relas[r_idx].addend = relocs[i].addend;
                r_idx++;
            }
        }
    }

    /* Invoke elf_emit_obj to write the ELF relocatable object */
    int ret = elf_emit_obj(out_o_filename, text_seg.data, text_seg.size, elf_syms, final_sym_count, elf_relas, final_reloc_count);

    /* Free allocated resources */
    if (elf_relas) free(elf_relas);
    free(elf_syms);
    free(text_seg.data);
    free(data_seg.data);

    return ret;
}

#ifdef main
#undef main
#endif

int main(int argc, char **argv) {
    int i;
    int emit_obj = 0;
    int o_flag_idx = -1;
    char *out_filename = "a.o";

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-emit-obj") == 0) {
            emit_obj = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                o_flag_idx = i + 1;
                out_filename = argv[i + 1];
            }
        }
    }

    if (emit_obj) {
        /* Intercept compilation! */
        char temp_s_filename[256];
        sprintf(temp_s_filename, "%s.s", out_filename);

        /* Construct modified argv for original zcc_main */
        char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 2));
        int mod_argc = 0;
        for (i = 0; i < argc; i++) {
            if (strcmp(argv[i], "-emit-obj") == 0) {
                /* omit -emit-obj flag */
                continue;
            } else if (i == o_flag_idx) {
                /* change output filename to temp_s_filename */
                mod_argv[mod_argc++] = temp_s_filename;
            } else {
                mod_argv[mod_argc++] = argv[i];
            }
        }
        mod_argv[mod_argc] = NULL;

        /* Call original compiler driver to compile input C to temporary assembly (.s) */
        int compile_ret = zcc_main(mod_argc, mod_argv);
        free(mod_argv);

        if (compile_ret != 0) {
            fprintf(stderr, "zcc: AST-codegen phase failed\n");
            return compile_ret;
        }

        /* Assemble the temporary assembly into the final ELF relocatable object (.o) */
        int assemble_ret = assemble(temp_s_filename, out_filename);
        
        /* Clean up temporary assembly file */
        remove(temp_s_filename);

        if (assemble_ret != 0) {
            fprintf(stderr, "zcc: surgical ELF emission failed with error code %d\n", assemble_ret);
            return assemble_ret;
        }

        return 0;
    } else {
        /* Pass through to standard zcc_main driver execution */
        return zcc_main(argc, argv);
    }
}

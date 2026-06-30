#include "elf_emit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef __GNUC_MINOR__
int __builtin_popcount(unsigned int x) {
    int count = 0;
    while (x) {
        count += (x & 1);
        x >>= 1;
    }
    return count;
}
#endif

/* Declare original main from part5.c */
extern int zcc_main(int argc, char **argv);

/* External POSIX signature and in-memory stream globals */
extern FILE *fmemopen(void *buf, size_t size, const char *mode);
extern char *g_in_mem_asm_buf;
extern size_t g_in_mem_asm_size;
extern int g_use_in_mem_asm;

AssemblerFile g_asm_files[512];
size_t g_asm_file_count = 0;

AssemblerLoc g_asm_locs[131072];
size_t g_asm_loc_count = 0;

int g_current_file_idx = 1;
int g_current_line = 1;
int g_loc_pending = 0;


/* Segment structure */
typedef struct {
    unsigned char *data;
    size_t size;
    size_t cap;
} Segment;

static void seg_append(Segment *seg, const unsigned char *bytes, size_t size) {
    if (seg->size + size > seg->cap) {
        if (!seg->cap) seg->cap = 4096;
        while (seg->size + size > seg->cap) {
            seg->cap *= 2;
        }
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

static Label labels[131072];
static size_t label_count = 0;

/* Relocation tracking */
typedef struct {
    size_t offset;
    char target_name[128];
    int type;
    long long addend;
} Reloc;

static Reloc relocs[131072];
static size_t reloc_count = 0;

static void add_reloc(Reloc r) {
    if (reloc_count >= 131072) {
        fprintf(stderr, "error: assembler relocation table limit exceeded (max 131072)\n");
        exit(1);
    }
    relocs[reloc_count++] = r;
}

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
    if (strcmp(s, "%xmm0") == 0) return 16;
    if (strcmp(s, "%xmm1") == 0) return 17;
    if (strcmp(s, "%xmm2") == 0) return 18;
    if (strcmp(s, "%xmm3") == 0) return 19;
    if (strcmp(s, "%xmm4") == 0) return 20;
    if (strcmp(s, "%xmm5") == 0) return 21;
    if (strcmp(s, "%xmm6") == 0) return 22;
    if (strcmp(s, "%xmm7") == 0) return 23;
    if (strcmp(s, "%xmm8") == 0) return 24;
    if (strcmp(s, "%xmm9") == 0) return 25;
    if (strcmp(s, "%xmm10") == 0) return 26;
    if (strcmp(s, "%xmm11") == 0) return 27;
    if (strcmp(s, "%xmm12") == 0) return 28;
    if (strcmp(s, "%xmm13") == 0) return 29;
    if (strcmp(s, "%xmm14") == 0) return 30;
    if (strcmp(s, "%xmm15") == 0) return 31;
    if (strcmp(s, "%rip") == 0) return 32;
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
        
        if (*reg == -1) {
            fprintf(stderr, "assembler error: invalid base register in memory operand '%s'\n", s);
            exit(1);
        }
        if (*reg == 32) {
            fprintf(stderr, "assembler error: %%rip is not supported as a base register in memory operand '%s' (must be rip-relative only)\n", s);
            exit(1);
        }
        
        if (paren == s) {
            *disp = 0;
        } else {
            char disp_str[64];
            size_t disp_len = paren - s;
            if (disp_len >= 63) disp_len = 63;
            strncpy(disp_str, s, disp_len);
            disp_str[disp_len] = '\0';
            *disp = strtoll(disp_str, NULL, 0);
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
        if ((src_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d = (unsigned char)disp;
        seg_append(seg, &d, 1);
    } else {
        modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        if ((src_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
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

static void encode_movq_imm_mem(Segment *seg, long long imm, int base_reg, long long disp) {
    unsigned char rex = 0x48;
    unsigned char opcode = 0xc7;
    unsigned char modrm;
    if (base_reg & 8) rex |= 0x01;
    
    if (disp == 0 && (base_reg & 7) != 5) {
        modrm = (0x00 << 6) | (0 << 3) | (base_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        if ((base_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
    } else if (disp >= -128 && disp <= 127) {
        modrm = (0x01 << 6) | (0 << 3) | (base_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        if ((base_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d = (unsigned char)disp;
        seg_append(seg, &d, 1);
    } else {
        modrm = (0x02 << 6) | (0 << 3) | (base_reg & 7);
        seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        if ((base_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d[4];
        d[0] = disp & 0xFF;
        d[1] = (disp >> 8) & 0xFF;
        d[2] = (disp >> 16) & 0xFF;
        d[3] = (disp >> 24) & 0xFF;
        seg_append(seg, d, 4);
    }
    unsigned char imm_bytes[4];
    imm_bytes[0] = imm & 0xFF;
    imm_bytes[1] = (imm >> 8) & 0xFF;
    imm_bytes[2] = (imm >> 16) & 0xFF;
    imm_bytes[3] = (imm >> 24) & 0xFF;
    seg_append(seg, imm_bytes, 4);
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
    add_reloc(r);
    
    unsigned char zero[4] = {0, 0, 0, 0};
    seg_append(seg, zero, 4);
}

static void encode_jump(Segment *seg, const char *mnemonic, const char *target_label) {
    unsigned char bytes[2];
    size_t size = 0;
    
    if (strcmp(mnemonic, "jmp") == 0) {
        bytes[0] = 0xe9;
        size = 1;
    } else if (strcmp(mnemonic, "je") == 0 || strcmp(mnemonic, "jz") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x84; size = 2;
    } else if (strcmp(mnemonic, "jne") == 0 || strcmp(mnemonic, "jnz") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x85; size = 2;
    } else if (strcmp(mnemonic, "jl") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8c; size = 2;
    } else if (strcmp(mnemonic, "jle") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8e; size = 2;
    } else if (strcmp(mnemonic, "jg") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8f; size = 2;
    } else if (strcmp(mnemonic, "jge") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x8d; size = 2;
    } else if (strcmp(mnemonic, "jb") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x82; size = 2;
    } else if (strcmp(mnemonic, "jbe") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x86; size = 2;
    } else if (strcmp(mnemonic, "ja") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x87; size = 2;
    } else if (strcmp(mnemonic, "jae") == 0) {
        bytes[0] = 0x0f; bytes[1] = 0x83; size = 2;
    }
    
    if (size > 0) {
        seg_append(seg, bytes, size);
        
        Reloc r;
        r.offset = seg->size;
        strcpy(r.target_name, target_label);
        r.type = R_X86_64_PC32;
        r.addend = -4;
        add_reloc(r);
        
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
    add_reloc(r);
    
    unsigned char zero[4] = {0, 0, 0, 0};
    seg_append(seg, zero, 4);
}

/* CG-FNPTR-001 fix: register-indirect CALL via FF /2 (ModRM reg)
 * Encodes: call *%rN  →  [REX.B] 0xFF 0xD0|(N&7)
 * No relocation entry — address is in the register at runtime. */
static void encode_call_indirect_reg(Segment *seg, int reg) {
    if (reg & 8) {
        /* REX.B needed for r8-r15 */
        unsigned char rex = 0x41;
        seg_append(seg, &rex, 1);
    }
    unsigned char ff  = 0xff;
    unsigned char mrm = 0xd0 | (reg & 7); /* ModRM: mod=11, reg=2 (/2), rm=reg */
    seg_append(seg, &ff,  1);
    seg_append(seg, &mrm, 1);
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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

static void encode_movl_imm_mem(Segment *seg, long long imm, int base_reg, long long disp) {
    unsigned char rex = 0x40;
    unsigned char opcode = 0xc7;
    unsigned char modrm;
    if (base_reg & 8) rex |= 0x01;
    
    if (disp == 0 && (base_reg & 7) != 5) {
        modrm = (0x00 << 6) | (0 << 3) | (base_reg & 7);
        if (rex != 0x40) seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        if ((base_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
    } else if (disp >= -128 && disp <= 127) {
        modrm = (0x01 << 6) | (0 << 3) | (base_reg & 7);
        if (rex != 0x40) seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        if ((base_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d = (unsigned char)disp;
        seg_append(seg, &d, 1);
    } else {
        modrm = (0x02 << 6) | (0 << 3) | (base_reg & 7);
        if (rex != 0x40) seg_append(seg, &rex, 1);
        seg_append(seg, &opcode, 1);
        seg_append(seg, &modrm, 1);
        if ((base_reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d[4];
        d[0] = disp & 0xFF;
        d[1] = (disp >> 8) & 0xFF;
        d[2] = (disp >> 16) & 0xFF;
        d[3] = (disp >> 24) & 0xFF;
        seg_append(seg, d, 4);
    }
    unsigned char imm_bytes[4];
    imm_bytes[0] = imm & 0xFF;
    imm_bytes[1] = (imm >> 8) & 0xFF;
    imm_bytes[2] = (imm >> 16) & 0xFF;
    imm_bytes[3] = (imm >> 24) & 0xFF;
    seg_append(seg, imm_bytes, 4);
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
    unsigned char modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
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
    else if (strcmp(mnemonic, "setle") == 0) bytes[1] = 0x9e;
    else if (strcmp(mnemonic, "setg") == 0) bytes[1] = 0x9f;
    else if (strcmp(mnemonic, "setge") == 0) bytes[1] = 0x9d;
    else if (strcmp(mnemonic, "setb") == 0) bytes[1] = 0x92;
    else if (strcmp(mnemonic, "setbe") == 0) bytes[1] = 0x96;
    else if (strcmp(mnemonic, "seta") == 0) bytes[1] = 0x97;
    else if (strcmp(mnemonic, "setae") == 0) bytes[1] = 0x93;
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40 || dst_reg >= 4 || src_reg >= 4) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40 || src_reg >= 4 || dst_reg >= 4) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, &opcode, 1);
            seg_append(seg, &modrm, 1);
            if ((dst_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            if (rex != 0x40) seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
            unsigned char d = (unsigned char)disp;
            seg_append(seg, &d, 1);
        } else {
            modrm = (0x02 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
            seg_append(seg, &rex, 1);
            seg_append(seg, opcode, 2);
            seg_append(seg, &modrm, 1);
            if ((src_reg & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(seg, &sib, 1);
            }
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

static int is_reg_64(const char *s) {
    if (s[0] == '%') {
        if (s[1] == 'r') {
            int len = strlen(s);
            if (s[len-1] == 'd' || s[len-1] == 'w' || s[len-1] == 'b') return 0;
            return 1;
        }
    }
    return 0;
}

static void encode_sse_binop(Segment *seg, const char *prefix_bytes, int num_prefixes, unsigned char opcode, int reg_src, int reg_dst) {
    int i;
    for (i = 0; i < num_prefixes; i++) {
        unsigned char p = prefix_bytes[i];
        seg_append(seg, &p, 1);
    }
    
    int hw_src = reg_src & 15;
    int hw_dst = reg_dst & 15;
    
    unsigned char rex = 0x40;
    int need_rex = 0;
    if (hw_src & 8) {
        rex |= 0x01;
        need_rex = 1;
    }
    if (hw_dst & 8) {
        rex |= 0x04;
        need_rex = 1;
    }
    if (need_rex) {
        seg_append(seg, &rex, 1);
    }
    
    unsigned char escape = 0x0f;
    seg_append(seg, &escape, 1);
    seg_append(seg, &opcode, 1);
    
    unsigned char modrm = 0xc0 | ((hw_dst & 7) << 3) | (hw_src & 7);
    seg_append(seg, &modrm, 1);
}

static void encode_sse_mem(Segment *seg, const char *prefix_bytes, int num_prefixes, unsigned char opcode, int reg_gp, int reg_xmm, int is_load, long long disp) {
    int i;
    for (i = 0; i < num_prefixes; i++) {
        unsigned char p = prefix_bytes[i];
        seg_append(seg, &p, 1);
    }
    
    int hw_gp = reg_gp & 15;
    int hw_xmm = reg_xmm & 15;
    
    unsigned char rex = 0x40;
    int need_rex = 0;
    if (hw_gp & 8) {
        rex |= 0x01;
        need_rex = 1;
    }
    if (hw_xmm & 8) {
        rex |= 0x04;
        need_rex = 1;
    }
    if (need_rex) {
        seg_append(seg, &rex, 1);
    }
    
    unsigned char escape = 0x0f;
    seg_append(seg, &escape, 1);
    seg_append(seg, &opcode, 1);
    
    unsigned char modrm;
    if (disp >= -128 && disp <= 127) {
        modrm = (0x01 << 6) | ((hw_xmm & 7) << 3) | (hw_gp & 7);
        seg_append(seg, &modrm, 1);
        if ((hw_gp & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d = (unsigned char)disp;
        seg_append(seg, &d, 1);
    } else {
        modrm = (0x02 << 6) | ((hw_xmm & 7) << 3) | (hw_gp & 7);
        seg_append(seg, &modrm, 1);
        if ((hw_gp & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d[4];
        d[0] = disp & 0xFF;
        d[1] = (disp >> 8) & 0xFF;
        d[2] = (disp >> 16) & 0xFF;
        d[3] = (disp >> 24) & 0xFF;
        seg_append(seg, d, 4);
    }
}

static void encode_sse_rip(Segment *seg, const char *prefix_bytes, int num_prefixes, unsigned char opcode, int reg_xmm, const char *label_name) {
    int i;
    for (i = 0; i < num_prefixes; i++) {
        unsigned char p = prefix_bytes[i];
        seg_append(seg, &p, 1);
    }
    
    int hw_xmm = reg_xmm & 15;
    unsigned char rex = 0x40;
    int need_rex = 0;
    if (hw_xmm & 8) {
        rex |= 0x04;
        need_rex = 1;
    }
    if (need_rex) {
        seg_append(seg, &rex, 1);
    }
    
    unsigned char escape = 0x0f;
    seg_append(seg, &escape, 1);
    seg_append(seg, &opcode, 1);
    
    unsigned char modrm = 0x05 | ((hw_xmm & 7) << 3);
    seg_append(seg, &modrm, 1);
    
    Reloc r;
    r.offset = seg->size;
    strcpy(r.target_name, label_name);
    r.type = R_X86_64_PC32;
    r.addend = -4;
    add_reloc(r);
    
    unsigned char zero[4] = {0, 0, 0, 0};
    seg_append(seg, zero, 4);
}

static void encode_movq_xmm(Segment *seg, int reg1, int reg2) {
    unsigned char p66 = 0x66;
    seg_append(seg, &p66, 1);
    
    unsigned char rex = 0x48;
    int hw1 = reg1 & 15;
    int hw2 = reg2 & 15;
    
    if (reg1 < 16 && reg2 >= 16) {
        if (hw1 & 8) rex |= 0x01;
        if (hw2 & 8) rex |= 0x04;
        seg_append(seg, &rex, 1);
        unsigned char op[2] = {0x0f, 0x6e};
        seg_append(seg, op, 2);
        unsigned char modrm = 0xc0 | ((hw2 & 7) << 3) | (hw1 & 7);
        seg_append(seg, &modrm, 1);
    } else if (reg1 >= 16 && reg2 < 16) {
        if (hw1 & 8) rex |= 0x01;
        if (hw2 & 8) rex |= 0x04;
        seg_append(seg, &rex, 1);
        unsigned char op[2] = {0x0f, 0x7e};
        seg_append(seg, op, 2);
        unsigned char modrm = 0xc0 | ((hw2 & 7) << 3) | (hw1 & 7);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_movd_xmm(Segment *seg, int reg1, int reg2) {
    unsigned char p66 = 0x66;
    seg_append(seg, &p66, 1);
    
    unsigned char rex = 0x40;
    int hw1 = reg1 & 15;
    int hw2 = reg2 & 15;
    int need_rex = 0;
    
    if (reg1 < 16 && reg2 >= 16) {
        if (hw1 & 8) { rex |= 0x01; need_rex = 1; }
        if (hw2 & 8) { rex |= 0x04; need_rex = 1; }
        if (need_rex) seg_append(seg, &rex, 1);
        unsigned char op[2] = {0x0f, 0x6e};
        seg_append(seg, op, 2);
        unsigned char modrm = 0xc0 | ((hw2 & 7) << 3) | (hw1 & 7);
        seg_append(seg, &modrm, 1);
    } else if (reg1 >= 16 && reg2 < 16) {
        if (hw1 & 8) { rex |= 0x01; need_rex = 1; }
        if (hw2 & 8) { rex |= 0x04; need_rex = 1; }
        if (need_rex) seg_append(seg, &rex, 1);
        unsigned char op[2] = {0x0f, 0x7e};
        seg_append(seg, op, 2);
        unsigned char modrm = 0xc0 | ((hw2 & 7) << 3) | (hw1 & 7);
        seg_append(seg, &modrm, 1);
    }
}

static void encode_cvtt_to_si(Segment *seg, const char *pref, int n_pref, unsigned char opcode, int reg_xmm, int reg_gp, int use_rex_w) {
    int i;
    for (i = 0; i < n_pref; i++) {
        unsigned char p = pref[i];
        seg_append(seg, &p, 1);
    }
    
    int hw_gp = reg_gp & 15;
    int hw_xmm = reg_xmm & 15;
    
    unsigned char rex = use_rex_w ? 0x48 : 0x40;
    int need_rex = use_rex_w;
    
    if (hw_xmm & 8) {
        rex |= 0x01;
        need_rex = 1;
    }
    if (hw_gp & 8) {
        rex |= 0x04;
        need_rex = 1;
    }
    
    if (need_rex) {
        seg_append(seg, &rex, 1);
    }
    
    unsigned char escape = 0x0f;
    seg_append(seg, &escape, 1);
    seg_append(seg, &opcode, 1);
    
    unsigned char modrm = 0xc0 | ((hw_gp & 7) << 3) | (hw_xmm & 7);
    seg_append(seg, &modrm, 1);
}

static void encode_cvtt_to_si_mem(Segment *seg, const char *pref, int n_pref, unsigned char opcode, int reg_gp_base, int reg_gp_dst, int use_rex_w, long long disp) {
    int i;
    for (i = 0; i < n_pref; i++) {
        unsigned char p = pref[i];
        seg_append(seg, &p, 1);
    }
    
    int hw_gp_base = reg_gp_base & 15;
    int hw_gp_dst = reg_gp_dst & 15;
    
    unsigned char rex = use_rex_w ? 0x48 : 0x40;
    int need_rex = use_rex_w;
    
    if (hw_gp_base & 8) {
        rex |= 0x01;
        need_rex = 1;
    }
    if (hw_gp_dst & 8) {
        rex |= 0x04;
        need_rex = 1;
    }
    
    if (need_rex) {
        seg_append(seg, &rex, 1);
    }
    
    unsigned char escape = 0x0f;
    seg_append(seg, &escape, 1);
    seg_append(seg, &opcode, 1);
    
    unsigned char modrm;
    if (disp >= -128 && disp <= 127) {
        modrm = (0x01 << 6) | ((hw_gp_dst & 7) << 3) | (hw_gp_base & 7);
        seg_append(seg, &modrm, 1);
        if ((hw_gp_base & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d = (unsigned char)disp;
        seg_append(seg, &d, 1);
    } else {
        modrm = (0x02 << 6) | ((hw_gp_dst & 7) << 3) | (hw_gp_base & 7);
        seg_append(seg, &modrm, 1);
        if ((hw_gp_base & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d[4];
        d[0] = disp & 0xFF;
        d[1] = (disp >> 8) & 0xFF;
        d[2] = (disp >> 16) & 0xFF;
        d[3] = (disp >> 24) & 0xFF;
        seg_append(seg, d, 4);
    }
}

static void encode_cvtsi_to_xmm(Segment *seg, const char *pref, int n_pref, unsigned char opcode, int reg_gp, int reg_xmm, int use_rex_w) {
    int i;
    for (i = 0; i < n_pref; i++) {
        unsigned char p = pref[i];
        seg_append(seg, &p, 1);
    }
    
    int hw_gp = reg_gp & 15;
    int hw_xmm = reg_xmm & 15;
    
    unsigned char rex = use_rex_w ? 0x48 : 0x40;
    int need_rex = use_rex_w;
    
    if (hw_gp & 8) {
        rex |= 0x01;
        need_rex = 1;
    }
    if (hw_xmm & 8) {
        rex |= 0x04;
        need_rex = 1;
    }
    
    if (need_rex) {
        seg_append(seg, &rex, 1);
    }
    
    unsigned char escape = 0x0f;
    seg_append(seg, &escape, 1);
    seg_append(seg, &opcode, 1);
    
    unsigned char modrm = 0xc0 | ((hw_xmm & 7) << 3) | (hw_gp & 7);
    seg_append(seg, &modrm, 1);
}

static void encode_cvtsi_to_xmm_mem(Segment *seg, const char *pref, int n_pref, unsigned char opcode, int reg_gp_base, int reg_xmm, int use_rex_w, long long disp) {
    int i;
    for (i = 0; i < n_pref; i++) {
        unsigned char p = pref[i];
        seg_append(seg, &p, 1);
    }
    
    int hw_gp_base = reg_gp_base & 15;
    int hw_xmm = reg_xmm & 15;
    
    unsigned char rex = use_rex_w ? 0x48 : 0x40;
    int need_rex = use_rex_w;
    
    if (hw_gp_base & 8) {
        rex |= 0x01;
        need_rex = 1;
    }
    if (hw_xmm & 8) {
        rex |= 0x04;
        need_rex = 1;
    }
    
    if (need_rex) {
        seg_append(seg, &rex, 1);
    }
    
    unsigned char escape = 0x0f;
    seg_append(seg, &escape, 1);
    seg_append(seg, &opcode, 1);
    
    unsigned char modrm;
    if (disp >= -128 && disp <= 127) {
        modrm = (0x01 << 6) | ((hw_xmm & 7) << 3) | (hw_gp_base & 7);
        seg_append(seg, &modrm, 1);
        if ((hw_gp_base & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d = (unsigned char)disp;
        seg_append(seg, &d, 1);
    } else {
        modrm = (0x02 << 6) | ((hw_xmm & 7) << 3) | (hw_gp_base & 7);
        seg_append(seg, &modrm, 1);
        if ((hw_gp_base & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(seg, &sib, 1);
        }
        unsigned char d[4];
        d[0] = disp & 0xFF;
        d[1] = (disp >> 8) & 0xFF;
        d[2] = (disp >> 16) & 0xFF;
        d[3] = (disp >> 24) & 0xFF;
        seg_append(seg, d, 4);
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

static char **g_direct_asm_lines = NULL;
static size_t g_direct_asm_line_count = 0;
static size_t g_direct_asm_line_cap = 0;

void zcc_direct_assemble_line(const char *line) {
    if (g_direct_asm_line_count >= g_direct_asm_line_cap) {
        g_direct_asm_line_cap = g_direct_asm_line_cap ? g_direct_asm_line_cap * 2 : 1024;
        g_direct_asm_lines = (char **)realloc(g_direct_asm_lines, g_direct_asm_line_cap * sizeof(char *));
    }
    g_direct_asm_lines[g_direct_asm_line_count++] = strdup(line);
}

/* Two-Pass Assembler Core */
static int assemble(const char *in_s_filename, const char *out_o_filename, const char *in_mem_buf, size_t mem_buf_len) {
    FILE *in = NULL;
    if (!g_direct_asm_lines) {
        if (in_mem_buf) {
            in = fmemopen((void *)in_mem_buf, mem_buf_len, "r");
        } else if (in_s_filename) {
            in = fopen(in_s_filename, "r");
        }
        if (!in) {
            return -1;
        }
    }
    Segment text_seg = { NULL, 0, 0 };
    Segment data_seg = { NULL, 0, 0 };
    char line[65536];
    int current_segment = 1; /* 1 = .text, 2 = .data */

    label_count = 0;
    reloc_count = 0;
    g_asm_file_count = 0;
    g_asm_loc_count = 0;
    g_current_file_idx = 1;
    g_current_line = 1;
    g_loc_pending = 0;

    /* Pass 1: Parse and encode */
    size_t line_idx = 0;
    while (1) {
        if (g_direct_asm_lines && line_idx < g_direct_asm_line_count) {
            strncpy(line, g_direct_asm_lines[line_idx++], sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
        } else if (in) {
            if (!fgets(line, sizeof(line), in)) break;
        } else {
            break;
        }
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
                if (label_count >= 131072) {
                    fprintf(stderr, "error: assembler label table limit exceeded (max 131072)\n");
                    exit(1);
                }
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
            } else if (strcmp(dir, ".data") == 0 || strcmp(dir, ".section") == 0 || strcmp(dir, ".rodata") == 0 || strcmp(dir, ".bss") == 0) {
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
                    if (label_count >= 131072) {
                        fprintf(stderr, "error: assembler label table limit exceeded (max 131072)\n");
                        exit(1);
                    }
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
                    if (label_count >= 131072) {
                        fprintf(stderr, "error: assembler label table limit exceeded (max 131072)\n");
                        exit(1);
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
                    add_reloc(r);
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
            } else if (strcmp(dir, ".file") == 0) {
                int idx = 0;
                char path[512];
                char *arg = p + 5;
                if (sscanf(arg, "%d", &idx) == 1) {
                    char *quote = strchr(arg, '"');
                    if (quote) {
                        char *end_quote = strchr(quote + 1, '"');
                        if (end_quote) {
                            *end_quote = '\0';
                            strcpy(path, quote + 1);
                            if (g_asm_file_count < 512) {
                                g_asm_files[g_asm_file_count].file_idx = idx;
                                strcpy(g_asm_files[g_asm_file_count].path, path);
                                g_asm_file_count++;
                            }
                        }
                    }
                }
            } else if (strcmp(dir, ".loc") == 0) {
                int f_idx = 0;
                int l_no = 0;
                if (sscanf(p + 4, "%d %d", &f_idx, &l_no) == 2) {
                    g_current_file_idx = f_idx;
                    g_current_line = l_no;
                    g_loc_pending = 1;
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
        if (g_loc_pending) {
            if (g_asm_loc_count < 131072) {
                g_asm_locs[g_asm_loc_count].address = text_seg.size;
                g_asm_locs[g_asm_loc_count].file_idx = g_current_file_idx;
                g_asm_locs[g_asm_loc_count].line = g_current_line;
                g_asm_loc_count++;
            }
            g_loc_pending = 0;
        }

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
        int matched = 0;
        /* 0-operand instructions */
        if (strcmp(mnemonic, "ret") == 0) {
            unsigned char b = 0xc3;
            seg_append(&text_seg, &b, 1);
            matched = 1;
        } else if (strcmp(mnemonic, "leave") == 0) {
            unsigned char b = 0xc9;
            seg_append(&text_seg, &b, 1);
            matched = 1;
        } else if (strcmp(mnemonic, "rep") == 0 && args_start && strcmp(trim(args_start), "movsb") == 0) {
            unsigned char b[2] = {0xf3, 0xa4};
            seg_append(&text_seg, b, 2);
            matched = 1;
        } else if (strcmp(mnemonic, "cqto") == 0 || strcmp(mnemonic, "cqo") == 0) {
            unsigned char b[2] = {0x48, 0x99};
            seg_append(&text_seg, b, 2);
            matched = 1;
        } else if (strcmp(mnemonic, "cltd") == 0 || strcmp(mnemonic, "cdq") == 0) {
            unsigned char b = 0x99;
            seg_append(&text_seg, &b, 1);
            matched = 1;
        } else if (strcmp(mnemonic, "hlt") == 0) {
            unsigned char b = 0xf4;
            seg_append(&text_seg, &b, 1);
            matched = 1;
        } else if (strcmp(mnemonic, "cltq") == 0) {
            unsigned char b[2] = {0x48, 0x98};
            seg_append(&text_seg, b, 2);
            matched = 1;
        }
        /* 1-operand instructions */
        else if (strcmp(mnemonic, "pushq") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int is_mem = 0;
            int reg = -1;
            long long disp = 0;
            if (parse_mem_operand(op, &reg, &disp)) {
                is_mem = 1;
            } else {
                reg = parse_reg(op);
            }
            if (reg < 0) {
                fprintf(stderr, "assembler error: invalid register/operand '%s' in pushq\n", op);
                exit(1);
            }
            if (is_mem) {
                unsigned char rex = 0x40;
                if (reg & 8) rex |= 0x01; /* REX.B */
                unsigned char opcode = 0xff;
                unsigned char modrm;
                if (disp >= -128 && disp <= 127) {
                    modrm = (0x01 << 6) | (6 << 3) | (reg & 7);
                    if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                    seg_append(&text_seg, &modrm, 1);
                    if ((reg & 7) == 4) { unsigned char sib = 0x24; seg_append(&text_seg, &sib, 1); }
                    unsigned char d = (unsigned char)disp;
                    seg_append(&text_seg, &d, 1);
                } else {
                    modrm = (0x02 << 6) | (6 << 3) | (reg & 7);
                    if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                    seg_append(&text_seg, &modrm, 1);
                    if ((reg & 7) == 4) { unsigned char sib = 0x24; seg_append(&text_seg, &sib, 1); }
                    unsigned char d[4];
                    d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
                    d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
                    seg_append(&text_seg, d, 4);
                }
            } else {
                if (reg == 5) {
                    unsigned char b = 0x55;
                    seg_append(&text_seg, &b, 1);
                } else {
                    unsigned char rex = 0x40 | (reg >> 3);
                    unsigned char opcode = 0x50 | (reg & 7);
                    if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                }
            }
            matched = 1;
        } else if (strcmp(mnemonic, "popq") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int is_mem = 0;
            int reg = -1;
            long long disp = 0;
            if (parse_mem_operand(op, &reg, &disp)) {
                is_mem = 1;
            } else {
                reg = parse_reg(op);
            }
            if (reg < 0) {
                fprintf(stderr, "assembler error: invalid register/operand '%s' in popq\n", op);
                exit(1);
            }
            if (is_mem) {
                unsigned char rex = 0x40;
                if (reg & 8) rex |= 0x01; /* REX.B */
                unsigned char opcode = 0x8f;
                unsigned char modrm;
                if (disp >= -128 && disp <= 127) {
                    modrm = (0x01 << 6) | (0 << 3) | (reg & 7);
                    if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                    seg_append(&text_seg, &modrm, 1);
                    if ((reg & 7) == 4) { unsigned char sib = 0x24; seg_append(&text_seg, &sib, 1); }
                    unsigned char d = (unsigned char)disp;
                    seg_append(&text_seg, &d, 1);
                } else {
                    modrm = (0x02 << 6) | (0 << 3) | (reg & 7);
                    if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                    seg_append(&text_seg, &modrm, 1);
                    if ((reg & 7) == 4) { unsigned char sib = 0x24; seg_append(&text_seg, &sib, 1); }
                    unsigned char d[4];
                    d[0] = disp & 0xFF; d[1] = (disp >> 8) & 0xFF;
                    d[2] = (disp >> 16) & 0xFF; d[3] = (disp >> 24) & 0xFF;
                    seg_append(&text_seg, d, 4);
                }
            } else {
                if (reg == 5) {
                    unsigned char b = 0x5d;
                    seg_append(&text_seg, &b, 1);
                } else {
                    unsigned char rex = 0x40 | (reg >> 3);
                    unsigned char opcode = 0x58 | (reg & 7);
                    if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                }
            }
            matched = 1;
        } else if (strcmp(mnemonic, "call") == 0 || strcmp(mnemonic, "callq") == 0) {
            char *target = args_start ? trim(args_start) : "";
            /* CG-FNPTR-001: detect register-indirect call "call *%rN"
             * These need FF/2 ModRM encoding, not a PLT32 relocation. */
            if (target[0] == '*') {
                int ireg = parse_reg(target + 1); /* skip '*', parse %rN */
                if (ireg == -1) {
                    fprintf(stderr, "assembler error: invalid register '%s' in indirect call\n", target);
                    exit(1);
                }
                encode_call_indirect_reg(&text_seg, ireg);
            } else {
                encode_call(&text_seg, target);
            }
            matched = 1;
        } else if (mnemonic[0] == 'j') {
            char *target = args_start ? trim(args_start) : "";
            encode_jump(&text_seg, mnemonic, target);
            matched = 1;
        } else if (strncmp(mnemonic, "set", 3) == 0 && strlen(mnemonic) <= 5) {
            encode_set(&text_seg, mnemonic);
            matched = 1;
        } else if (strcmp(mnemonic, "idivq") == 0 || strcmp(mnemonic, "idivl") == 0 ||
                   strcmp(mnemonic, "idivw") == 0 || strcmp(mnemonic, "idivb") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            if (reg < 0) {
                fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op, mnemonic);
                exit(1);
            }
            char size_char = mnemonic[4];
            unsigned char prefix = 0;
            unsigned char rex = 0;
            unsigned char opcode = 0;
            unsigned char modrm = 0xf8 | (reg & 7);
            
            if (size_char == 'w') {
                prefix = 0x66;
            }
            
            if (size_char == 'q') {
                rex = 0x48;
            } else if (reg & 8) {
                rex = 0x40;
            } else if (size_char == 'b' && (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
                rex = 0x40;
            }
            
            if (reg & 8) rex |= 0x01;
            
            if (size_char == 'b') {
                opcode = 0xf6;
            } else {
                opcode = 0xf7;
            }
            
            if (prefix) seg_append(&text_seg, &prefix, 1);
            if (rex) seg_append(&text_seg, &rex, 1);
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
            matched = 1;
        } else if (strcmp(mnemonic, "divq") == 0 || strcmp(mnemonic, "divl") == 0 ||
                   strcmp(mnemonic, "divw") == 0 || strcmp(mnemonic, "divb") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            if (reg < 0) {
                fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op, mnemonic);
                exit(1);
            }
            char size_char = mnemonic[3];
            unsigned char prefix = 0;
            unsigned char rex = 0;
            unsigned char opcode = 0;
            unsigned char modrm = 0xf0 | (reg & 7);
            
            if (size_char == 'w') {
                prefix = 0x66;
            }
            
            if (size_char == 'q') {
                rex = 0x48;
            } else if (reg & 8) {
                rex = 0x40;
            } else if (size_char == 'b' && (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
                rex = 0x40;
            }
            
            if (reg & 8) rex |= 0x01;
            
            if (size_char == 'b') {
                opcode = 0xf6;
            } else {
                opcode = 0xf7;
            }
            
            if (prefix) seg_append(&text_seg, &prefix, 1);
            if (rex) seg_append(&text_seg, &rex, 1);
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
            matched = 1;
        } else if (strcmp(mnemonic, "negq") == 0 || strcmp(mnemonic, "negl") == 0 ||
                   strcmp(mnemonic, "negw") == 0 || strcmp(mnemonic, "negb") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            if (reg < 0) {
                fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op, mnemonic);
                exit(1);
            }
            char size_char = mnemonic[3];
            unsigned char prefix = 0;
            unsigned char rex = 0;
            unsigned char opcode = 0;
            unsigned char modrm = 0xd8 | (reg & 7);
            
            if (size_char == 'w') {
                prefix = 0x66;
            }
            
            if (size_char == 'q') {
                rex = 0x48;
            } else if (reg & 8) {
                rex = 0x40;
            } else if (size_char == 'b' && (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
                rex = 0x40;
            }
            
            if (reg & 8) rex |= 0x01;
            
            if (size_char == 'b') {
                opcode = 0xf6;
            } else {
                opcode = 0xf7;
            }
            
            if (prefix) seg_append(&text_seg, &prefix, 1);
            if (rex) seg_append(&text_seg, &rex, 1);
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
            matched = 1;
        } else if (strcmp(mnemonic, "notq") == 0 || strcmp(mnemonic, "notl") == 0 ||
                   strcmp(mnemonic, "notw") == 0 || strcmp(mnemonic, "notb") == 0) {
            char *op = args_start ? trim(args_start) : "";
            int reg = parse_reg(op);
            if (reg < 0) {
                fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op, mnemonic);
                exit(1);
            }
            char size_char = mnemonic[3];
            unsigned char prefix = 0;
            unsigned char rex = 0;
            unsigned char opcode = 0;
            unsigned char modrm = 0xd0 | (reg & 7);
            
            if (size_char == 'w') {
                prefix = 0x66;
            }
            
            if (size_char == 'q') {
                rex = 0x48;
            } else if (reg & 8) {
                rex = 0x40;
            } else if (size_char == 'b' && (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
                rex = 0x40;
            }
            
            if (reg & 8) rex |= 0x01;
            
            if (size_char == 'b') {
                opcode = 0xf6;
            } else {
                opcode = 0xf7;
            }
            
            if (prefix) seg_append(&text_seg, &prefix, 1);
            if (rex) seg_append(&text_seg, &rex, 1);
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
            matched = 1;
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
                
                if (op1[0] == '%' && reg1 == -1) {
                    fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op1, mnemonic);
                    exit(1);
                }
                if (op2[0] == '%' && reg2 == -1) {
                    fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op2, mnemonic);
                    exit(1);
                }
                
                if (strcmp(mnemonic, "movq") == 0) {
                    if ((reg1 >= 16 && reg1 <= 31) || (reg2 >= 16 && reg2 <= 31)) {
                        int is_mem1 = 0, is_mem2 = 0;
                        long long disp1 = 0, disp2 = 0;
                        if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                        if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                        
                        if (is_mem1) {
                            encode_sse_mem(&text_seg, "\xF3", 1, 0x7E, reg1, reg2, 1, disp1);
                        } else if (is_mem2) {
                            encode_sse_mem(&text_seg, "\x66", 1, 0xD6, reg2, reg1, 0, disp2);
                        } else {
                            if (reg1 < 16 && reg2 >= 16) {
                                encode_movq_xmm(&text_seg, reg1, reg2);
                            } else if (reg1 >= 16 && reg2 < 16) {
                                encode_movq_xmm(&text_seg, reg1, reg2);
                            } else {
                                encode_sse_binop(&text_seg, "\xF3", 1, 0x7E, reg1, reg2);
                            }
                        }
                    } else if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        int reg2_base = 0;
                        long long disp2 = 0;
                        if (parse_mem_operand(op2, &reg2_base, &disp2)) {
                            encode_movq_imm_mem(&text_seg, imm, reg2_base, disp2);
                        } else {
                            encode_movq_imm(&text_seg, imm, reg2);
                        }
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
                    matched = 1;
                } else if (strcmp(mnemonic, "movabsq") == 0 || strcmp(mnemonic, "movabs") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        unsigned char rex = 0x48;
                        if (reg2 & 8) rex |= 0x01;
                        unsigned char opcode = 0xb8 | (reg2 & 7);
                        seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        unsigned char b[8];
                        b[0] = imm & 0xFF;
                        b[1] = (imm >> 8) & 0xFF;
                        b[2] = (imm >> 16) & 0xFF;
                        b[3] = (imm >> 24) & 0xFF;
                        b[4] = (imm >> 32) & 0xFF;
                        b[5] = (imm >> 40) & 0xFF;
                        b[6] = (imm >> 48) & 0xFF;
                        b[7] = (imm >> 56) & 0xFF;
                        seg_append(&text_seg, b, 8);
                    } else {
                        fprintf(stderr, "assembler error: movabsq requires an immediate source operand\n");
                        exit(1);
                    }
                    matched = 1;
                } else if (strcmp(mnemonic, "movd") == 0) {
                    if ((reg1 >= 16 && reg1 <= 31) || (reg2 >= 16 && reg2 <= 31)) {
                        int is_mem1 = 0, is_mem2 = 0;
                        long long disp1 = 0, disp2 = 0;
                        if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                        if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                        
                        if (is_mem1) {
                            encode_sse_mem(&text_seg, "\x66", 1, 0x6E, reg1, reg2, 1, disp1);
                        } else if (is_mem2) {
                            encode_sse_mem(&text_seg, "\x66", 1, 0x7E, reg2, reg1, 0, disp2);
                        } else {
                            encode_movd_xmm(&text_seg, reg1, reg2);
                        }
                    } else {
                        fprintf(stderr, "assembler error: movd requires an xmm register\n");
                        exit(1);
                    }
                    matched = 1;
                } else if (strcmp(mnemonic, "movss") == 0 || strcmp(mnemonic, "movsd") == 0) {
                    const char *pref = (strcmp(mnemonic, "movss") == 0) ? "\xF3" : "\xF2";
                    if (strstr(op1, "(%rip)")) {
                        char lbl[128];
                        char *rip = strstr(op1, "(%rip)");
                        *rip = '\0';
                        strcpy(lbl, trim(op1));
                        encode_sse_rip(&text_seg, pref, 1, 0x10, reg2, lbl);
                    } else if (strstr(op2, "(%rip)")) {
                        char lbl[128];
                        char *rip = strstr(op2, "(%rip)");
                        *rip = '\0';
                        strcpy(lbl, trim(op2));
                        encode_sse_rip(&text_seg, pref, 1, 0x11, reg1, lbl);
                    } else {
                        int is_mem1 = 0, is_mem2 = 0;
                        long long disp1 = 0, disp2 = 0;
                        if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                        if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                        
                        if (is_mem1) {
                            encode_sse_mem(&text_seg, pref, 1, 0x10, reg1, reg2, 1, disp1);
                        } else if (is_mem2) {
                            encode_sse_mem(&text_seg, pref, 1, 0x11, reg2, reg1, 0, disp2);
                        } else {
                            encode_sse_binop(&text_seg, pref, 1, 0x10, reg1, reg2);
                        }
                    }
                } else if (strcmp(mnemonic, "movaps") == 0) {
                    if (strstr(op1, "(%rip)")) {
                        char lbl[128];
                        char *rip = strstr(op1, "(%rip)");
                        *rip = '\0';
                        strcpy(lbl, trim(op1));
                        encode_sse_rip(&text_seg, "", 0, 0x28, reg2, lbl);
                    } else if (strstr(op2, "(%rip)")) {
                        char lbl[128];
                        char *rip = strstr(op2, "(%rip)");
                        *rip = '\0';
                        strcpy(lbl, trim(op2));
                        encode_sse_rip(&text_seg, "", 0, 0x29, reg1, lbl);
                    } else {
                        int is_mem1 = 0, is_mem2 = 0;
                        long long disp1 = 0, disp2 = 0;
                        if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                        if (parse_mem_operand(op2, &reg2, &disp2)) is_mem2 = 1;
                        
                        if (is_mem1) {
                            encode_sse_mem(&text_seg, "", 0, 0x28, reg1, reg2, 1, disp1);
                        } else if (is_mem2) {
                            encode_sse_mem(&text_seg, "", 0, 0x29, reg2, reg1, 0, disp2);
                        } else {
                            encode_sse_binop(&text_seg, "", 0, 0x28, reg1, reg2);
                        }
                    }
                } else if (strcmp(mnemonic, "movl") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        int reg2_base = 0;
                        long long disp2 = 0;
                        if (parse_mem_operand(op2, &reg2_base, &disp2)) {
                            encode_movl_imm_mem(&text_seg, imm, reg2_base, disp2);
                        } else {
                            encode_movl_imm(&text_seg, imm, reg2);
                        }
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
                        unsigned char rex = 0x48;
                        unsigned char opcode = 0x63;
                        unsigned char modrm;
                        if (disp1 == 0 && (reg1 & 7) != 5) {
                            modrm = (0x00 << 6) | ((reg2 & 7) << 3) | (reg1 & 7);
                            if (reg1 & 8) rex |= 0x01;
                            if (reg2 & 8) rex |= 0x04;
                            seg_append(&text_seg, &rex, 1);
                            seg_append(&text_seg, &opcode, 1);
                            seg_append(&text_seg, &modrm, 1);
                            if ((reg1 & 7) == 4) { unsigned char sib = 0x24; seg_append(&text_seg, &sib, 1); }
                        } else if (disp1 >= -128 && disp1 <= 127) {
                            modrm = (0x01 << 6) | ((reg2 & 7) << 3) | (reg1 & 7);
                            if (reg1 & 8) rex |= 0x01;
                            if (reg2 & 8) rex |= 0x04;
                            seg_append(&text_seg, &rex, 1);
                            seg_append(&text_seg, &opcode, 1);
                            seg_append(&text_seg, &modrm, 1);
                            if ((reg1 & 7) == 4) { unsigned char sib = 0x24; seg_append(&text_seg, &sib, 1); }
                            unsigned char d = (unsigned char)disp1;
                            seg_append(&text_seg, &d, 1);
                        } else {
                            modrm = (0x02 << 6) | ((reg2 & 7) << 3) | (reg1 & 7);
                            if (reg1 & 8) rex |= 0x01;
                            if (reg2 & 8) rex |= 0x04;
                            seg_append(&text_seg, &rex, 1);
                            seg_append(&text_seg, &opcode, 1);
                            seg_append(&text_seg, &modrm, 1);
                            if ((reg1 & 7) == 4) { unsigned char sib = 0x24; seg_append(&text_seg, &sib, 1); }
                            unsigned char d[4];
                            d[0] = disp1 & 0xFF; d[1] = (disp1 >> 8) & 0xFF;
                            d[2] = (disp1 >> 16) & 0xFF; d[3] = (disp1 >> 24) & 0xFF;
                            seg_append(&text_seg, d, 4);
                        }
                    } else {
                        encode_movslq(&text_seg, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "addss") == 0 || strcmp(mnemonic, "addsd") == 0 ||
                           strcmp(mnemonic, "subss") == 0 || strcmp(mnemonic, "subsd") == 0 ||
                           strcmp(mnemonic, "mulss") == 0 || strcmp(mnemonic, "mulsd") == 0 ||
                           strcmp(mnemonic, "divss") == 0 || strcmp(mnemonic, "divsd") == 0 ||
                           strcmp(mnemonic, "cvtsd2ss") == 0 || strcmp(mnemonic, "cvtss2sd") == 0) {
                    const char *pref = (strcmp(mnemonic, "addss") == 0 || strcmp(mnemonic, "subss") == 0 ||
                                        strcmp(mnemonic, "mulss") == 0 || strcmp(mnemonic, "divss") == 0 ||
                                        strcmp(mnemonic, "cvtss2sd") == 0) ? "\xF3" : "\xF2";
                    unsigned char opcode = 0x58;
                    if (strcmp(mnemonic, "addss") == 0 || strcmp(mnemonic, "addsd") == 0) opcode = 0x58;
                    else if (strcmp(mnemonic, "subss") == 0 || strcmp(mnemonic, "subsd") == 0) opcode = 0x5C;
                    else if (strcmp(mnemonic, "mulss") == 0 || strcmp(mnemonic, "mulsd") == 0) opcode = 0x59;
                    else if (strcmp(mnemonic, "divss") == 0 || strcmp(mnemonic, "divsd") == 0) opcode = 0x5E;
                    else if (strcmp(mnemonic, "cvtsd2ss") == 0 || strcmp(mnemonic, "cvtss2sd") == 0) opcode = 0x5A;
                    
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    
                    if (is_mem1) {
                        encode_sse_mem(&text_seg, pref, 1, opcode, reg1, reg2, 1, disp1);
                    } else {
                        encode_sse_binop(&text_seg, pref, 1, opcode, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "ucomiss") == 0 || strcmp(mnemonic, "ucomisd") == 0) {
                    const char *pref = (strcmp(mnemonic, "ucomiss") == 0) ? "" : "\x66";
                    int n_pref = (strcmp(mnemonic, "ucomiss") == 0) ? 0 : 1;
                    unsigned char opcode = 0x2E;
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    
                    if (is_mem1) {
                        encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1, disp1);
                    } else {
                        encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "pxor") == 0) {
                    const char *pref = "\x66";
                    int n_pref = 1;
                    unsigned char opcode = 0xEF;
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    
                    if (is_mem1) {
                        encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1, disp1);
                    } else {
                        encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "pcmpeqd") == 0) {
                    const char *pref = "\x66";
                    int n_pref = 1;
                    unsigned char opcode = 0x76;
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    
                    if (is_mem1) {
                        encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1, disp1);
                    } else {
                        encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "psrld") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        unsigned char pref = 0x66;
                        seg_append(&text_seg, &pref, 1);
                        int hw_dst = reg2 & 15;
                        unsigned char rex = 0x40;
                        int need_rex = 0;
                        if (hw_dst & 8) {
                            rex |= 0x01;
                            need_rex = 1;
                        }
                        if (need_rex) {
                            seg_append(&text_seg, &rex, 1);
                        }
                        unsigned char escape = 0x0f;
                        seg_append(&text_seg, &escape, 1);
                        unsigned char opcode = 0x72;
                        seg_append(&text_seg, &opcode, 1);
                        unsigned char modrm = 0xc0 | (2 << 3) | (hw_dst & 7);
                        seg_append(&text_seg, &modrm, 1);
                        unsigned char imm_val = (unsigned char)imm;
                        seg_append(&text_seg, &imm_val, 1);
                    } else {
                        fprintf(stderr, "assembler error: psrld only supports immediate shift\n");
                        exit(1);
                    }
                } else if (strcmp(mnemonic, "psrlq") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        unsigned char pref = 0x66;
                        seg_append(&text_seg, &pref, 1);
                        int hw_dst = reg2 & 15;
                        unsigned char rex = 0x40;
                        int need_rex = 0;
                        if (hw_dst & 8) {
                            rex |= 0x01;
                            need_rex = 1;
                        }
                        if (need_rex) {
                            seg_append(&text_seg, &rex, 1);
                        }
                        unsigned char escape = 0x0f;
                        seg_append(&text_seg, &escape, 1);
                        unsigned char opcode = 0x73;
                        seg_append(&text_seg, &opcode, 1);
                        unsigned char modrm = 0xc0 | (2 << 3) | (hw_dst & 7);
                        seg_append(&text_seg, &modrm, 1);
                        unsigned char imm_val = (unsigned char)imm;
                        seg_append(&text_seg, &imm_val, 1);
                    } else {
                        fprintf(stderr, "assembler error: psrlq only supports immediate shift\n");
                        exit(1);
                    }
                } else if (strcmp(mnemonic, "andps") == 0) {
                    const char *pref = "";
                    int n_pref = 0;
                    unsigned char opcode = 0x54;
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    if (is_mem1) {
                        encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1, disp1);
                    } else {
                        encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "andpd") == 0) {
                    const char *pref = "\x66";
                    int n_pref = 1;
                    unsigned char opcode = 0x54;
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    if (is_mem1) {
                        encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1, disp1);
                    } else {
                        encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "cvttss2si") == 0 || strcmp(mnemonic, "cvttss2siq") == 0 ||
                           strcmp(mnemonic, "cvttsd2si") == 0 || strcmp(mnemonic, "cvttsd2siq") == 0) {
                    const char *pref = (strncmp(mnemonic, "cvttss", 6) == 0) ? "\xF3" : "\xF2";
                    int use_rex_w = (strstr(mnemonic, "siq") != NULL) || is_reg_64(op2);
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    
                    if (is_mem1) {
                        encode_cvtt_to_si_mem(&text_seg, pref, 1, 0x2C, reg1, reg2, use_rex_w, disp1);
                    } else {
                        encode_cvtt_to_si(&text_seg, pref, 1, 0x2C, reg1, reg2, use_rex_w);
                    }
                } else if (strcmp(mnemonic, "cvtsi2ss") == 0 || strcmp(mnemonic, "cvtsi2ssq") == 0 || strcmp(mnemonic, "cvtsi2ssl") == 0 ||
                           strcmp(mnemonic, "cvtsi2sd") == 0 || strcmp(mnemonic, "cvtsi2sdq") == 0 || strcmp(mnemonic, "cvtsi2sdl") == 0) {
                    const char *pref = (strstr(mnemonic, "2ss") != NULL) ? "\xF3" : "\xF2";
                    int use_rex_w = (strstr(mnemonic, "q") != NULL) || is_reg_64(op1);
                    int is_mem1 = 0;
                    long long disp1 = 0;
                    if (parse_mem_operand(op1, &reg1, &disp1)) is_mem1 = 1;
                    
                    if (is_mem1) {
                        encode_cvtsi_to_xmm_mem(&text_seg, pref, 1, 0x2A, reg1, reg2, use_rex_w, disp1);
                    } else {
                        encode_cvtsi_to_xmm(&text_seg, pref, 1, 0x2A, reg1, reg2, use_rex_w);
                    }
                } else if (strcmp(mnemonic, "addq") == 0 || strcmp(mnemonic, "subq") == 0 ||
                           strcmp(mnemonic, "cmpq") == 0 || strcmp(mnemonic, "imulq") == 0 ||
                           strcmp(mnemonic, "andq") == 0 || strcmp(mnemonic, "orq") == 0 ||
                           strcmp(mnemonic, "xorq") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        if (strcmp(mnemonic, "imulq") == 0) {
                            unsigned char rex = 0x48;
                            unsigned char opcode = 0x69;
                            unsigned char modrm = 0xc0 | ((reg2 & 7) << 3) | (reg2 & 7);
                            if (reg2 & 8) {
                                rex |= 0x01; // REX.B
                                rex |= 0x04; // REX.R
                            }
                            if (imm >= -128 && imm <= 127) {
                                opcode = 0x6b;
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
                            unsigned char rex = 0x48;
                            unsigned char opcode = 0x81;
                            unsigned char modrm = 0xc0;
                            if (strcmp(mnemonic, "addq") == 0) modrm = 0xc0;
                            else if (strcmp(mnemonic, "orq") == 0)  modrm = 0xc8;
                            else if (strcmp(mnemonic, "andq") == 0) modrm = 0xe0;
                            else if (strcmp(mnemonic, "subq") == 0) modrm = 0xe8;
                            else if (strcmp(mnemonic, "xorq") == 0) modrm = 0xf0;
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
                        }
                    } else {
                        encode_binop(&text_seg, mnemonic, reg1, reg2);
                    }
                } else if (strcmp(mnemonic, "sarq") == 0 || strcmp(mnemonic, "shlq") == 0 || strcmp(mnemonic, "shrq") == 0 ||
                           strcmp(mnemonic, "sarl") == 0 || strcmp(mnemonic, "shll") == 0 || strcmp(mnemonic, "shrl") == 0 ||
                           strcmp(mnemonic, "sarw") == 0 || strcmp(mnemonic, "shlw") == 0 || strcmp(mnemonic, "shrw") == 0 ||
                           strcmp(mnemonic, "sarb") == 0 || strcmp(mnemonic, "shlb") == 0 || strcmp(mnemonic, "shrb") == 0) {
                    char size_char = mnemonic[strlen(mnemonic) - 1];
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        unsigned char prefix = 0;
                        unsigned char rex = 0;
                        unsigned char opcode = 0;
                        unsigned char modrm;
                        
                        if (size_char == 'w') {
                            prefix = 0x66;
                        }
                        
                        if (size_char == 'q') {
                            rex = 0x48;
                        } else if (reg2 & 8) {
                            rex = 0x40;
                        } else if (size_char == 'b' && (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7)) {
                            rex = 0x40;
                        }
                        
                        if (reg2 & 8) rex |= 0x01;
                        
                        if (size_char == 'b') {
                            opcode = 0xc0;
                        } else {
                            opcode = 0xc1;
                        }
                        
                        int shift_type = 0;
                        if (strncmp(mnemonic, "sar", 3) == 0) shift_type = 2;
                        else if (strncmp(mnemonic, "shr", 3) == 0) shift_type = 1;
                        
                        if (shift_type == 2) {
                            modrm = 0xf8 | (reg2 & 7);
                        } else if (shift_type == 1) {
                            modrm = 0xe8 | (reg2 & 7);
                        } else {
                            modrm = 0xe0 | (reg2 & 7);
                        }
                        
                        if (prefix) seg_append(&text_seg, &prefix, 1);
                        if (rex) seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                        unsigned char b = (unsigned char)imm;
                        seg_append(&text_seg, &b, 1);
                    } else {
                        unsigned char prefix = 0;
                        unsigned char rex = 0;
                        unsigned char opcode = 0;
                        unsigned char modrm;
                        
                        if (size_char == 'w') {
                            prefix = 0x66;
                        }
                        
                        if (size_char == 'q') {
                            rex = 0x48;
                        } else if (reg2 & 8) {
                            rex = 0x40;
                        } else if (size_char == 'b' && (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7)) {
                            rex = 0x40;
                        }
                        
                        if (reg2 & 8) rex |= 0x01;
                        
                        if (size_char == 'b') {
                            opcode = 0xd2;
                        } else {
                            opcode = 0xd3;
                        }
                        
                        int shift_type = 0;
                        if (strncmp(mnemonic, "sar", 3) == 0) shift_type = 2;
                        else if (strncmp(mnemonic, "shr", 3) == 0) shift_type = 1;
                        
                        if (shift_type == 2) {
                            modrm = 0xf8 | (reg2 & 7);
                        } else if (shift_type == 1) {
                            modrm = 0xe8 | (reg2 & 7);
                        } else {
                            modrm = 0xe0 | (reg2 & 7);
                        }
                        
                        if (prefix) seg_append(&text_seg, &prefix, 1);
                        if (rex) seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                    }
                } else if (strcmp(mnemonic, "addl") == 0 || strcmp(mnemonic, "subl") == 0 || strcmp(mnemonic, "cmpl") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        unsigned char rex = 0x40;
                        unsigned char opcode = 0x81;
                        unsigned char modrm = 0xc0;
                        if (strcmp(mnemonic, "addl") == 0) modrm = 0xc0 | (reg2 & 7);
                        else if (strcmp(mnemonic, "subl") == 0) modrm = 0xe8 | (reg2 & 7);
                        else if (strcmp(mnemonic, "cmpl") == 0) modrm = 0xf8 | (reg2 & 7);
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
                        unsigned char opcode = 0x01;
                        if (strcmp(mnemonic, "addl") == 0) opcode = 0x01;
                        else if (strcmp(mnemonic, "subl") == 0) opcode = 0x29;
                        else if (strcmp(mnemonic, "cmpl") == 0) opcode = 0x39;
                        unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
                        if (reg1 & 8) rex |= 0x04;
                        if (reg2 & 8) rex |= 0x01;
                        if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                    }
                } else if (strcmp(mnemonic, "xorl") == 0 || strcmp(mnemonic, "andl") == 0 || strcmp(mnemonic, "orl") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        unsigned char rex = 0x40;
                        unsigned char opcode = 0x81;
                        unsigned char modrm = 0xc0;
                        if (strcmp(mnemonic, "orl") == 0) modrm = 0xc8;
                        else if (strcmp(mnemonic, "andl") == 0) modrm = 0xe0;
                        else if (strcmp(mnemonic, "xorl") == 0) modrm = 0xf0;
                        modrm |= (reg2 & 7);
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
                            b[0] = imm & 0xFF;
                            b[1] = (imm >> 8) & 0xFF;
                            b[2] = (imm >> 16) & 0xFF;
                            b[3] = (imm >> 24) & 0xFF;
                            seg_append(&text_seg, b, 4);
                        }
                    } else {
                        unsigned char rex = 0x40;
                        unsigned char opcode = 0x31;
                        if (strcmp(mnemonic, "orl") == 0) opcode = 0x09;
                        else if (strcmp(mnemonic, "andl") == 0) opcode = 0x21;
                        else if (strcmp(mnemonic, "xorl") == 0) opcode = 0x31;
                        unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
                        if (reg1 & 8) rex |= 0x04;
                        if (reg2 & 8) rex |= 0x01;
                        if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                    }
                } else if (strcmp(mnemonic, "xorb") == 0 || strcmp(mnemonic, "andb") == 0 || strcmp(mnemonic, "orb") == 0 ||
                           strcmp(mnemonic, "addb") == 0 || strcmp(mnemonic, "subb") == 0 || strcmp(mnemonic, "cmpb") == 0) {
                    if (op1[0] == '$') {
                        long long imm = strtoll(op1 + 1, NULL, 0);
                        unsigned char rex = 0x40;
                        unsigned char opcode = 0x80;
                        unsigned char modrm = 0xc0;
                        if (strcmp(mnemonic, "addb") == 0) modrm = 0xc0;
                        else if (strcmp(mnemonic, "orb") == 0) modrm = 0xc8;
                        else if (strcmp(mnemonic, "subb") == 0) modrm = 0xe8;
                        else if (strcmp(mnemonic, "andb") == 0) modrm = 0xe0;
                        else if (strcmp(mnemonic, "xorb") == 0) modrm = 0xf0;
                        else if (strcmp(mnemonic, "cmpb") == 0) modrm = 0xf8;
                        modrm |= (reg2 & 7);
                        if (reg2 & 8) rex |= 0x01;
                        if (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7) rex |= 0x40;
                        
                        if (rex != 0x40 || (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7)) {
                            seg_append(&text_seg, &rex, 1);
                        }
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                        unsigned char b = (unsigned char)imm;
                        seg_append(&text_seg, &b, 1);
                    } else {
                        unsigned char rex = 0x40;
                        unsigned char opcode = 0x30;
                        if (strcmp(mnemonic, "addb") == 0) opcode = 0x00;
                        else if (strcmp(mnemonic, "orb") == 0) opcode = 0x08;
                        else if (strcmp(mnemonic, "subb") == 0) opcode = 0x28;
                        else if (strcmp(mnemonic, "andb") == 0) opcode = 0x20;
                        else if (strcmp(mnemonic, "xorb") == 0) opcode = 0x30;
                        else if (strcmp(mnemonic, "cmpb") == 0) opcode = 0x38;
                        unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
                        if (reg1 & 8) rex |= 0x04;
                        if (reg2 & 8) rex |= 0x01;
                        if (reg1 == 4 || reg1 == 5 || reg1 == 6 || reg1 == 7 ||
                            reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7) {
                            rex |= 0x40;
                        }
                        if (rex != 0x40 || (rex & 0x40)) {
                            seg_append(&text_seg, &rex, 1);
                        }
                        seg_append(&text_seg, &opcode, 1);
                        seg_append(&text_seg, &modrm, 1);
                    }
                } else if (strcmp(mnemonic, "testq") == 0) {
                    /* testq %rSrc, %rDst — REX.W 0x85 ModRM(11,src,dst) */
                    unsigned char rex = 0x48;
                    unsigned char opcode = 0x85;
                    if (reg1 & 8) rex |= 0x04;
                    if (reg2 & 8) rex |= 0x01;
                    unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
                    seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                    seg_append(&text_seg, &modrm, 1);
                } else if (strcmp(mnemonic, "testl") == 0) {
                    /* testl %eSrc, %eDst — [REX] 0x85 ModRM(11,src,dst) */
                    unsigned char rex = 0x40;
                    unsigned char opcode = 0x85;
                    if (reg1 & 8) rex |= 0x04;
                    if (reg2 & 8) rex |= 0x01;
                    unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
                    if (rex != 0x40) seg_append(&text_seg, &rex, 1);
                    seg_append(&text_seg, &opcode, 1);
                    seg_append(&text_seg, &modrm, 1);
                } else {
                    fprintf(stderr, "assembler error: unrecognized 2-operand instruction '%s'\n", mnemonic);
                    exit(1);
                }
                matched = 1;
            }
        }
        
        if (!matched) {
            fprintf(stderr, "assembler error: unrecognized instruction '%s' with args '%s'\n", mnemonic, args_start ? args_start : "");
            exit(1);
        }
    }
    if (in) {
        fclose(in);
    }
    if (g_direct_asm_lines) {
        size_t li;
        for (li = 0; li < g_direct_asm_line_count; li++) {
            free(g_direct_asm_lines[li]);
        }
        free(g_direct_asm_lines);
        g_direct_asm_lines = NULL;
        g_direct_asm_line_count = 0;
        g_direct_asm_line_cap = 0;
    }
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
                if (label_count >= 131072) {
                    fprintf(stderr, "error: assembler label table limit exceeded (max 131072)\n");
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

extern int zld_link(const char **obj_files, int obj_count, const char *out_path, const char *script_path, const char *tensor_attest_bin_path, const char *tensor_note_json_path, const char *build_attest_bin_path);

int main(int argc, char **argv) {
    int i;
    int use_system_as = 0;
    int native_elf = 0;
    int compile_only = 0;
    char *input_c_file = NULL;
    char *out_filename = NULL;
    char *ld_script = NULL;
    const char *obj_files[2048];
    int obj_count = 0;
    int has_trace_abi = 0;
    int has_emit_gguf = 0;
    int has_frontend_dump = 0;

    /* Parse arguments */
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--use-system-as") == 0) {
            use_system_as = 1;
        } else if (strcmp(argv[i], "--native-elf") == 0) {
            native_elf = 1;
        } else if (strcmp(argv[i], "--trace-abi") == 0) {
            has_trace_abi = 1;
        } else if (strcmp(argv[i], "--emit-gguf") == 0) {
            has_emit_gguf = 1;
        } else if (strcmp(argv[i], "--pp-only") == 0 || strcmp(argv[i], "--dump-ast-json") == 0) {
            has_frontend_dump = 1;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "-emit-obj") == 0) {
            compile_only = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                out_filename = argv[i + 1];
                i++;
            }
        } else if (strcmp(argv[i], "-T") == 0) {
            if (i + 1 < argc) {
                ld_script = argv[i + 1];
                i++;
            }
        } else if (argv[i][0] != '-') {
            int len = strlen(argv[i]);
            if (len > 2 && strcmp(argv[i] + len - 2, ".c") == 0) {
                input_c_file = argv[i];
            } else if (len > 2 && strcmp(argv[i] + len - 2, ".o") == 0) {
                if (obj_count < 2048) {
                    obj_files[obj_count++] = argv[i];
                }
            }
        }
    }

    /* Fallback escape hatch: use system assembler and linker if explicitly requested */
    if (use_system_as) {
        char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 1));
        int mod_argc = 0;
        for (i = 0; i < argc; i++) {
            if (strcmp(argv[i], "--use-system-as") == 0) {
                continue;
            }
            mod_argv[mod_argc++] = argv[i];
        }
        mod_argv[mod_argc] = NULL;
        int ret = zcc_main(mod_argc, mod_argv);
        free(mod_argv);
        return ret;
    }

    /* Bootstrap pass-through: compile zcc.c using legacy compiler driver logic */
    int is_zcc_c = 0;
    if (input_c_file) {
        const char *p = input_c_file;
        int len = strlen(p);
        if (strcmp(p, "zcc.c") == 0 || (len >= 6 && strcmp(p + len - 6, "/zcc.c") == 0)) {
            is_zcc_c = 1;
        }
    }

    /* Output-assembly pass-through: if compiling directly to .s, use legacy path */
    int is_out_s = 0;
    if (out_filename) {
        int len = strlen(out_filename);
        if (len > 2 && strcmp(out_filename + len - 2, ".s") == 0) {
            is_out_s = 1;
        }
    }

    if ((is_zcc_c && !native_elf) || is_out_s || has_trace_abi || has_emit_gguf || has_frontend_dump) {
        return zcc_main(argc, argv);
    }

    /* Driver integration: assemble/link freestanding statically by default */
    if (input_c_file) {
        /* Generate safe temporary assembly filename in current directory */
        char temp_s_filename[256];
        const char *base = input_c_file;
        const char *p = input_c_file;
        while (*p) {
            if (*p == '/' || *p == '\\') {
                base = p + 1;
            }
            p++;
        }
        sprintf(temp_s_filename, ".tmp_codegen_%s.s", base);

        if (compile_only) {
            char derived_o_filename[256];
            if (!out_filename) {
                strncpy(derived_o_filename, input_c_file, sizeof(derived_o_filename) - 1);
                derived_o_filename[sizeof(derived_o_filename) - 1] = '\0';
                int len = strlen(derived_o_filename);
                if (len > 2 && strcmp(derived_o_filename + len - 2, ".c") == 0) {
                    derived_o_filename[len - 2] = '\0';
                    strcat(derived_o_filename, ".o");
                } else {
                    strcat(derived_o_filename, ".o");
                }
                out_filename = derived_o_filename;
            }

            /* Construct modified argv for zcc_main to produce temporary assembly */
            char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 3));
            int mod_argc = 0;
            mod_argv[mod_argc++] = argv[0];
            for (i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-c") == 0) {
                    continue;
                } else if (strcmp(argv[i], "-o") == 0) {
                    if (i + 1 < argc) i++;
                    continue;
                } else if (strcmp(argv[i], "-T") == 0) {
                    if (i + 1 < argc) i++;
                    continue;
                } else if (strcmp(argv[i], "-emit-obj") == 0) {
                    continue;
                } else {
                    mod_argv[mod_argc++] = argv[i];
                }
            }
            mod_argv[mod_argc++] = "-o";
            mod_argv[mod_argc++] = temp_s_filename;
            mod_argv[mod_argc] = NULL;

            /* Enable in-memory assembly stream */
            g_use_in_mem_asm = 1;
            g_in_mem_asm_buf = NULL;
            g_in_mem_asm_size = 0;

            int compile_ret = zcc_main(mod_argc, mod_argv);
            free(mod_argv);

            if (compile_ret != 0) {
                fprintf(stderr, "zcc: AST-codegen phase failed\n");
                if (g_in_mem_asm_buf) free(g_in_mem_asm_buf);
                g_in_mem_asm_buf = NULL;
                g_in_mem_asm_size = 0;
                g_use_in_mem_asm = 0;
                return compile_ret;
            }

            int assemble_ret = assemble(NULL, out_filename, g_in_mem_asm_buf, g_in_mem_asm_size);
            if (g_in_mem_asm_buf) {
                free(g_in_mem_asm_buf);
                g_in_mem_asm_buf = NULL;
                g_in_mem_asm_size = 0;
            }
            g_use_in_mem_asm = 0;

            if (assemble_ret != 0) {
                fprintf(stderr, "zcc: surgical ELF emission failed with error code %d\n", assemble_ret);
                return assemble_ret;
            }

            return 0;
        } else {
            /* Compile and Link */
            if (!out_filename) {
                out_filename = "a.out";
            }

            char temp_o_filename[256];
            sprintf(temp_o_filename, ".tmp_codegen_%s.o", base);

            /* 1. Compile to temporary assembly (.s) */
            char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 3));
            int mod_argc = 0;
            mod_argv[mod_argc++] = argv[0];
            for (i = 1; i < argc; i++) {
                if (strcmp(argv[i], "-o") == 0) {
                    if (i + 1 < argc) i++;
                    continue;
                } else if (strcmp(argv[i], "-T") == 0) {
                    if (i + 1 < argc) i++;
                    continue;
                } else if (strcmp(argv[i], "-emit-obj") == 0) {
                    continue;
                } else {
                    mod_argv[mod_argc++] = argv[i];
                }
            }
            mod_argv[mod_argc++] = "-o";
            mod_argv[mod_argc++] = temp_s_filename;
            mod_argv[mod_argc] = NULL;

            /* Enable in-memory assembly stream */
            g_use_in_mem_asm = 1;
            g_in_mem_asm_buf = NULL;
            g_in_mem_asm_size = 0;

            int compile_ret = zcc_main(mod_argc, mod_argv);
            free(mod_argv);

            if (compile_ret != 0) {
                fprintf(stderr, "zcc: AST-codegen phase failed\n");
                if (g_in_mem_asm_buf) free(g_in_mem_asm_buf);
                g_in_mem_asm_buf = NULL;
                g_in_mem_asm_size = 0;
                g_use_in_mem_asm = 0;
                return compile_ret;
            }

            /* 2. Assemble directly to temporary object (.o) in memory */
            int assemble_ret = assemble(NULL, temp_o_filename, g_in_mem_asm_buf, g_in_mem_asm_size);
            if (g_in_mem_asm_buf) {
                free(g_in_mem_asm_buf);
                g_in_mem_asm_buf = NULL;
                g_in_mem_asm_size = 0;
            }
            g_use_in_mem_asm = 0;

            if (assemble_ret != 0) {
                fprintf(stderr, "zcc: surgical ELF emission failed with error code %d\n", assemble_ret);
                return assemble_ret;
            }

            /* 3. Link utilizing zld */
            const char *link_objs[2048];
            int link_obj_count = 0;
            link_objs[link_obj_count++] = temp_o_filename;
            for (i = 0; i < obj_count; i++) {
                if (link_obj_count < 2048) {
                    link_objs[link_obj_count++] = obj_files[i];
                }
            }

            int link_ret = zld_link(link_objs, link_obj_count, out_filename, ld_script, NULL, NULL, NULL);
            remove(temp_o_filename);

            if (link_ret != 0) {
                fprintf(stderr, "zcc: static linking failed with error code %d\n", link_ret);
                return link_ret;
            }

            return 0;
        }
    } else if (obj_count > 0) {
        /* Only object files passed: run linker directly */
        if (!out_filename) {
            out_filename = "a.out";
        }
        int link_ret = zld_link(obj_files, obj_count, out_filename, ld_script, NULL, NULL, NULL);
        if (link_ret != 0) {
            fprintf(stderr, "zcc: static linking failed with error code %d\n", link_ret);
            return link_ret;
        }
        return 0;
    } else {
        /* Pass through standard execution */
        return zcc_main(argc, argv);
    }
}

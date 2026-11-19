#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "elf_emit.h"
#include "arm64_elf_emit.c"
#include "riscv_elf_emit.c"
#include "win64_pe_emit.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#if defined(_MSC_VER) || defined(__MINGW32__)
#include <process.h>
#else
extern int _getpid(void);
#endif
#else
#include <unistd.h>
#endif

#ifndef __GNUC_MINOR__
int _builtin_popcount(unsigned int x) {
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
int g_win64_gui_subsystem = 0;
int g_win64_is_shared = 0;
int g_win64_ms_abi = 0;
const char *g_win64_cert_file = NULL;
const char *g_win64_delay_dlls[16] = {0};
const char *g_win64_delay_funcs[16][32] = {{0}};
size_t g_win64_delay_func_counts[16] = {0};
size_t g_win64_delay_dll_count = 0;

static char g_temp_o_to_remove[256] = {0};
static char g_temp_s_to_remove[256] = {0};
static char g_temp_multi_c_to_remove[256] = {0};

static void cleanup_temp_files(void) {
  if (g_temp_o_to_remove[0]) {
    remove(g_temp_o_to_remove);
    g_temp_o_to_remove[0] = '\0';
  }
  if (g_temp_s_to_remove[0]) {
    remove(g_temp_s_to_remove);
    g_temp_s_to_remove[0] = '\0';
  }
  if (g_temp_multi_c_to_remove[0]) {
    remove(g_temp_multi_c_to_remove);
    g_temp_multi_c_to_remove[0] = '\0';
  }
}

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
    if (!seg->cap)
      seg->cap = 4096;
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

static Label *labels = NULL;
static size_t label_count = 0;

#define LABEL_HASH_BUCKETS 262144
static int label_hash_slot[LABEL_HASH_BUCKETS];

static unsigned int code_fnv1a(const char *s) {
  unsigned int h = 2166136261u;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

static int find_label_idx(const char *name) {
  unsigned int h = code_fnv1a(name) & (LABEL_HASH_BUCKETS - 1);
  while (label_hash_slot[h] != -1) {
    int idx = label_hash_slot[h];
    if (strcmp(labels[idx].name, name) == 0)
      return idx;
    h = (h + 1) & (LABEL_HASH_BUCKETS - 1);
  }
  return -1;
}

static void register_label_hash(size_t idx) {
  unsigned int h = code_fnv1a(labels[idx].name) & (LABEL_HASH_BUCKETS - 1);
  while (label_hash_slot[h] != -1) {
    if (label_hash_slot[h] == (int)idx)
      return;
    h = (h + 1) & (LABEL_HASH_BUCKETS - 1);
  }
  label_hash_slot[h] = (int)idx;
}

/* Relocation tracking */
typedef struct {
  size_t offset;
  char target_name[128];
  int type;
  long long addend;
} Reloc;

static Reloc *relocs = NULL;
static size_t reloc_count = 0;

static void ensure_codegen_tables(void) {
  if (!labels) {
    /* Simultaneous single-slab allocation for labels + relocs */
    void *slab = calloc(1, 131072 * (sizeof(Label) + sizeof(Reloc)));
    labels = (Label *)slab;
    relocs = (Reloc *)((char *)slab + 131072 * sizeof(Label));
  }
}

static int codegen_cmp_u32(const void *a, const void *b) {
  uint32_t ua = *(const uint32_t *)a;
  uint32_t ub = *(const uint32_t *)b;
  return (ua > ub) - (ua < ub);
}

static const char *win64_remap_msvcrt_sym(const char *sym, int ms_abi) {
  if (!sym) return sym;
  if (strcmp(sym, "snprintf") == 0) return ms_abi ? "_snprintf" : "_vsnprintf";
  if (!ms_abi && strcmp(sym, "_snprintf") == 0) return "_vsnprintf";
  if (strcmp(sym, "strdup") == 0) return "_strdup";
  if (strcmp(sym, "strtoll") == 0) return "_strtoi64";
  if (strcmp(sym, "strtoull") == 0) return "_strtoui64";
  if (strcmp(sym, "atoll") == 0) return "_atoi64";
  if (strcmp(sym, "isatty") == 0) return "_isatty";
  if (strcmp(sym, "getpid") == 0) return "_getpid";
  if (strcmp(sym, "getcwd") == 0) return "_getcwd";
  return sym;
}

static void add_reloc(Reloc r) {
  ensure_codegen_tables();
  size_t tlen = strlen(r.target_name);
  if (tlen > 2) {
    for (size_t k = 1; k < tlen; k++) {
      if (r.target_name[k] == '+' || r.target_name[k] == '-') {
        char *ep = NULL;
        long long off = strtoll(&r.target_name[k], &ep, 0);
        if (ep && *ep == '\0' && ep != &r.target_name[k] + 1) {
          r.addend += off;
          r.target_name[k] = '\0';
        }
        break;
      }
    }
  }
  if (reloc_count >= 131072) {
    fprintf(stderr,
            "error: assembler relocation table limit exceeded (max 131072)\n");
    exit(1);
  }
  relocs[reloc_count++] = r;
}

static int parse_reg(const char *s) {
  if (strcmp(s, "%rax") == 0 || strcmp(s, "%eax") == 0 ||
      strcmp(s, "%ax") == 0 || strcmp(s, "%al") == 0)
    return 0;
  if (strcmp(s, "%rcx") == 0 || strcmp(s, "%ecx") == 0 ||
      strcmp(s, "%cx") == 0 || strcmp(s, "%cl") == 0)
    return 1;
  if (strcmp(s, "%rdx") == 0 || strcmp(s, "%edx") == 0 ||
      strcmp(s, "%dx") == 0 || strcmp(s, "%dl") == 0)
    return 2;
  if (strcmp(s, "%rbx") == 0 || strcmp(s, "%ebx") == 0 ||
      strcmp(s, "%bx") == 0 || strcmp(s, "%bl") == 0)
    return 3;
  if (strcmp(s, "%rsp") == 0 || strcmp(s, "%esp") == 0 ||
      strcmp(s, "%sp") == 0 || strcmp(s, "%spl") == 0)
    return 4;
  if (strcmp(s, "%rbp") == 0 || strcmp(s, "%ebp") == 0 ||
      strcmp(s, "%bp") == 0 || strcmp(s, "%bpl") == 0)
    return 5;
  if (strcmp(s, "%rsi") == 0 || strcmp(s, "%esi") == 0 ||
      strcmp(s, "%si") == 0 || strcmp(s, "%sil") == 0)
    return 6;
  if (strcmp(s, "%rdi") == 0 || strcmp(s, "%edi") == 0 ||
      strcmp(s, "%di") == 0 || strcmp(s, "%dil") == 0)
    return 7;
  if (strcmp(s, "%r8") == 0 || strcmp(s, "%r8d") == 0 ||
      strcmp(s, "%r8w") == 0 || strcmp(s, "%r8b") == 0)
    return 8;
  if (strcmp(s, "%r9") == 0 || strcmp(s, "%r9d") == 0 ||
      strcmp(s, "%r9w") == 0 || strcmp(s, "%r9b") == 0)
    return 9;
  if (strcmp(s, "%r10") == 0 || strcmp(s, "%r10d") == 0 ||
      strcmp(s, "%r10w") == 0 || strcmp(s, "%r10b") == 0)
    return 10;
  if (strcmp(s, "%r11") == 0 || strcmp(s, "%r11d") == 0 ||
      strcmp(s, "%r11w") == 0 || strcmp(s, "%r11b") == 0)
    return 11;
  if (strcmp(s, "%r12") == 0 || strcmp(s, "%r12d") == 0 ||
      strcmp(s, "%r12w") == 0 || strcmp(s, "%r12b") == 0)
    return 12;
  if (strcmp(s, "%r13") == 0 || strcmp(s, "%r13d") == 0 ||
      strcmp(s, "%r13w") == 0 || strcmp(s, "%r13b") == 0)
    return 13;
  if (strcmp(s, "%r14") == 0 || strcmp(s, "%r14d") == 0 ||
      strcmp(s, "%r14w") == 0 || strcmp(s, "%r14b") == 0)
    return 14;
  if (strcmp(s, "%r15") == 0 || strcmp(s, "%r15d") == 0 ||
      strcmp(s, "%r15w") == 0 || strcmp(s, "%r15b") == 0)
    return 15;
  if (strcmp(s, "%xmm0") == 0)
    return 16;
  if (strcmp(s, "%xmm1") == 0)
    return 17;
  if (strcmp(s, "%xmm2") == 0)
    return 18;
  if (strcmp(s, "%xmm3") == 0)
    return 19;
  if (strcmp(s, "%xmm4") == 0)
    return 20;
  if (strcmp(s, "%xmm5") == 0)
    return 21;
  if (strcmp(s, "%xmm6") == 0)
    return 22;
  if (strcmp(s, "%xmm7") == 0)
    return 23;
  if (strcmp(s, "%xmm8") == 0)
    return 24;
  if (strcmp(s, "%xmm9") == 0)
    return 25;
  if (strcmp(s, "%xmm10") == 0)
    return 26;
  if (strcmp(s, "%xmm11") == 0)
    return 27;
  if (strcmp(s, "%xmm12") == 0)
    return 28;
  if (strcmp(s, "%xmm13") == 0)
    return 29;
  if (strcmp(s, "%xmm14") == 0)
    return 30;
  if (strcmp(s, "%xmm15") == 0)
    return 31;
  if (strcmp(s, "%rip") == 0)
    return 32;
  if (strncmp(s, "%zmm", 4) == 0 && s[4] >= '0' && s[4] <= '9') {
    int z = atoi(s + 4);
    if (z >= 0 && z <= 31)
      return 64 + z;
  }
  if (strncmp(s, "%k", 2) == 0 && s[2] >= '0' && s[2] <= '7' && s[3] == '\0') {
    return 96 + (s[2] - '0');
  }
  return -1;
}

static char *trim(char *s);

static int parse_mem_operand(const char *s, int *reg, long long *disp) {
  const char *paren = strchr(s, '(');
  if (paren) {
    char reg_name[64];
    const char *end_paren = strchr(paren, ')');
    if (!end_paren)
      return 0;
    size_t len = end_paren - (paren + 1);
    if (len >= 63)
      len = 63;
    strncpy(reg_name, paren + 1, len);
    reg_name[len] = '\0';
    *reg = parse_reg(reg_name);

    if (*reg == -1) {
      fprintf(stderr,
              "assembler error: invalid base register in memory operand '%s'\n",
              s);
      exit(1);
    }
    if (*reg == 32) {
      fprintf(stderr,
              "assembler error: %%rip is not supported as a base register in "
              "memory operand '%s' (must be rip-relative only)\n",
              s);
      exit(1);
    }

    if (paren == s) {
      *disp = 0;
    } else {
      char disp_str[64];
      size_t disp_len = paren - s;
      if (disp_len >= 63)
        disp_len = 63;
      strncpy(disp_str, s, disp_len);
      disp_str[disp_len] = '\0';
      char *tpoff = strstr(disp_str, "@tpoff");
      if (tpoff) {
        *tpoff = '\0';
        char *sym = trim(disp_str);
        *disp = 0;
        int lbl_idx = find_label_idx(sym);
        if (lbl_idx >= 0) {
          *disp = labels[lbl_idx].offset;
        }
      } else {
        *disp = strtoll(disp_str, NULL, 0);
      }
    }
    return 1;
  }
  return 0;
}

static int encode_cmpq_mem(Segment *seg, const char *mem_str, int reg_op,
                           unsigned char opcode) {
  const char *lp = strchr(mem_str, '(');
  const char *rp = lp ? strchr(lp, ')') : NULL;
  if (!lp || !rp || reg_op < 0 || reg_op > 15)
    return 0;
  long long disp = 0;
  if (lp > mem_str) {
    char disp_str[64];
    size_t dlen = (size_t)(lp - mem_str);
    if (dlen >= sizeof(disp_str))
      dlen = sizeof(disp_str) - 1;
    memcpy(disp_str, mem_str, dlen);
    disp_str[dlen] = '\0';
    disp = strtoll(disp_str, NULL, 0);
  }
  char inner[64];
  size_t ilen = (size_t)(rp - (lp + 1));
  if (ilen >= sizeof(inner))
    ilen = sizeof(inner) - 1;
  memcpy(inner, lp + 1, ilen);
  inner[ilen] = '\0';

  char *c1 = strchr(inner, ',');
  char *c2 = c1 ? strchr(c1 + 1, ',') : NULL;
  if (c1)
    *c1 = '\0';
  if (c2)
    *c2 = '\0';

  int base_reg = parse_reg(trim(inner));
  int idx_reg = c1 ? parse_reg(trim(c1 + 1)) : -1;
  int scale = c2 ? atoi(trim(c2 + 1)) : 1;
  int scale_bits = (scale == 1) ? 0 : (scale == 2) ? 1 : (scale == 4) ? 2 : (scale == 8) ? 3 : -1;
  if (base_reg < 0 || base_reg > 15 || (c1 && (idx_reg < 0 || idx_reg > 15 || idx_reg == 4)) || scale_bits < 0) {
    fprintf(stderr, "assembler error: invalid memory operand '%s' in cmpq\n", mem_str);
    exit(1);
  }

  unsigned char rex = 0x48;
  if (reg_op & 8)
    rex |= 0x04;
  if (idx_reg >= 0 && (idx_reg & 8))
    rex |= 0x02;
  if (base_reg & 8)
    rex |= 0x01;

  unsigned char mod = (disp == 0 && (base_reg & 7) != 5)
                          ? 0x00
                          : (disp >= -128 && disp <= 127) ? 0x01 : 0x02;
  int need_sib = (idx_reg >= 0) || ((base_reg & 7) == 4);
  unsigned char modrm = (unsigned char)((mod << 6) | ((reg_op & 7) << 3) |
                                        (need_sib ? 0x04 : (base_reg & 7)));
  seg_append(seg, &rex, 1);
  seg_append(seg, &opcode, 1);
  seg_append(seg, &modrm, 1);
  if (need_sib) {
    unsigned char sib = (unsigned char)((scale_bits << 6) |
                                        (((idx_reg >= 0) ? (idx_reg & 7) : 0x04) << 3) |
                                        (base_reg & 7));
    seg_append(seg, &sib, 1);
  }
  if (mod == 0x01) {
    unsigned char d = (unsigned char)disp;
    seg_append(seg, &d, 1);
  } else if (mod == 0x02) {
    unsigned char d[4] = {(unsigned char)(disp & 0xFF),
                          (unsigned char)((disp >> 8) & 0xFF),
                          (unsigned char)((disp >> 16) & 0xFF),
                          (unsigned char)((disp >> 24) & 0xFF)};
    seg_append(seg, d, 4);
  }
  return 1;
}

/* Helper encoders */
static void encode_movq(Segment *seg, int src_reg, int dst_reg, int is_mem_src,
                        int is_mem_dst, long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode;
  unsigned char modrm;

  if (is_mem_src) {
    if (src_reg & 8)
      rex |= 0x01;
    if (dst_reg & 8)
      rex |= 0x04;
  } else {
    if (src_reg & 8)
      rex |= 0x04;
    if (dst_reg & 8)
      rex |= 0x01;
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

static void encode_leaq(Segment *seg, int src_reg, int dst_reg,
                        long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0x8d;
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

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

static void encode_leaq_disp32(Segment *seg, int src_reg, int dst_reg,
                               long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0x8d;
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

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

static void encode_movq_imm(Segment *seg, long long imm, int dst_reg) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0xc7;
  unsigned char modrm = 0xc0 | (dst_reg & 7);
  if (dst_reg & 8)
    rex |= 0x01;
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

static void encode_movq_imm_mem(Segment *seg, long long imm, int base_reg,
                                long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0xc7;
  unsigned char modrm;
  if (base_reg & 8)
    rex |= 0x01;

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

static void encode_rip_relative(Segment *seg, const char *label_name, int reg,
                                int is_load) {
  unsigned char rex = 0x48;
  unsigned char opcode = (is_load == 2) ? 0x8d : (is_load ? 0x8b : 0x89);
  unsigned char modrm = 0x05 | ((reg & 7) << 3);
  if (reg & 8)
    rex |= 0x04;

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

static int g_num_label_gen[10];

static void encode_jump(Segment *seg, const char *mnemonic,
                        const char *target_label) {
  unsigned char bytes[2];
  size_t size = 0;
  char resolved_num_lbl[64];
  if (target_label && target_label[0] >= '0' && target_label[0] <= '9' &&
      (target_label[1] == 'b' || target_label[1] == 'f') &&
      target_label[2] == '\0') {
    int d = target_label[0] - '0';
    int gen = (target_label[1] == 'b') ? g_num_label_gen[d]
                                       : (g_num_label_gen[d] + 1);
    snprintf(resolved_num_lbl, sizeof(resolved_num_lbl), ".Lnum_%d_%d", d, gen);
    target_label = resolved_num_lbl;
  }

  if (strcmp(mnemonic, "jmp") == 0) {
    bytes[0] = 0xe9;
    size = 1;
  } else if (strcmp(mnemonic, "je") == 0 || strcmp(mnemonic, "jz") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x84;
    size = 2;
  } else if (strcmp(mnemonic, "jne") == 0 || strcmp(mnemonic, "jnz") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x85;
    size = 2;
  } else if (strcmp(mnemonic, "jl") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x8c;
    size = 2;
  } else if (strcmp(mnemonic, "jle") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x8e;
    size = 2;
  } else if (strcmp(mnemonic, "jg") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x8f;
    size = 2;
  } else if (strcmp(mnemonic, "jge") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x8d;
    size = 2;
  } else if (strcmp(mnemonic, "jb") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x82;
    size = 2;
  } else if (strcmp(mnemonic, "jbe") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x86;
    size = 2;
  } else if (strcmp(mnemonic, "ja") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x87;
    size = 2;
  } else if (strcmp(mnemonic, "jae") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x83;
    size = 2;
  } else if (strcmp(mnemonic, "js") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x88;
    size = 2;
  } else if (strcmp(mnemonic, "jns") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x89;
    size = 2;
  } else if (strcmp(mnemonic, "jp") == 0 || strcmp(mnemonic, "jpe") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x8a;
    size = 2;
  } else if (strcmp(mnemonic, "jnp") == 0 || strcmp(mnemonic, "jpo") == 0) {
    bytes[0] = 0x0f;
    bytes[1] = 0x8b;
    size = 2;
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
  unsigned char ff = 0xff;
  unsigned char mrm = 0xd0 | (reg & 7); /* ModRM: mod=11, reg=2 (/2), rm=reg */
  seg_append(seg, &ff, 1);
  seg_append(seg, &mrm, 1);
}

static void encode_binop(Segment *seg, const char *op, int src_reg,
                         int dst_reg) {
  unsigned char rex = 0x48;
  unsigned char opcode;
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x04;
  if (dst_reg & 8)
    rex |= 0x01;

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
    if (dst_reg & 8)
      rex |= 0x04;
    if (src_reg & 8)
      rex |= 0x01;
    seg_append(seg, &rex, 1);
    seg_append(seg, op2, 2);
    seg_append(seg, &modrm, 1);
  }
}

static void encode_shift(Segment *seg, const char *op, int dst_reg) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0xd3;
  unsigned char modrm;
  if (dst_reg & 8)
    rex |= 0x01;
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
  if (reg & 8)
    rex |= 0x01;
  seg_append(seg, &rex, 1);
  seg_append(seg, &opcode, 1);
  seg_append(seg, &modrm, 1);
}

static void encode_divq(Segment *seg, int reg) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0xf7;
  unsigned char modrm = 0xf0 | (reg & 7);
  if (reg & 8)
    rex |= 0x01;
  seg_append(seg, &rex, 1);
  seg_append(seg, &opcode, 1);
  seg_append(seg, &modrm, 1);
}

static void encode_negq(Segment *seg, int reg) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0xf7;
  unsigned char modrm = 0xd8 | (reg & 7);
  if (reg & 8)
    rex |= 0x01;
  seg_append(seg, &rex, 1);
  seg_append(seg, &opcode, 1);
  seg_append(seg, &modrm, 1);
}

static void encode_movl(Segment *seg, int src_reg, int dst_reg, int is_mem_src,
                        int is_mem_dst, long long disp) {
  unsigned char rex = 0x40;
  unsigned char opcode;
  unsigned char modrm;

  if (is_mem_src) {
    if (src_reg & 8)
      rex |= 0x01;
    if (dst_reg & 8)
      rex |= 0x04;
  } else {
    if (src_reg & 8)
      rex |= 0x04;
    if (dst_reg & 8)
      rex |= 0x01;
  }

  if (is_mem_src) {
    opcode = 0x8b;
    if (disp >= -128 && disp <= 127) {
      modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
      if (rex != 0x40)
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
      if (rex != 0x40)
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
      if (rex != 0x40)
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
      if (rex != 0x40)
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
    if (rex != 0x40)
      seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
  }
}

static void encode_movl_imm(Segment *seg, long long imm, int dst_reg) {
  unsigned char rex = 0x40;
  unsigned char opcode = 0xc7;
  unsigned char modrm = 0xc0 | (dst_reg & 7);
  if (dst_reg & 8)
    rex |= 0x01;
  if (rex != 0x40)
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

static void encode_movl_imm_mem(Segment *seg, long long imm, int base_reg,
                                long long disp) {
  unsigned char rex = 0x40;
  unsigned char opcode = 0xc7;
  unsigned char modrm;
  if (base_reg & 8)
    rex |= 0x01;

  if (disp == 0 && (base_reg & 7) != 5) {
    modrm = (0x00 << 6) | (0 << 3) | (base_reg & 7);
    if (rex != 0x40)
      seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
    if ((base_reg & 7) == 4) {
      unsigned char sib = 0x24;
      seg_append(seg, &sib, 1);
    }
  } else if (disp >= -128 && disp <= 127) {
    modrm = (0x01 << 6) | (0 << 3) | (base_reg & 7);
    if (rex != 0x40)
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
    if (rex != 0x40)
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

static void encode_movzwq(Segment *seg, int src_reg, int dst_reg,
                          int is_mem_src, long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode[2] = {0x0f, 0xb7};
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

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

static void encode_movslq(Segment *seg, int src_reg, int dst_reg) {
  unsigned char rex = 0x48;
  unsigned char opcode = 0x63;
  unsigned char modrm = 0xc0 | ((dst_reg & 7) << 3) | (src_reg & 7);
  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;
  seg_append(seg, &rex, 1);
  seg_append(seg, &opcode, 1);
  seg_append(seg, &modrm, 1);
}

static void encode_set(Segment *seg, const char *mnemonic, const char *op_str) {
  int reg = op_str ? parse_reg(op_str) : 0;
  if (reg < 0 || reg >= 16)
    reg = 0;
  unsigned char rex = 0;
  if (reg & 8)
    rex = 0x41;
  else if (reg >= 4 && reg <= 7)
    rex = 0x40;
  unsigned char bytes[3] = {0x0f, 0x94, (unsigned char)(0xc0 | (reg & 7))};
  if (strcmp(mnemonic, "sete") == 0 || strcmp(mnemonic, "setz") == 0)
    bytes[1] = 0x94;
  else if (strcmp(mnemonic, "setne") == 0 || strcmp(mnemonic, "setnz") == 0)
    bytes[1] = 0x95;
  else if (strcmp(mnemonic, "setl") == 0)
    bytes[1] = 0x9c;
  else if (strcmp(mnemonic, "setle") == 0)
    bytes[1] = 0x9e;
  else if (strcmp(mnemonic, "setg") == 0)
    bytes[1] = 0x9f;
  else if (strcmp(mnemonic, "setge") == 0)
    bytes[1] = 0x9d;
  else if (strcmp(mnemonic, "setb") == 0)
    bytes[1] = 0x92;
  else if (strcmp(mnemonic, "setbe") == 0)
    bytes[1] = 0x96;
  else if (strcmp(mnemonic, "seta") == 0)
    bytes[1] = 0x97;
  else if (strcmp(mnemonic, "setae") == 0)
    bytes[1] = 0x93;
  else if (strcmp(mnemonic, "setp") == 0 || strcmp(mnemonic, "setpe") == 0)
    bytes[1] = 0x9a;
  else if (strcmp(mnemonic, "setnp") == 0 || strcmp(mnemonic, "setpo") == 0)
    bytes[1] = 0x9b;
  else if (strcmp(mnemonic, "seto") == 0)
    bytes[1] = 0x90;
  else if (strcmp(mnemonic, "setno") == 0)
    bytes[1] = 0x91;
  if (rex)
    seg_append(seg, &rex, 1);
  seg_append(seg, bytes, 3);
}

static void encode_movb(Segment *seg, int src_reg, int dst_reg, int is_mem_src,
                        int is_mem_dst, long long disp) {
  unsigned char rex = 0x40;
  unsigned char opcode;
  unsigned char modrm;

  if (is_mem_src) {
    if (src_reg & 8)
      rex |= 0x01;
    if (dst_reg & 8)
      rex |= 0x04;
  } else {
    if (src_reg & 8)
      rex |= 0x04;
    if (dst_reg & 8)
      rex |= 0x01;
  }

  if (is_mem_src) {
    opcode = 0x8a;
    if (disp >= -128 && disp <= 127) {
      modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
      if (rex != 0x40 || dst_reg >= 4 || src_reg >= 4)
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
      if (rex != 0x40 || dst_reg >= 4 || src_reg >= 4)
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
    opcode = 0x88;
    if (disp >= -128 && disp <= 127) {
      modrm = (0x01 << 6) | ((src_reg & 7) << 3) | (dst_reg & 7);
      if (rex != 0x40 || src_reg >= 4 || dst_reg >= 4)
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
      if (rex != 0x40 || src_reg >= 4 || dst_reg >= 4)
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
    opcode = 0x88;
    modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
    if (rex != 0x40 || src_reg >= 4 || dst_reg >= 4)
      seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
  }
}

static void encode_movw(Segment *seg, int src_reg, int dst_reg, int is_mem_src,
                        int is_mem_dst, long long disp) {
  unsigned char prefix = 0x66;
  unsigned char rex = 0x40;
  unsigned char opcode;
  unsigned char modrm;

  if (is_mem_src) {
    if (src_reg & 8)
      rex |= 0x01;
    if (dst_reg & 8)
      rex |= 0x04;
  } else {
    if (src_reg & 8)
      rex |= 0x04;
    if (dst_reg & 8)
      rex |= 0x01;
  }

  seg_append(seg, &prefix, 1);

  if (is_mem_src) {
    opcode = 0x8b;
    if (disp >= -128 && disp <= 127) {
      modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
      if (rex != 0x40)
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
      if (rex != 0x40)
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
      if (rex != 0x40)
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
      if (rex != 0x40)
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
    modrm = 0xc0 | ((src_reg & 7) << 3) | (dst_reg & 7);
    if (rex != 0x40)
      seg_append(seg, &rex, 1);
    seg_append(seg, &opcode, 1);
    seg_append(seg, &modrm, 1);
  }
}

static void encode_movswq(Segment *seg, int src_reg, int dst_reg,
                          int is_mem_src, long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode[2] = {0x0f, 0xbf};
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

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

static void encode_movzbl(Segment *seg, int src_reg, int dst_reg,
                          int is_mem_src, long long disp) {
  unsigned char rex = 0x40;
  unsigned char opcode[2] = {0x0f, 0xb6};
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

  if (is_mem_src) {
    if (disp >= -128 && disp <= 127) {
      modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
      if (rex != 0x40)
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
      if (rex != 0x40)
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
    if (rex != 0x40)
      seg_append(seg, &rex, 1);
    seg_append(seg, opcode, 2);
    seg_append(seg, &modrm, 1);
  }
}

static void encode_movzwl(Segment *seg, int src_reg, int dst_reg,
                          int is_mem_src, long long disp) {
  unsigned char rex = 0x40;
  unsigned char opcode[2] = {0x0f, 0xb7};
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

  if (is_mem_src) {
    if (disp >= -128 && disp <= 127) {
      modrm = (0x01 << 6) | ((dst_reg & 7) << 3) | (src_reg & 7);
      if (rex != 0x40)
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
      if (rex != 0x40)
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
    if (rex != 0x40)
      seg_append(seg, &rex, 1);
    seg_append(seg, opcode, 2);
    seg_append(seg, &modrm, 1);
  }
}

static void encode_movsbq(Segment *seg, int src_reg, int dst_reg,
                          int is_mem_src, long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode[2] = {0x0f, 0xbe};
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

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

static void encode_movzbq(Segment *seg, int src_reg, int dst_reg,
                          int is_mem_src, long long disp) {
  unsigned char rex = 0x48;
  unsigned char opcode[2] = {0x0f, 0xb6};
  unsigned char modrm;

  if (src_reg & 8)
    rex |= 0x01;
  if (dst_reg & 8)
    rex |= 0x04;

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
      if (s[len - 1] == 'd' || s[len - 1] == 'w' || s[len - 1] == 'b')
        return 0;
      return 1;
    }
  }
  return 0;
}

static void encode_sse_binop(Segment *seg, const char *prefix_bytes,
                             int num_prefixes, unsigned char opcode,
                             int reg_src, int reg_dst) {
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

static void encode_sse_mem(Segment *seg, const char *prefix_bytes,
                           int num_prefixes, unsigned char opcode, int reg_gp,
                           int reg_xmm, int is_load, long long disp) {
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

static void encode_sse_rip(Segment *seg, const char *prefix_bytes,
                           int num_prefixes, unsigned char opcode, int reg_xmm,
                           const char *label_name) {
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

/* =========================================================================
 * AVX-512 512-BIT EVEX (0x62) DIRECT MACHINE-CODE ENCODER
 * Supports %zmm0..%zmm31, SIB addressing, RIP-relative relocations, and
 * compressed disp8*N scaling for Tuple1 (64B) and Tuple1_Scalar (8B).
 * ========================================================================= */
static int parse_evex_mem_operand(const char *s, int *base_reg, int *index_reg,
                                  int *scale_bits, long long *disp, int *is_rip,
                                  char *rip_label) {
  *base_reg = -1;
  *index_reg = -1;
  *scale_bits = 0;
  *disp = 0;
  *is_rip = 0;
  rip_label[0] = '\0';

  const char *paren = strchr(s, '(');
  const char *end_paren = paren ? strchr(paren, ')') : NULL;
  if (!paren || !end_paren)
    return 0;

  char prefix[128];
  size_t plen = (size_t)(paren - s);
  if (plen >= sizeof(prefix))
    plen = sizeof(prefix) - 1;
  memcpy(prefix, s, plen);
  prefix[plen] = '\0';
  char *t_prefix = trim(prefix);

  char inside[128];
  size_t ilen = (size_t)(end_paren - (paren + 1));
  if (ilen >= sizeof(inside))
    ilen = sizeof(inside) - 1;
  memcpy(inside, paren + 1, ilen);
  inside[ilen] = '\0';

  if (strcmp(trim(inside), "%rip") == 0) {
    *is_rip = 1;
    strncpy(rip_label, t_prefix, 127);
    rip_label[127] = '\0';
    return 1;
  }

  if (t_prefix[0] != '\0') {
    *disp = strtoll(t_prefix, NULL, 0);
  }

  char *c1 = strchr(inside, ',');
  if (!c1) {
    *base_reg = parse_reg(trim(inside));
    return (*base_reg >= 0 && *base_reg < 16) ? 1 : 0;
  }
  *c1 = '\0';
  char *c2 = strchr(c1 + 1, ',');
  if (c2)
    *c2 = '\0';

  *base_reg = parse_reg(trim(inside));
  *index_reg = parse_reg(trim(c1 + 1));
  if (*base_reg < 0 || *base_reg >= 16 || *index_reg < 0 || *index_reg >= 16)
    return 0;

  if (c2) {
    int sc = atoi(trim(c2 + 1));
    if (sc == 2)
      *scale_bits = 1;
    else if (sc == 4)
      *scale_bits = 2;
    else if (sc == 8)
      *scale_bits = 3;
    else
      *scale_bits = 0;
  }
  return 1;
}

static void parse_evex_mask_suffix(char *op_str, int *aaa_out, int *z_out) {
  *aaa_out = 0;
  *z_out = 0;
  if (!op_str)
    return;
  char *b1 = strchr(op_str, '{');
  if (!b1)
    return;
  if (strncmp(b1, "{%k", 3) == 0 && b1[3] >= '0' && b1[3] <= '7' &&
      b1[4] == '}') {
    *aaa_out = b1[3] - '0';
    if (strncmp(b1 + 5, "{z}", 3) == 0) {
      *z_out = 1;
    }
    *b1 = '\0';
    trim(op_str);
  }
}

static void encode_evex_mem_ex(Segment *seg, unsigned char mm, unsigned char pp,
                               int w_bit, unsigned char opcode, int zmm_reg,
                               int vvvv_zmm, int base_reg, int index_reg,
                               int scale_bits, long long disp, int is_rip,
                               const char *rip_label, int disp8_scale, int aaa,
                               int z_bit) {
  int hw_reg = (zmm_reg >= 96)
                   ? (zmm_reg - 96)
                   : ((zmm_reg >= 64) ? (zmm_reg - 64) : (zmm_reg & 31));
  int hw_vvvv = (vvvv_zmm >= 64) ? (vvvv_zmm - 64) : 0;

  int R = (hw_reg >> 3) & 1;
  int R_prime = (hw_reg >> 4) & 1;
  int B = (!is_rip && base_reg >= 0) ? ((base_reg >> 3) & 1) : 0;
  int X = (!is_rip && index_reg >= 0) ? ((index_reg >> 3) & 1) : 0;
  int V_prime = (hw_vvvv >> 4) & 1;

  unsigned char evex[4];
  evex[0] = 0x62;
  evex[1] = (unsigned char)(((R ^ 1) << 7) | ((X ^ 1) << 6) | ((B ^ 1) << 5) |
                            ((R_prime ^ 1) << 4) | (mm & 3));
  evex[2] = (unsigned char)(((w_bit & 1) << 7) | (((~hw_vvvv) & 0x0F) << 3) |
                            (1 << 2) | (pp & 3));
  evex[3] = (unsigned char)(((z_bit & 1) << 7) | (2 << 5) |
                            ((V_prime ^ 1) << 3) | (aaa & 7));
  seg_append(seg, evex, 4);
  seg_append(seg, &opcode, 1);

  if (is_rip) {
    unsigned char modrm = (unsigned char)(((hw_reg & 7) << 3) | 5);
    seg_append(seg, &modrm, 1);
    Reloc r;
    r.offset = seg->size;
    strncpy(r.target_name, rip_label, sizeof(r.target_name) - 1);
    r.target_name[sizeof(r.target_name) - 1] = '\0';
    r.type = R_X86_64_PC32;
    r.addend = -4;
    add_reloc(r);
    unsigned char zero[4] = {0, 0, 0, 0};
    seg_append(seg, zero, 4);
    return;
  }

  int need_sib = (index_reg >= 0 || (base_reg & 7) == 4);
  int rm_field = need_sib ? 4 : (base_reg & 7);
  int mod = 0;
  long long scaled_disp8 = 0;

  if (disp == 0 && (base_reg & 7) != 5) {
    mod = 0;
  } else if (disp == 0 && (base_reg & 7) == 5) {
    mod = 1;
    scaled_disp8 = 0;
  } else if (disp8_scale > 0 && (disp % disp8_scale == 0) &&
             (disp / disp8_scale) >= -128 && (disp / disp8_scale) <= 127) {
    mod = 1;
    scaled_disp8 = disp / disp8_scale;
  } else {
    mod = 2;
  }

  unsigned char modrm =
      (unsigned char)((mod << 6) | ((hw_reg & 7) << 3) | rm_field);
  seg_append(seg, &modrm, 1);

  if (need_sib) {
    int idx_field = (index_reg >= 0) ? (index_reg & 7) : 4;
    int sc_field = (index_reg >= 0) ? (scale_bits & 3) : 0;
    unsigned char sib =
        (unsigned char)((sc_field << 6) | (idx_field << 3) | (base_reg & 7));
    seg_append(seg, &sib, 1);
  }

  if (mod == 1) {
    unsigned char d8 = (unsigned char)(scaled_disp8 & 0xFF);
    seg_append(seg, &d8, 1);
  } else if (mod == 2) {
    unsigned char d32[4];
    d32[0] = (unsigned char)(disp & 0xFF);
    d32[1] = (unsigned char)((disp >> 8) & 0xFF);
    d32[2] = (unsigned char)((disp >> 16) & 0xFF);
    d32[3] = (unsigned char)((disp >> 24) & 0xFF);
    seg_append(seg, d32, 4);
  }
}

static void encode_evex_reg_ex(Segment *seg, unsigned char mm, unsigned char pp,
                               int w_bit, unsigned char opcode, int rm_zmm,
                               int vvvv_zmm, int reg_zmm, int aaa, int z_bit) {
  int hw_reg = (reg_zmm >= 96)
                   ? (reg_zmm - 96)
                   : ((reg_zmm >= 64) ? (reg_zmm - 64) : (reg_zmm & 31));
  int hw_vvvv = (vvvv_zmm >= 64) ? (vvvv_zmm - 64) : 0;
  int hw_rm = (rm_zmm >= 96) ? (rm_zmm - 96)
                             : ((rm_zmm >= 64) ? (rm_zmm - 64) : (rm_zmm & 15));

  int R = (hw_reg >> 3) & 1;
  int R_prime = (hw_reg >> 4) & 1;
  int B = (hw_rm >> 3) & 1;
  int X = (hw_rm >> 4) & 1;
  int V_prime = (hw_vvvv >> 4) & 1;

  unsigned char evex[4];
  evex[0] = 0x62;
  evex[1] = (unsigned char)(((R ^ 1) << 7) | ((X ^ 1) << 6) | ((B ^ 1) << 5) |
                            ((R_prime ^ 1) << 4) | (mm & 3));
  evex[2] = (unsigned char)(((w_bit & 1) << 7) | (((~hw_vvvv) & 0x0F) << 3) |
                            (1 << 2) | (pp & 3));
  evex[3] = (unsigned char)(((z_bit & 1) << 7) | (2 << 5) |
                            ((V_prime ^ 1) << 3) | (aaa & 7));
  seg_append(seg, evex, 4);
  seg_append(seg, &opcode, 1);

  unsigned char modrm =
      (unsigned char)(0xC0 | ((hw_reg & 7) << 3) | (hw_rm & 7));
  seg_append(seg, &modrm, 1);
}

static int try_encode_avx512_evex(Segment *seg, const char *mnemonic,
                                  const char *args_raw) {
  if (!mnemonic || !args_raw)
    return 0;
  if (mnemonic[0] != 'v' && mnemonic[0] != 'k')
    return 0;

  char buf[256];
  strncpy(buf, args_raw, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char *ops[4] = {NULL, NULL, NULL, NULL};
  int op_cnt = 0;
  int pdepth = 0;
  char *start = buf;
  for (char *p = buf; *p; p++) {
    if (*p == '(')
      pdepth++;
    else if (*p == ')')
      pdepth--;
    else if (*p == ',' && pdepth == 0) {
      if (op_cnt < 3) {
        *p = '\0';
        ops[op_cnt++] = trim(start);
        start = p + 1;
      }
    }
  }
  ops[op_cnt++] = trim(start);

  /* Opmask kmovw / kmovb / kmovd / kmovq */
  if (op_cnt == 2 &&
      (strcmp(mnemonic, "kmovw") == 0 || strcmp(mnemonic, "kmovb") == 0 ||
       strcmp(mnemonic, "kmovd") == 0 || strcmp(mnemonic, "kmovq") == 0)) {
    int r0 = parse_reg(ops[0]);
    int r1 = parse_reg(ops[1]);
    unsigned char pp = 0;
    int w_bit = 0;
    if (strcmp(mnemonic, "kmovb") == 0)
      pp = 1;
    else if (strcmp(mnemonic, "kmovd") == 0) {
      pp = 3;
      w_bit = 0;
    } else if (strcmp(mnemonic, "kmovq") == 0) {
      pp = 3;
      w_bit = 1;
    }

    if (r0 >= 0 && r0 < 16 && r1 >= 96 && r1 <= 103) {
      /* GPR -> %k: VEX.L0.0F 0x92 /r */
      int k_dst = r1 - 96;
      if ((r0 & 8) == 0 && w_bit == 0) {
        unsigned char vex[4] = {
            0xC5, (unsigned char)(0xF8 | (pp & 3)), 0x92,
            (unsigned char)(0xC0 | (k_dst << 3) | (r0 & 7))};
        seg_append(seg, vex, 4);
      } else {
        unsigned char b_inv = ((r0 >> 3) & 1) ? 0 : 1;
        unsigned char vex3[5] = {
            0xC4, (unsigned char)(0xC1 | (b_inv << 5)),
            (unsigned char)(((w_bit & 1) << 7) | 0x78 | (pp & 3)), 0x92,
            (unsigned char)(0xC0 | (k_dst << 3) | (r0 & 7))};
        seg_append(seg, vex3, 5);
      }
      return 1;
    }
    if (r0 >= 96 && r0 <= 103 && r1 >= 0 && r1 < 16) {
      /* %k -> GPR: VEX.L0.0F 0x93 /r */
      int k_src = r0 - 96;
      if ((r1 & 8) == 0 && w_bit == 0) {
        unsigned char vex[4] = {
            0xC5, (unsigned char)(0xF8 | (pp & 3)), 0x93,
            (unsigned char)(0xC0 | ((r1 & 7) << 3) | k_src)};
        seg_append(seg, vex, 4);
      } else {
        unsigned char r_inv = ((r1 >> 3) & 1) ? 0 : 1;
        unsigned char vex3[5] = {
            0xC4, (unsigned char)(0x61 | (r_inv << 7)),
            (unsigned char)(((w_bit & 1) << 7) | 0x78 | (pp & 3)), 0x93,
            (unsigned char)(0xC0 | ((r1 & 7) << 3) | k_src)};
        seg_append(seg, vex3, 5);
      }
      return 1;
    }
    if (r0 >= 96 && r0 <= 103 && r1 >= 96 && r1 <= 103) {
      int k_src = r0 - 96, k_dst = r1 - 96;
      unsigned char vex[4] = {0xC5, (unsigned char)(0xF8 | (pp & 3)), 0x90,
                              (unsigned char)(0xC0 | (k_dst << 3) | k_src)};
      seg_append(seg, vex, 4);
      return 1;
    }
  }

  int aaa = 0, z_bit = 0;
  parse_evex_mask_suffix(ops[op_cnt - 1], &aaa, &z_bit);

  /* 2-operand vector moves:
   * vmovupd/vmovapd/vmovups/vmovaps/vmovdqu64/vmovdqa64/vmovdqu32/vmovdqa32 */
  if (op_cnt == 2) {
    unsigned char pp = 0, load_op = 0, store_op = 0;
    int w_bit = 1, is_mov = 0;
    if (strcmp(mnemonic, "vmovupd") == 0) {
      pp = 1;
      w_bit = 1;
      load_op = 0x10;
      store_op = 0x11;
      is_mov = 1;
    } else if (strcmp(mnemonic, "vmovapd") == 0) {
      pp = 1;
      w_bit = 1;
      load_op = 0x28;
      store_op = 0x29;
      is_mov = 1;
    } else if (strcmp(mnemonic, "vmovups") == 0) {
      pp = 0;
      w_bit = 0;
      load_op = 0x10;
      store_op = 0x11;
      is_mov = 1;
    } else if (strcmp(mnemonic, "vmovaps") == 0) {
      pp = 0;
      w_bit = 0;
      load_op = 0x28;
      store_op = 0x29;
      is_mov = 1;
    } else if (strcmp(mnemonic, "vmovdqu64") == 0) {
      pp = 2;
      w_bit = 1;
      load_op = 0x6F;
      store_op = 0x7F;
      is_mov = 1;
    } else if (strcmp(mnemonic, "vmovdqa64") == 0) {
      pp = 1;
      w_bit = 1;
      load_op = 0x6F;
      store_op = 0x7F;
      is_mov = 1;
    } else if (strcmp(mnemonic, "vmovdqu32") == 0) {
      pp = 2;
      w_bit = 0;
      load_op = 0x6F;
      store_op = 0x7F;
      is_mov = 1;
    } else if (strcmp(mnemonic, "vmovdqa32") == 0) {
      pp = 1;
      w_bit = 0;
      load_op = 0x6F;
      store_op = 0x7F;
      is_mov = 1;
    }

    if (is_mov) {
      int r0 = parse_reg(ops[0]);
      int r1 = parse_reg(ops[1]);
      if (r0 >= 64 && r1 >= 64) {
        encode_evex_reg_ex(seg, 1, pp, w_bit, load_op, r0, 0, r1, aaa, z_bit);
        return 1;
      }
      int base = -1, idx = -1, sc = 0, is_rip = 0;
      long long disp = 0;
      char rip_lbl[128];
      if (r1 >= 64 && parse_evex_mem_operand(ops[0], &base, &idx, &sc, &disp,
                                             &is_rip, rip_lbl)) {
        encode_evex_mem_ex(seg, 1, pp, w_bit, load_op, r1, 0, base, idx, sc,
                           disp, is_rip, rip_lbl, 64, aaa, z_bit);
        return 1;
      }
      if (r0 >= 64 && parse_evex_mem_operand(ops[1], &base, &idx, &sc, &disp,
                                             &is_rip, rip_lbl)) {
        encode_evex_mem_ex(seg, 1, pp, w_bit, store_op, r0, 0, base, idx, sc,
                           disp, is_rip, rip_lbl, 64, aaa, 0);
        return 1;
      }
    }
  }

  /* 2-operand broadcasts: vbroadcastsd, vbroadcastss, vpbroadcastq,
   * vpbroadcastd */
  if (op_cnt == 2 && (strcmp(mnemonic, "vbroadcastsd") == 0 ||
                      strcmp(mnemonic, "vbroadcastss") == 0 ||
                      strcmp(mnemonic, "vpbroadcastq") == 0 ||
                      strcmp(mnemonic, "vpbroadcastd") == 0)) {
    int is_64 = (strcmp(mnemonic, "vbroadcastsd") == 0 ||
                 strcmp(mnemonic, "vpbroadcastq") == 0);
    int is_int = (strncmp(mnemonic, "vp", 2) == 0);
    unsigned char vec_op =
        is_int ? (is_64 ? 0x59 : 0x58) : (is_64 ? 0x19 : 0x18);
    int disp8_sc = is_64 ? 8 : 4;
    int r0 = parse_reg(ops[0]);
    int r1 = parse_reg(ops[1]);
    if (r1 >= 64 && (r0 >= 16 && r0 <= 31)) {
      encode_evex_reg_ex(seg, 2, 1, is_64, vec_op, r0, 0, r1, aaa, z_bit);
      return 1;
    }
    if (is_int && r1 >= 64 && (r0 >= 0 && r0 < 16)) {
      /* vpbroadcastq %r64, %zmm (0x7C W=1) / vpbroadcastd %r32, %zmm (0x7C W=0)
       */
      encode_evex_reg_ex(seg, 2, 1, is_64, 0x7C, r0, 0, r1, aaa, z_bit);
      return 1;
    }
    int base = -1, idx = -1, sc = 0, is_rip = 0;
    long long disp = 0;
    char rip_lbl[128];
    if (r1 >= 64 && parse_evex_mem_operand(ops[0], &base, &idx, &sc, &disp,
                                           &is_rip, rip_lbl)) {
      encode_evex_mem_ex(seg, 2, 1, is_64, vec_op, r1, 0, base, idx, sc, disp,
                         is_rip, rip_lbl, disp8_sc, aaa, z_bit);
      return 1;
    }
  }

  /* 3-operand immediate shifts: vpsllq, vpsrlq, vpsraq, vpslld, vpsrld, vpsrad
   */
  if (op_cnt == 3 && ops[0][0] == '$' &&
      (strcmp(mnemonic, "vpsllq") == 0 || strcmp(mnemonic, "vpsrlq") == 0 ||
       strcmp(mnemonic, "vpsraq") == 0 || strcmp(mnemonic, "vpslld") == 0 ||
       strcmp(mnemonic, "vpsrld") == 0 || strcmp(mnemonic, "vpsrad") == 0)) {
    int imm8 = (int)strtoll(ops[0] + 1, NULL, 0) & 0xFF;
    int r_src = parse_reg(ops[1]);
    int r_dst = parse_reg(ops[2]);
    if (r_src >= 64 && r_dst >= 64) {
      int is_q = (mnemonic[5] == 'q');
      unsigned char opcode = is_q ? 0x73 : 0x72;
      int subop = 6;
      if (strncmp(mnemonic, "vpsrl", 5) == 0)
        subop = 2;
      else if (strncmp(mnemonic, "vpsra", 5) == 0) {
        subop = 4;
        opcode = 0x72;
      }
      encode_evex_reg_ex(seg, 1, 1, is_q, opcode, r_src, r_dst, subop, aaa,
                         z_bit);
      unsigned char ib = (unsigned char)imm8;
      seg_append(seg, &ib, 1);
      return 1;
    }
  }

  /* 4-operand or 3-operand vector compares into %k0..%k7: vcmppd, vcmpps,
   * vcmpeqpd, vpcmpq, vpcmpuq, vpcmpd, vpcmpud */
  if ((op_cnt == 4 && ops[0][0] == '$') ||
      (op_cnt == 3 && strncmp(mnemonic, "vcmp", 4) == 0)) {
    int imm_cmp = -1;
    unsigned char mm = 0, pp = 1, opcode = 0;
    int w_bit = 1;
    int src2_idx = (op_cnt == 4) ? 1 : 0;
    if (op_cnt == 4) {
      imm_cmp = (int)strtoll(ops[0] + 1, NULL, 0) & 0xFF;
      if (strcmp(mnemonic, "vcmppd") == 0) {
        mm = 1;
        pp = 1;
        w_bit = 1;
        opcode = 0xC2;
      } else if (strcmp(mnemonic, "vcmpps") == 0) {
        mm = 1;
        pp = 0;
        w_bit = 0;
        opcode = 0xC2;
      } else if (strcmp(mnemonic, "vpcmpq") == 0) {
        mm = 3;
        pp = 1;
        w_bit = 1;
        opcode = 0x1F;
      } else if (strcmp(mnemonic, "vpcmpuq") == 0) {
        mm = 3;
        pp = 1;
        w_bit = 1;
        opcode = 0x1E;
      } else if (strcmp(mnemonic, "vpcmpd") == 0) {
        mm = 3;
        pp = 1;
        w_bit = 0;
        opcode = 0x1F;
      } else if (strcmp(mnemonic, "vpcmpud") == 0) {
        mm = 3;
        pp = 1;
        w_bit = 0;
        opcode = 0x1E;
      }
    } else if (strcmp(mnemonic, "vcmpeqpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xC2;
      imm_cmp = 0;
    } else if (strcmp(mnemonic, "vcmpltpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xC2;
      imm_cmp = 1;
    }
    if (mm != 0 && imm_cmp >= 0) {
      int r_src2 = parse_reg(ops[src2_idx]);
      int r_src1 = parse_reg(ops[src2_idx + 1]);
      int r_kdst = parse_reg(ops[src2_idx + 2]);
      if (r_src2 >= 64 && r_src1 >= 64 && r_kdst >= 96 && r_kdst <= 103) {
        encode_evex_reg_ex(seg, mm, pp, w_bit, opcode, r_src2, r_src1, r_kdst,
                           aaa, 0);
        unsigned char ib = (unsigned char)imm_cmp;
        seg_append(seg, &ib, 1);
        return 1;
      }
    }
  }

  if (op_cnt == 3) {
    unsigned char mm = 0, pp = 1, opcode = 0;
    int w_bit = 1;
    /* Packed double (pd) */
    if (strcmp(mnemonic, "vaddpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x58;
    } else if (strcmp(mnemonic, "vmulpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x59;
    } else if (strcmp(mnemonic, "vsubpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x5C;
    } else if (strcmp(mnemonic, "vminpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x5D;
    } else if (strcmp(mnemonic, "vdivpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x5E;
    } else if (strcmp(mnemonic, "vmaxpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x5F;
    } else if (strcmp(mnemonic, "vandpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x54;
    } else if (strcmp(mnemonic, "vandnpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x55;
    } else if (strcmp(mnemonic, "vorpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x56;
    } else if (strcmp(mnemonic, "vxorpd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0x57;
    } else if (strcmp(mnemonic, "vfmadd132pd") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0x98;
    } else if (strcmp(mnemonic, "vfmadd213pd") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0xA8;
    } else if (strcmp(mnemonic, "vfmadd231pd") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0xB8;
    } else if (strcmp(mnemonic, "vfmsub231pd") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0xBA;
    } else if (strcmp(mnemonic, "vfnmadd231pd") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0xBC;
    }
    /* Packed float (ps) */
    else if (strcmp(mnemonic, "vaddps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x58;
    } else if (strcmp(mnemonic, "vmulps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x59;
    } else if (strcmp(mnemonic, "vsubps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x5C;
    } else if (strcmp(mnemonic, "vminps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x5D;
    } else if (strcmp(mnemonic, "vdivps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x5E;
    } else if (strcmp(mnemonic, "vmaxps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x5F;
    } else if (strcmp(mnemonic, "vandps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x54;
    } else if (strcmp(mnemonic, "vandnps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x55;
    } else if (strcmp(mnemonic, "vorps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x56;
    } else if (strcmp(mnemonic, "vxorps") == 0) {
      mm = 1;
      pp = 0;
      w_bit = 0;
      opcode = 0x57;
    } else if (strcmp(mnemonic, "vfmadd132ps") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 0;
      opcode = 0x98;
    } else if (strcmp(mnemonic, "vfmadd213ps") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 0;
      opcode = 0xA8;
    } else if (strcmp(mnemonic, "vfmadd231ps") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 0;
      opcode = 0xB8;
    } else if (strcmp(mnemonic, "vfmsub231ps") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 0;
      opcode = 0xBA;
    } else if (strcmp(mnemonic, "vfnmadd231ps") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 0;
      opcode = 0xBC;
    }
    /* Packed 64-bit integer (epi64 / q) */
    else if (strcmp(mnemonic, "vpaddq") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xD4;
    } else if (strcmp(mnemonic, "vpsubq") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xFB;
    } else if (strcmp(mnemonic, "vpmullq") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0x40;
    } else if (strcmp(mnemonic, "vpandq") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xDB;
    } else if (strcmp(mnemonic, "vpandnq") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xDF;
    } else if (strcmp(mnemonic, "vporq") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xEB;
    } else if (strcmp(mnemonic, "vpxorq") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 1;
      opcode = 0xEF;
    } else if (strcmp(mnemonic, "vpminsq") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0x38;
    } else if (strcmp(mnemonic, "vpmaxsq") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0x3D;
    } else if (strcmp(mnemonic, "vpminuq") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0x3A;
    } else if (strcmp(mnemonic, "vpmaxuq") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 1;
      opcode = 0x3F;
    }
    /* Packed 32-bit integer (epi32 / d) */
    else if (strcmp(mnemonic, "vpaddd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 0;
      opcode = 0xFE;
    } else if (strcmp(mnemonic, "vpsubd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 0;
      opcode = 0xFA;
    } else if (strcmp(mnemonic, "vpmulld") == 0) {
      mm = 2;
      pp = 1;
      w_bit = 0;
      opcode = 0x40;
    } else if (strcmp(mnemonic, "vpandd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 0;
      opcode = 0xDB;
    } else if (strcmp(mnemonic, "vpandnd") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 0;
      opcode = 0xDF;
    } else if (strcmp(mnemonic, "vpord") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 0;
      opcode = 0xEB;
    } else if (strcmp(mnemonic, "vpxord") == 0) {
      mm = 1;
      pp = 1;
      w_bit = 0;
      opcode = 0xEF;
    }

    if (mm != 0) {
      /* AT&T syntax: mnemonic src2(r/m), src1(vvvv), dst(reg){%k1}{z} */
      int r_src2 = parse_reg(ops[0]);
      int r_src1 = parse_reg(ops[1]);
      int r_dst = parse_reg(ops[2]);
      if (r_src1 >= 64 && r_dst >= 64) {
        if (r_src2 >= 64) {
          encode_evex_reg_ex(seg, mm, pp, w_bit, opcode, r_src2, r_src1, r_dst,
                             aaa, z_bit);
          return 1;
        }
        int base = -1, idx = -1, sc = 0, is_rip = 0;
        long long disp = 0;
        char rip_lbl[128];
        if (parse_evex_mem_operand(ops[0], &base, &idx, &sc, &disp, &is_rip,
                                   rip_lbl)) {
          encode_evex_mem_ex(seg, mm, pp, w_bit, opcode, r_dst, r_src1, base,
                             idx, sc, disp, is_rip, rip_lbl, 64, aaa, z_bit);
          return 1;
        }
      }
    }
  }
  return 0;
}

static void encode_movq_xmm(Segment *seg, int reg1, int reg2) {
  unsigned char p66 = 0x66;
  seg_append(seg, &p66, 1);

  unsigned char rex = 0x48;
  int hw1 = reg1 & 15;
  int hw2 = reg2 & 15;

  if (reg1 < 16 && reg2 >= 16) {
    if (hw1 & 8)
      rex |= 0x01;
    if (hw2 & 8)
      rex |= 0x04;
    seg_append(seg, &rex, 1);
    unsigned char op[2] = {0x0f, 0x6e};
    seg_append(seg, op, 2);
    unsigned char modrm = 0xc0 | ((hw2 & 7) << 3) | (hw1 & 7);
    seg_append(seg, &modrm, 1);
  } else if (reg1 >= 16 && reg2 < 16) {
    if (hw1 & 8)
      rex |= 0x04;
    if (hw2 & 8)
      rex |= 0x01;
    seg_append(seg, &rex, 1);
    unsigned char op[2] = {0x0f, 0x7e};
    seg_append(seg, op, 2);
    unsigned char modrm = 0xc0 | ((hw1 & 7) << 3) | (hw2 & 7);
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
    if (hw1 & 8) {
      rex |= 0x01;
      need_rex = 1;
    }
    if (hw2 & 8) {
      rex |= 0x04;
      need_rex = 1;
    }
    if (need_rex)
      seg_append(seg, &rex, 1);
    unsigned char op[2] = {0x0f, 0x6e};
    seg_append(seg, op, 2);
    unsigned char modrm = 0xc0 | ((hw2 & 7) << 3) | (hw1 & 7);
    seg_append(seg, &modrm, 1);
  } else if (reg1 >= 16 && reg2 < 16) {
    if (hw1 & 8) {
      rex |= 0x04;
      need_rex = 1;
    }
    if (hw2 & 8) {
      rex |= 0x01;
      need_rex = 1;
    }
    if (need_rex)
      seg_append(seg, &rex, 1);
    unsigned char op[2] = {0x0f, 0x7e};
    seg_append(seg, op, 2);
    unsigned char modrm = 0xc0 | ((hw1 & 7) << 3) | (hw2 & 7);
    seg_append(seg, &modrm, 1);
  }
}

static void encode_cvtt_to_si(Segment *seg, const char *pref, int n_pref,
                              unsigned char opcode, int reg_xmm, int reg_gp,
                              int use_rex_w) {
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

static void encode_cvtt_to_si_mem(Segment *seg, const char *pref, int n_pref,
                                  unsigned char opcode, int reg_gp_base,
                                  int reg_gp_dst, int use_rex_w,
                                  long long disp) {
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

static void encode_cvtsi_to_xmm(Segment *seg, const char *pref, int n_pref,
                                unsigned char opcode, int reg_gp, int reg_xmm,
                                int use_rex_w) {
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

static void encode_cvtsi_to_xmm_mem(Segment *seg, const char *pref, int n_pref,
                                    unsigned char opcode, int reg_gp_base,
                                    int reg_xmm, int use_rex_w,
                                    long long disp) {
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
  while (isspace((unsigned char)*s))
    s++;
  if (*s == 0)
    return s;
  end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return s;
}

static char **g_direct_asm_lines = NULL;
static size_t g_direct_asm_line_count = 0;
static size_t g_direct_asm_line_cap = 0;

void zcc_direct_assemble_line(const char *line) {
  if (g_direct_asm_line_count >= g_direct_asm_line_cap) {
    g_direct_asm_line_cap =
        g_direct_asm_line_cap ? g_direct_asm_line_cap * 2 : 1024;
    g_direct_asm_lines = (char **)realloc(
        g_direct_asm_lines, g_direct_asm_line_cap * sizeof(char *));
  }
  g_direct_asm_lines[g_direct_asm_line_count++] = strdup(line);
}

/* Two-Pass Assembler Core */
static int assemble(const char *in_s_filename, const char *out_o_filename,
                    const char *in_mem_buf, size_t mem_buf_len,
                    int is_win64_pe) {
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
  Segment text_seg = {NULL, 0, 0};
  Segment data_seg = {NULL, 0, 0};
  Segment tls_seg = {NULL, 0, 0};
  size_t tls_zero_fill = 0;
#define CUR_DATA_SEG ((current_segment == 4) ? &tls_seg : &data_seg)
  char line[65536];
  int current_segment = 1; /* 1 = .text, 2 = .data, 4 = .tls */

  ensure_codegen_tables();
  label_count = 0;
  reloc_count = 0;
  memset(label_hash_slot, -1, sizeof(label_hash_slot));
  g_asm_file_count = 0;
  g_asm_loc_count = 0;
  g_current_file_idx = 1;
  g_current_line = 1;
  g_loc_pending = 0;
  memset(g_num_label_gen, 0, sizeof(g_num_label_gen));

  /* Pass 1: Parse and encode */
  size_t line_idx = 0;
  while (1) {
    if (g_direct_asm_lines && line_idx < g_direct_asm_line_count) {
      strncpy(line, g_direct_asm_lines[line_idx++], sizeof(line) - 1);
      line[sizeof(line) - 1] = '\0';
    } else if (in) {
      if (!fgets(line, sizeof(line), in))
        break;
    } else {
      break;
    }
    {
      int in_str = 0;
      char *scan;
      for (scan = line; *scan; scan++) {
        if (*scan == '\\' && in_str && scan[1]) {
          scan++;
          continue;
        }
        if (*scan == '"') {
          in_str = !in_str;
          continue;
        }
        if (!in_str && (*scan == '#' || *scan == ';')) {
          *scan = '\0';
          break;
        }
      }
    }
    char *p;

    p = trim(line);
    if (*p == '\0')
      continue;

    /* Is it a label? (Must not have whitespace before colon) */
    char *colon = strchr(p, ':');
    char *lbl_space = strchr(p, ' ');
    char *lbl_tab = strchr(p, '\t');
    char *first_ws = (lbl_space && lbl_tab)
                         ? (lbl_space < lbl_tab ? lbl_space : lbl_tab)
                         : (lbl_space ? lbl_space : lbl_tab);
    if (colon && (!first_ws || colon < first_ws) && !strchr(p, '"')) {
      *colon = '\0';
      char *lbl_name = trim(p);
      char num_lbl_buf[64];
      if (lbl_name[0] >= '0' && lbl_name[0] <= '9' && lbl_name[1] == '\0') {
        int d = lbl_name[0] - '0';
        g_num_label_gen[d]++;
        snprintf(num_lbl_buf, sizeof(num_lbl_buf), ".Lnum_%d_%d", d,
                 g_num_label_gen[d]);
        lbl_name = num_lbl_buf;
      }
      /* Add or update label */
      int found_idx = find_label_idx(lbl_name);
      size_t idx;
      if (found_idx < 0) {
        if (label_count >= 131072) {
          fprintf(stderr,
                  "error: assembler label table limit exceeded (max 131072)\n");
          exit(1);
        }
        idx = label_count++;
        strcpy(labels[idx].name, lbl_name);
        labels[idx].is_global = 0;
        register_label_hash(idx);
      } else {
        idx = (size_t)found_idx;
      }
      if (current_segment == 1)
        labels[idx].offset = text_seg.size;
      else if (current_segment == 4)
        labels[idx].offset = tls_seg.size;
      else
        labels[idx].offset = data_seg.size;
      labels[idx].segment = current_segment;
      continue;
    }

    /* Is it a directive? */
    if (*p == '.') {
      char dir[64];
      int n = sscanf(p, "%63s", dir);
      if (n <= 0)
        continue;

      if (strcmp(dir, ".text") == 0) {
        current_segment = 1;
      } else if (strcmp(dir, ".data") == 0 || strcmp(dir, ".section") == 0 ||
                 strcmp(dir, ".rodata") == 0 || strcmp(dir, ".bss") == 0) {
        if (strstr(p, ".tdata") || strstr(p, ".tbss")) {
          current_segment = 4;
        } else {
          current_segment = 2;
        }
      } else if (strcmp(dir, ".globl") == 0 || strcmp(dir, ".global") == 0) {
        char name[128];
        sscanf(p + strlen(dir), "%127s", name);
        char *gname = trim(name);
        int found_idx = find_label_idx(gname);
        if (found_idx >= 0) {
          labels[found_idx].is_global = 1;
        } else {
          if (label_count >= 131072) {
            fprintf(
                stderr,
                "error: assembler label table limit exceeded (max 131072)\n");
            exit(1);
          }
          size_t idx = label_count++;
          strcpy(labels[idx].name, gname);
          labels[idx].is_global = 1;
          labels[idx].segment = 0;
          register_label_hash(idx);
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
          int existing_idx = find_label_idx(name);
          size_t idx;
          if (existing_idx >= 0) {
            idx = (size_t)existing_idx;
          } else {
            if (label_count >= 131072) {
              fprintf(
                  stderr,
                  "error: assembler label table limit exceeded (max 131072)\n");
              exit(1);
            }
            idx = label_count++;
            strcpy(labels[idx].name, name);
            register_label_hash(idx);
          }
          labels[idx].is_global = 1;
          labels[idx].segment = 3; /* Common */
          labels[idx].offset = align;
          labels[idx].size = size;
          if (current_segment == 4) {
            tls_zero_fill += (size_t)size;
          }
        }
      } else if (strcmp(dir, ".long") == 0) {
        long long val = strtoll(p + 5, NULL, 0);
        unsigned char b[4];
        b[0] = val & 0xFF;
        b[1] = (val >> 8) & 0xFF;
        b[2] = (val >> 16) & 0xFF;
        b[3] = (val >> 24) & 0xFF;
        seg_append(CUR_DATA_SEG, b, 4);
      } else if (strcmp(dir, ".quad") == 0) {
        /* Check if it's a label or constant */
        char target[128];
        long long val = 0;
        char *arg = trim(p + 5);
        if (isalpha((unsigned char)*arg) || *arg == '.' || *arg == '_') {
          sscanf(arg, "%127s", target);
          Reloc r;
          r.offset = CUR_DATA_SEG->size;
          strcpy(r.target_name, target);
          r.type = R_X86_64_64;
          r.addend = 0;
          add_reloc(r);
          unsigned char zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
          seg_append(CUR_DATA_SEG, zero, 8);
        } else {
          val = (*arg == '-') ? strtoll(arg, NULL, 0) : (long long)strtoull(arg, NULL, 0);
          unsigned char b[8];
          int i;
          for (i = 0; i < 8; i++) {
            b[i] = (val >> (i * 8)) & 0xFF;
          }
          seg_append(CUR_DATA_SEG, b, 8);
        }
      } else if (strcmp(dir, ".byte") == 0) {
        int val = (int)strtoll(p + 5, NULL, 0);
        unsigned char b = (unsigned char)val;
        seg_append(CUR_DATA_SEG, &b, 1);
      } else if (strcmp(dir, ".short") == 0) {
        int val = (int)strtoll(p + 6, NULL, 0);
        unsigned char b[2];
        b[0] = val & 0xFF;
        b[1] = (val >> 8) & 0xFF;
        seg_append(CUR_DATA_SEG, b, 2);
      } else if (strcmp(dir, ".zero") == 0) {
        long long size = 0;
        sscanf(p + 5, "%lld", &size);
        if (size > 0) {
          unsigned char *zeros = (unsigned char *)calloc(1, size);
          seg_append(CUR_DATA_SEG, zeros, size);
          free(zeros);
        }
      } else if (strcmp(dir, ".align") == 0 || strcmp(dir, ".p2align") == 0) {
        long long align = 0;
        sscanf(p + strlen(dir), "%lld", &align);
        if (strcmp(dir, ".p2align") == 0 && align > 0) {
          align = 1LL << align;
        }
        if (align > 0) {
          size_t pad_size = (align - (CUR_DATA_SEG->size % align)) % align;
          if (pad_size > 0) {
            unsigned char *zeros = (unsigned char *)calloc(1, pad_size);
            seg_append(CUR_DATA_SEG, zeros, pad_size);
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
      } else if (strcmp(dir, ".ascii") == 0 || strcmp(dir, ".asciz") == 0 ||
                 strcmp(dir, ".string") == 0) {
        char *str_start = strchr(p, '"');
        if (str_start) {
          char *src = str_start + 1;
          while (*src && *src != '"') {
            if (*src == '\\') {
              src++;
              if (*src == 'n') {
                seg_append(&data_seg, (const unsigned char *)"\n", 1);
              } else if (*src == 't') {
                seg_append(&data_seg, (const unsigned char *)"\t", 1);
              } else if (*src == 'r') {
                seg_append(&data_seg, (const unsigned char *)"\r", 1);
              } else if (*src == '\\') {
                seg_append(&data_seg, (const unsigned char *)"\\", 1);
              } else if (*src == '"') {
                seg_append(&data_seg, (const unsigned char *)"\"", 1);
              } else if (*src >= '0' && *src <= '7') {
                unsigned int ov = (unsigned int)(*src - '0');
                int oc;
                for (oc = 0; oc < 2 && src[1] >= '0' && src[1] <= '7'; oc++) {
                  src++;
                  ov = (ov << 3) | (unsigned int)(*src - '0');
                }
                unsigned char ob = (unsigned char)(ov & 0xFF);
                seg_append(&data_seg, &ob, 1);
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
    if (!space)
      space = strchr(p, '\t');

    if (space) {
      size_t mlen = space - p;
      if (mlen >= 63)
        mlen = 63;
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
    } else if (strcmp(mnemonic, "rep") == 0 && args_start &&
               strcmp(trim(args_start), "movsb") == 0) {
      unsigned char b[2] = {0xf3, 0xa4};
      seg_append(&text_seg, b, 2);
      matched = 1;
    } else if (strcmp(mnemonic, "rep") == 0 && args_start &&
               strcmp(trim(args_start), "stosb") == 0) {
      unsigned char b[2] = {0xf3, 0xaa};
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
    } else if (strcmp(mnemonic, "nop") == 0) {
      unsigned char b = 0x90;
      seg_append(&text_seg, &b, 1);
      matched = 1;
    } else if (strcmp(mnemonic, "cltq") == 0) {
      unsigned char b[2] = {0x48, 0x98};
      seg_append(&text_seg, b, 2);
      matched = 1;
    } else if (strcmp(mnemonic, "vzeroupper") == 0) {
      unsigned char b[3] = {0xc5, 0xf8, 0x77};
      seg_append(&text_seg, b, 3);
      matched = 1;
    } else if (strcmp(mnemonic, "syscall") == 0) {
      unsigned char b[2] = {0x0f, 0x05};
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
        fprintf(stderr,
                "assembler error: invalid register/operand '%s' in pushq\n",
                op);
        exit(1);
      }
      if (is_mem) {
        unsigned char rex = 0x40;
        if (reg & 8)
          rex |= 0x01; /* REX.B */
        unsigned char opcode = 0xff;
        unsigned char modrm;
        if (disp >= -128 && disp <= 127) {
          modrm = (0x01 << 6) | (6 << 3) | (reg & 7);
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
          seg_append(&text_seg, &opcode, 1);
          seg_append(&text_seg, &modrm, 1);
          if ((reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(&text_seg, &sib, 1);
          }
          unsigned char d = (unsigned char)disp;
          seg_append(&text_seg, &d, 1);
        } else {
          modrm = (0x02 << 6) | (6 << 3) | (reg & 7);
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
          seg_append(&text_seg, &opcode, 1);
          seg_append(&text_seg, &modrm, 1);
          if ((reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(&text_seg, &sib, 1);
          }
          unsigned char d[4];
          d[0] = disp & 0xFF;
          d[1] = (disp >> 8) & 0xFF;
          d[2] = (disp >> 16) & 0xFF;
          d[3] = (disp >> 24) & 0xFF;
          seg_append(&text_seg, d, 4);
        }
      } else {
        if (reg == 5) {
          unsigned char b = 0x55;
          seg_append(&text_seg, &b, 1);
        } else {
          unsigned char rex = 0x40 | (reg >> 3);
          unsigned char opcode = 0x50 | (reg & 7);
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
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
        fprintf(stderr,
                "assembler error: invalid register/operand '%s' in popq\n", op);
        exit(1);
      }
      if (is_mem) {
        unsigned char rex = 0x40;
        if (reg & 8)
          rex |= 0x01; /* REX.B */
        unsigned char opcode = 0x8f;
        unsigned char modrm;
        if (disp >= -128 && disp <= 127) {
          modrm = (0x01 << 6) | (0 << 3) | (reg & 7);
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
          seg_append(&text_seg, &opcode, 1);
          seg_append(&text_seg, &modrm, 1);
          if ((reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(&text_seg, &sib, 1);
          }
          unsigned char d = (unsigned char)disp;
          seg_append(&text_seg, &d, 1);
        } else {
          modrm = (0x02 << 6) | (0 << 3) | (reg & 7);
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
          seg_append(&text_seg, &opcode, 1);
          seg_append(&text_seg, &modrm, 1);
          if ((reg & 7) == 4) {
            unsigned char sib = 0x24;
            seg_append(&text_seg, &sib, 1);
          }
          unsigned char d[4];
          d[0] = disp & 0xFF;
          d[1] = (disp >> 8) & 0xFF;
          d[2] = (disp >> 16) & 0xFF;
          d[3] = (disp >> 24) & 0xFF;
          seg_append(&text_seg, d, 4);
        }
      } else {
        if (reg == 5) {
          unsigned char b = 0x5d;
          seg_append(&text_seg, &b, 1);
        } else {
          unsigned char rex = 0x40 | (reg >> 3);
          unsigned char opcode = 0x58 | (reg & 7);
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
          seg_append(&text_seg, &opcode, 1);
        }
      }
      matched = 1;
    } else if (strcmp(mnemonic, "call") == 0 ||
               strcmp(mnemonic, "callq") == 0) {
      char *target = args_start ? trim(args_start) : "";
      /* CG-FNPTR-001: detect register-indirect call "call *%rN"
       * These need FF/2 ModRM encoding, not a PLT32 relocation. */
      if (target[0] == '*') {
        int ireg = parse_reg(target + 1); /* skip '*', parse %rN */
        if (ireg == -1) {
          fprintf(stderr,
                  "assembler error: invalid register '%s' in indirect call\n",
                  target);
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
      char *op = args_start ? trim(args_start) : "%al";
      encode_set(&text_seg, mnemonic, op);
      matched = 1;
    } else if (strcmp(mnemonic, "idivq") == 0 ||
               strcmp(mnemonic, "idivl") == 0 ||
               strcmp(mnemonic, "idivw") == 0 ||
               strcmp(mnemonic, "idivb") == 0) {
      char *op = args_start ? trim(args_start) : "";
      int reg = parse_reg(op);
      if (reg < 0) {
        fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op,
                mnemonic);
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
      } else if (size_char == 'b' &&
                 (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
        rex = 0x40;
      }

      if (reg & 8)
        rex |= 0x01;

      if (size_char == 'b') {
        opcode = 0xf6;
      } else {
        opcode = 0xf7;
      }

      if (prefix)
        seg_append(&text_seg, &prefix, 1);
      if (rex)
        seg_append(&text_seg, &rex, 1);
      seg_append(&text_seg, &opcode, 1);
      seg_append(&text_seg, &modrm, 1);
      matched = 1;
    } else if (strcmp(mnemonic, "divq") == 0 || strcmp(mnemonic, "divl") == 0 ||
               strcmp(mnemonic, "divw") == 0 || strcmp(mnemonic, "divb") == 0) {
      char *op = args_start ? trim(args_start) : "";
      int reg = parse_reg(op);
      if (reg < 0) {
        fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op,
                mnemonic);
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
      } else if (size_char == 'b' &&
                 (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
        rex = 0x40;
      }

      if (reg & 8)
        rex |= 0x01;

      if (size_char == 'b') {
        opcode = 0xf6;
      } else {
        opcode = 0xf7;
      }

      if (prefix)
        seg_append(&text_seg, &prefix, 1);
      if (rex)
        seg_append(&text_seg, &rex, 1);
      seg_append(&text_seg, &opcode, 1);
      seg_append(&text_seg, &modrm, 1);
      matched = 1;
    } else if (strcmp(mnemonic, "negq") == 0 || strcmp(mnemonic, "negl") == 0 ||
               strcmp(mnemonic, "negw") == 0 || strcmp(mnemonic, "negb") == 0) {
      char *op = args_start ? trim(args_start) : "";
      int reg = parse_reg(op);
      if (reg < 0) {
        fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op,
                mnemonic);
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
      } else if (size_char == 'b' &&
                 (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
        rex = 0x40;
      }

      if (reg & 8)
        rex |= 0x01;

      if (size_char == 'b') {
        opcode = 0xf6;
      } else {
        opcode = 0xf7;
      }

      if (prefix)
        seg_append(&text_seg, &prefix, 1);
      if (rex)
        seg_append(&text_seg, &rex, 1);
      seg_append(&text_seg, &opcode, 1);
      seg_append(&text_seg, &modrm, 1);
      matched = 1;
    } else if (strcmp(mnemonic, "notq") == 0 || strcmp(mnemonic, "notl") == 0 ||
               strcmp(mnemonic, "notw") == 0 || strcmp(mnemonic, "notb") == 0) {
      char *op = args_start ? trim(args_start) : "";
      int reg = parse_reg(op);
      if (reg < 0) {
        fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op,
                mnemonic);
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
      } else if (size_char == 'b' &&
                 (reg == 4 || reg == 5 || reg == 6 || reg == 7)) {
        rex = 0x40;
      }

      if (reg & 8)
        rex |= 0x01;

      if (size_char == 'b') {
        opcode = 0xf6;
      } else {
        opcode = 0xf7;
      }

      if (prefix)
        seg_append(&text_seg, &prefix, 1);
      if (rex)
        seg_append(&text_seg, &rex, 1);
      seg_append(&text_seg, &opcode, 1);
      seg_append(&text_seg, &modrm, 1);
      matched = 1;
    } else if (strcmp(mnemonic, "incq") == 0 || strcmp(mnemonic, "incl") == 0 ||
               strcmp(mnemonic, "incw") == 0 || strcmp(mnemonic, "incb") == 0 ||
               strcmp(mnemonic, "decq") == 0 || strcmp(mnemonic, "decl") == 0 ||
               strcmp(mnemonic, "decw") == 0 || strcmp(mnemonic, "decb") == 0) {
      char *op = args_start ? trim(args_start) : "";
      int reg = parse_reg(op);
      if (reg < 0) {
        fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op,
                mnemonic);
        exit(1);
      }
      int is_dec = (mnemonic[0] == 'd');
      char size_char = mnemonic[3];
      unsigned char prefix = (size_char == 'w') ? 0x66 : 0;
      unsigned char rex = 0;
      if (size_char == 'q')
        rex = 0x48;
      else if (reg & 8)
        rex = 0x40;
      else if (size_char == 'b' && (reg >= 4 && reg <= 7))
        rex = 0x40;
      if (reg & 8)
        rex |= 0x01;
      unsigned char opcode = (size_char == 'b') ? 0xfe : 0xff;
      unsigned char modrm = (unsigned char)((is_dec ? 0xc8 : 0xc0) | (reg & 7));
      if (prefix)
        seg_append(&text_seg, &prefix, 1);
      if (rex)
        seg_append(&text_seg, &rex, 1);
      seg_append(&text_seg, &opcode, 1);
      seg_append(&text_seg, &modrm, 1);
      matched = 1;
    } else if (strcmp(mnemonic, "bswapq") == 0 ||
               strcmp(mnemonic, "bswapl") == 0 ||
               strcmp(mnemonic, "bswap") == 0) {
      char *op = args_start ? trim(args_start) : "";
      int reg = parse_reg(op);
      if (reg < 0) {
        fprintf(stderr, "assembler error: invalid register '%s' in %s\n", op,
                mnemonic);
        exit(1);
      }
      int is_64 = (strcmp(mnemonic, "bswapq") == 0 ||
                   (reg >= 0 && reg < 16 && op[1] == 'r'));
      unsigned char rex = 0;
      if (is_64) {
        rex = 0x48;
      } else if (reg & 8) {
        rex = 0x40;
      }
      if (reg & 8)
        rex |= 0x01;
      unsigned char b1 = 0x0f;
      unsigned char b2 = 0xc8 | (reg & 7);
      if (rex)
        seg_append(&text_seg, &rex, 1);
      seg_append(&text_seg, &b1, 1);
      seg_append(&text_seg, &b2, 1);
      matched = 1;
    } else if (args_start &&
               try_encode_avx512_evex(&text_seg, mnemonic, args_start)) {
      matched = 1;
    }
    /* 2-operand instructions */
    else if (args_start) {
      char op1[128], op2[128];
      char *comma = NULL;
      int paren_depth = 0;
      for (char *p = args_start; *p; p++) {
        if (*p == '(')
          paren_depth++;
        else if (*p == ')')
          paren_depth--;
        else if (*p == ',' && paren_depth == 0) {
          comma = p;
          break;
        }
      }
      if (comma) {
        *comma = '\0';
        strcpy(op1, trim(args_start));
        strcpy(op2, trim(comma + 1));

        int reg1 = parse_reg(op1);
        int reg2 = parse_reg(op2);

        if (op1[0] == '%' && reg1 == -1) {
          if (strncmp(op1, "%fs:", 4) != 0 && strncmp(op1, "%gs:", 4) != 0) {
            fprintf(stderr, "assembler error: invalid register '%s' in %s\n",
                    op1, mnemonic);
            exit(1);
          }
        }
        if (op2[0] == '%' && reg2 == -1) {
          if (strncmp(op2, "%fs:", 4) != 0 && strncmp(op2, "%gs:", 4) != 0) {
            fprintf(stderr, "assembler error: invalid register '%s' in %s\n",
                    op2, mnemonic);
            exit(1);
          }
        }

        if (strcmp(mnemonic, "movq") == 0 || strcmp(mnemonic, "mov") == 0) {
          if (strncmp(op1, "%fs:", 4) == 0) {
            if (is_win64_pe) {
              /* Win64 PE TLS model:
               * %gs:0x58 points to ThreadLocalStoragePointer array in TEB.
               * movq %gs:0x58, %rax -> 65 48 8b 04 25 58 00 00 00
               * movq (%rax), %rax   -> 48 8b 00
               */
              const unsigned char gs_tls_load[] = {0x65, 0x48, 0x8B, 0x04,
                                                   0x25, 0x58, 0x00, 0x00,
                                                   0x00, 0x48, 0x8B, 0x00};
              seg_append(&text_seg, gs_tls_load, sizeof(gs_tls_load));
              if (reg2 != 0) {
                encode_movq(&text_seg, 0, reg2, 0, 0, 0);
              }
            } else {
              /* SystemV ELF TLS: movq %fs:0, reg2 */
              unsigned char fs_mov[] = {0x64, 0x48, 0x8B, 0x04, 0x25,
                                        0x00, 0x00, 0x00, 0x00};
              if (reg2 & 8)
                fs_mov[1] |= 0x04;
              fs_mov[3] = (unsigned char)(0x04 | ((reg2 & 7) << 3));
              seg_append(&text_seg, fs_mov, sizeof(fs_mov));
            }
          } else if (strncmp(op1, "%gs:", 4) == 0) {
            long long disp = strtoll(op1 + 4, NULL, 0);
            unsigned char gs_mov[] = {0x65,
                                      0x48,
                                      0x8B,
                                      0x04,
                                      0x25,
                                      (unsigned char)(disp & 0xFF),
                                      (unsigned char)((disp >> 8) & 0xFF),
                                      (unsigned char)((disp >> 16) & 0xFF),
                                      (unsigned char)((disp >> 24) & 0xFF)};
            if (reg2 & 8)
              gs_mov[1] |= 0x04;
            gs_mov[3] = (unsigned char)(0x04 | ((reg2 & 7) << 3));
            seg_append(&text_seg, gs_mov, sizeof(gs_mov));
          } else if ((reg1 >= 16 && reg1 <= 31) || (reg2 >= 16 && reg2 <= 31)) {
            int is_mem1 = 0, is_mem2 = 0;
            long long disp1 = 0, disp2 = 0;
            if (parse_mem_operand(op1, &reg1, &disp1))
              is_mem1 = 1;
            if (parse_mem_operand(op2, &reg2, &disp2))
              is_mem2 = 1;

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
            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);
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
            if (parse_mem_operand(op1, &reg1, &disp1))
              is_mem1 = 1;
            if (parse_mem_operand(op2, &reg2, &disp2))
              is_mem2 = 1;
            encode_movq(&text_seg, reg1, reg2, is_mem1, is_mem2,
                        is_mem1 ? disp1 : disp2);
          }
          matched = 1;
        } else if (strcmp(mnemonic, "movabsq") == 0 ||
                   strcmp(mnemonic, "movabs") == 0) {
          if (op1[0] == '$') {
            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);
            unsigned char rex = 0x48;
            if (reg2 & 8)
              rex |= 0x01;
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
            fprintf(stderr, "assembler error: movabsq requires an immediate "
                            "source operand\n");
            exit(1);
          }
          matched = 1;
        } else if (strcmp(mnemonic, "movd") == 0) {
          if ((reg1 >= 16 && reg1 <= 31) || (reg2 >= 16 && reg2 <= 31)) {
            int is_mem1 = 0, is_mem2 = 0;
            long long disp1 = 0, disp2 = 0;
            if (parse_mem_operand(op1, &reg1, &disp1))
              is_mem1 = 1;
            if (parse_mem_operand(op2, &reg2, &disp2))
              is_mem2 = 1;

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
        } else if (strcmp(mnemonic, "movss") == 0 ||
                   strcmp(mnemonic, "movsd") == 0) {
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
            if (parse_mem_operand(op1, &reg1, &disp1))
              is_mem1 = 1;
            if (parse_mem_operand(op2, &reg2, &disp2))
              is_mem2 = 1;

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
            if (parse_mem_operand(op1, &reg1, &disp1))
              is_mem1 = 1;
            if (parse_mem_operand(op2, &reg2, &disp2))
              is_mem2 = 1;

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
            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);
            int reg2_base = 0;
            long long disp2 = 0;
            if (parse_mem_operand(op2, &reg2_base, &disp2)) {
              encode_movl_imm_mem(&text_seg, imm, reg2_base, disp2);
            } else {
              encode_movl_imm(&text_seg, imm, reg2);
            }
          } else if (strstr(op1, "(%rip)")) {
            char lbl[128];
            char *rip = strstr(op1, "(%rip)");
            *rip = '\0';
            strcpy(lbl, trim(op1));
            if (reg2 & 8) {
              unsigned char rex = 0x44;
              seg_append(&text_seg, &rex, 1);
            }
            unsigned char op_b = 0x8b;
            unsigned char modrm = (unsigned char)(0x05 | ((reg2 & 7) << 3));
            seg_append(&text_seg, &op_b, 1);
            seg_append(&text_seg, &modrm, 1);
            Reloc r;
            r.offset = text_seg.size;
            strcpy(r.target_name, lbl);
            r.type = R_X86_64_PC32;
            r.addend = -4;
            add_reloc(r);
            unsigned char z4[4] = {0, 0, 0, 0};
            seg_append(&text_seg, z4, 4);
          } else if (strstr(op2, "(%rip)")) {
            char lbl[128];
            char *rip = strstr(op2, "(%rip)");
            *rip = '\0';
            strcpy(lbl, trim(op2));
            if (reg1 & 8) {
              unsigned char rex = 0x44;
              seg_append(&text_seg, &rex, 1);
            }
            unsigned char op_b = 0x89;
            unsigned char modrm = (unsigned char)(0x05 | ((reg1 & 7) << 3));
            seg_append(&text_seg, &op_b, 1);
            seg_append(&text_seg, &modrm, 1);
            Reloc r;
            r.offset = text_seg.size;
            strcpy(r.target_name, lbl);
            r.type = R_X86_64_PC32;
            r.addend = -4;
            add_reloc(r);
            unsigned char z4[4] = {0, 0, 0, 0};
            seg_append(&text_seg, z4, 4);
          } else {
            int is_mem1 = 0, is_mem2 = 0;
            long long disp1 = 0, disp2 = 0;
            if (parse_mem_operand(op1, &reg1, &disp1))
              is_mem1 = 1;
            if (parse_mem_operand(op2, &reg2, &disp2))
              is_mem2 = 1;
            encode_movl(&text_seg, reg1, reg2, is_mem1, is_mem2,
                        is_mem1 ? disp1 : disp2);
          }
        } else if (strcmp(mnemonic, "leaq") == 0) {
          if (strstr(op1, "(%rip)")) {
            char lbl[128];
            char *rip = strstr(op1, "(%rip)");
            *rip = '\0';
            strcpy(lbl, trim(op1));
            encode_rip_relative(&text_seg, lbl, reg2, 2);
          } else if (strstr(op1, "@tpoff")) {
            char sym[128];
            char *tpoff = strstr(op1, "@tpoff");
            size_t sym_len = (size_t)(tpoff - op1);
            if (sym_len >= sizeof(sym))
              sym_len = sizeof(sym) - 1;
            strncpy(sym, op1, sym_len);
            sym[sym_len] = '\0';

            long long extra = 0;
            char *paren = strchr(tpoff, '(');
            char *plus = strchr(tpoff, '+');
            char *minus = strchr(tpoff, '-');
            if (plus && (!paren || plus < paren)) {
              extra = strtoll(plus, NULL, 0);
            } else if (minus && (!paren || minus < paren)) {
              extra = strtoll(minus, NULL, 0);
            }

            int base_reg = 0;
            if (paren) {
              char reg_name[32];
              char *rparen = strchr(paren, ')');
              if (rparen) {
                size_t rlen = (size_t)(rparen - (paren + 1));
                if (rlen >= sizeof(reg_name))
                  rlen = sizeof(reg_name) - 1;
                strncpy(reg_name, paren + 1, rlen);
                reg_name[rlen] = '\0';
                base_reg = parse_reg(reg_name);
                if (base_reg < 0)
                  base_reg = 0;
              }
            }

            encode_leaq_disp32(&text_seg, base_reg, reg2, 0);
            Reloc r;
            r.offset = text_seg.size - 4;
            strcpy(r.target_name, trim(sym));
            r.type = R_X86_64_TPOFF32;
            r.addend = extra;
            add_reloc(r);
          } else if (strchr(op1, ',') != NULL && strchr(op1, '(') != NULL) {
            encode_cmpq_mem(&text_seg, op1, reg2, 0x8D);
          } else {
            int is_mem1 = 0;
            long long disp1 = 0;
            if (parse_mem_operand(op1, &reg1, &disp1))
              is_mem1 = 1;
            encode_leaq(&text_seg, reg1, reg2, disp1);
          }
        } else if (strcmp(mnemonic, "movb") == 0) {
          int is_mem1 = 0, is_mem2 = 0;
          long long disp1 = 0, disp2 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          if (parse_mem_operand(op2, &reg2, &disp2))
            is_mem2 = 1;
          encode_movb(&text_seg, reg1, reg2, is_mem1, is_mem2,
                      is_mem1 ? disp1 : disp2);
        } else if (strcmp(mnemonic, "movw") == 0) {
          int is_mem1 = 0, is_mem2 = 0;
          long long disp1 = 0, disp2 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          if (parse_mem_operand(op2, &reg2, &disp2))
            is_mem2 = 1;
          encode_movw(&text_seg, reg1, reg2, is_mem1, is_mem2,
                      is_mem1 ? disp1 : disp2);
        } else if (strcmp(mnemonic, "movswq") == 0) {
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          encode_movswq(&text_seg, reg1, reg2, is_mem1, disp1);
        } else if (strcmp(mnemonic, "movzbl") == 0) {
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          encode_movzbl(&text_seg, reg1, reg2, is_mem1, disp1);
        } else if (strcmp(mnemonic, "movzwl") == 0) {
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          encode_movzwl(&text_seg, reg1, reg2, is_mem1, disp1);
        } else if (strcmp(mnemonic, "movzwq") == 0) {
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          encode_movzwq(&text_seg, reg1, reg2, is_mem1, disp1);
        } else if (strcmp(mnemonic, "movsbq") == 0) {
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          encode_movsbq(&text_seg, reg1, reg2, is_mem1, disp1);
        } else if (strcmp(mnemonic, "movzbq") == 0) {
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          encode_movzbq(&text_seg, reg1, reg2, is_mem1, disp1);
        } else if (strcmp(mnemonic, "movslq") == 0) {
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          if (is_mem1) {
            unsigned char rex = 0x48;
            unsigned char opcode = 0x63;
            unsigned char modrm;
            if (disp1 == 0 && (reg1 & 7) != 5) {
              modrm = (0x00 << 6) | ((reg2 & 7) << 3) | (reg1 & 7);
              if (reg1 & 8)
                rex |= 0x01;
              if (reg2 & 8)
                rex |= 0x04;
              seg_append(&text_seg, &rex, 1);
              seg_append(&text_seg, &opcode, 1);
              seg_append(&text_seg, &modrm, 1);
              if ((reg1 & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(&text_seg, &sib, 1);
              }
            } else if (disp1 >= -128 && disp1 <= 127) {
              modrm = (0x01 << 6) | ((reg2 & 7) << 3) | (reg1 & 7);
              if (reg1 & 8)
                rex |= 0x01;
              if (reg2 & 8)
                rex |= 0x04;
              seg_append(&text_seg, &rex, 1);
              seg_append(&text_seg, &opcode, 1);
              seg_append(&text_seg, &modrm, 1);
              if ((reg1 & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(&text_seg, &sib, 1);
              }
              unsigned char d = (unsigned char)disp1;
              seg_append(&text_seg, &d, 1);
            } else {
              modrm = (0x02 << 6) | ((reg2 & 7) << 3) | (reg1 & 7);
              if (reg1 & 8)
                rex |= 0x01;
              if (reg2 & 8)
                rex |= 0x04;
              seg_append(&text_seg, &rex, 1);
              seg_append(&text_seg, &opcode, 1);
              seg_append(&text_seg, &modrm, 1);
              if ((reg1 & 7) == 4) {
                unsigned char sib = 0x24;
                seg_append(&text_seg, &sib, 1);
              }
              unsigned char d[4];
              d[0] = disp1 & 0xFF;
              d[1] = (disp1 >> 8) & 0xFF;
              d[2] = (disp1 >> 16) & 0xFF;
              d[3] = (disp1 >> 24) & 0xFF;
              seg_append(&text_seg, d, 4);
            }
          } else {
            encode_movslq(&text_seg, reg1, reg2);
          }
        } else if (strcmp(mnemonic, "addss") == 0 ||
                   strcmp(mnemonic, "addsd") == 0 ||
                   strcmp(mnemonic, "subss") == 0 ||
                   strcmp(mnemonic, "subsd") == 0 ||
                   strcmp(mnemonic, "mulss") == 0 ||
                   strcmp(mnemonic, "mulsd") == 0 ||
                   strcmp(mnemonic, "divss") == 0 ||
                   strcmp(mnemonic, "divsd") == 0 ||
                   strcmp(mnemonic, "sqrtss") == 0 ||
                   strcmp(mnemonic, "sqrtsd") == 0 ||
                   strcmp(mnemonic, "cvtsd2ss") == 0 ||
                   strcmp(mnemonic, "cvtss2sd") == 0) {
          const char *pref = (strcmp(mnemonic, "addss") == 0 ||
                              strcmp(mnemonic, "subss") == 0 ||
                              strcmp(mnemonic, "mulss") == 0 ||
                              strcmp(mnemonic, "divss") == 0 ||
                              strcmp(mnemonic, "sqrtss") == 0 ||
                              strcmp(mnemonic, "cvtss2sd") == 0)
                                 ? "\xF3"
                                 : "\xF2";
          unsigned char opcode = 0x58;
          if (strcmp(mnemonic, "addss") == 0 || strcmp(mnemonic, "addsd") == 0)
            opcode = 0x58;
          else if (strcmp(mnemonic, "subss") == 0 ||
                   strcmp(mnemonic, "subsd") == 0)
            opcode = 0x5C;
          else if (strcmp(mnemonic, "mulss") == 0 ||
                   strcmp(mnemonic, "mulsd") == 0)
            opcode = 0x59;
          else if (strcmp(mnemonic, "divss") == 0 ||
                   strcmp(mnemonic, "divsd") == 0)
            opcode = 0x5E;
          else if (strcmp(mnemonic, "sqrtss") == 0 ||
                   strcmp(mnemonic, "sqrtsd") == 0)
            opcode = 0x51;
          else if (strcmp(mnemonic, "cvtsd2ss") == 0 ||
                   strcmp(mnemonic, "cvtss2sd") == 0)
            opcode = 0x5A;

          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;

          if (is_mem1) {
            encode_sse_mem(&text_seg, pref, 1, opcode, reg1, reg2, 1, disp1);
          } else {
            encode_sse_binop(&text_seg, pref, 1, opcode, reg1, reg2);
          }
        } else if (strcmp(mnemonic, "ucomiss") == 0 ||
                   strcmp(mnemonic, "ucomisd") == 0) {
          const char *pref = (strcmp(mnemonic, "ucomiss") == 0) ? "" : "\x66";
          int n_pref = (strcmp(mnemonic, "ucomiss") == 0) ? 0 : 1;
          unsigned char opcode = 0x2E;
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;

          if (is_mem1) {
            encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1,
                           disp1);
          } else {
            encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
          }
        } else if (strcmp(mnemonic, "pxor") == 0) {
          const char *pref = "\x66";
          int n_pref = 1;
          unsigned char opcode = 0xEF;
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;

          if (is_mem1) {
            encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1,
                           disp1);
          } else {
            encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
          }
        } else if (strcmp(mnemonic, "pcmpeqd") == 0) {
          const char *pref = "\x66";
          int n_pref = 1;
          unsigned char opcode = 0x76;
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;

          if (is_mem1) {
            encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1,
                           disp1);
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
            fprintf(stderr,
                    "assembler error: psrld only supports immediate shift\n");
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
            fprintf(stderr,
                    "assembler error: psrlq only supports immediate shift\n");
            exit(1);
          }
        } else if (strcmp(mnemonic, "andps") == 0) {
          const char *pref = "";
          int n_pref = 0;
          unsigned char opcode = 0x54;
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          if (is_mem1) {
            encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1,
                           disp1);
          } else {
            encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
          }
        } else if (strcmp(mnemonic, "andpd") == 0) {
          const char *pref = "\x66";
          int n_pref = 1;
          unsigned char opcode = 0x54;
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;
          if (is_mem1) {
            encode_sse_mem(&text_seg, pref, n_pref, opcode, reg1, reg2, 1,
                           disp1);
          } else {
            encode_sse_binop(&text_seg, pref, n_pref, opcode, reg1, reg2);
          }
        } else if (strcmp(mnemonic, "cvttss2si") == 0 ||
                   strcmp(mnemonic, "cvttss2siq") == 0 ||
                   strcmp(mnemonic, "cvttss2sil") == 0 ||
                   strcmp(mnemonic, "cvttsd2si") == 0 ||
                   strcmp(mnemonic, "cvttsd2siq") == 0 ||
                   strcmp(mnemonic, "cvttsd2sil") == 0) {
          const char *pref =
              (strncmp(mnemonic, "cvttss", 6) == 0) ? "\xF3" : "\xF2";
          int use_rex_w = (strstr(mnemonic, "siq") != NULL) || is_reg_64(op2);
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;

          if (is_mem1) {
            encode_cvtt_to_si_mem(&text_seg, pref, 1, 0x2C, reg1, reg2,
                                  use_rex_w, disp1);
          } else {
            encode_cvtt_to_si(&text_seg, pref, 1, 0x2C, reg1, reg2, use_rex_w);
          }
        } else if (strcmp(mnemonic, "cvtsi2ss") == 0 ||
                   strcmp(mnemonic, "cvtsi2ssq") == 0 ||
                   strcmp(mnemonic, "cvtsi2ssl") == 0 ||
                   strcmp(mnemonic, "cvtsi2sd") == 0 ||
                   strcmp(mnemonic, "cvtsi2sdq") == 0 ||
                   strcmp(mnemonic, "cvtsi2sdl") == 0) {
          const char *pref =
              (strstr(mnemonic, "2ss") != NULL) ? "\xF3" : "\xF2";
          int use_rex_w = (strstr(mnemonic, "q") != NULL) || is_reg_64(op1);
          int is_mem1 = 0;
          long long disp1 = 0;
          if (parse_mem_operand(op1, &reg1, &disp1))
            is_mem1 = 1;

          if (is_mem1) {
            encode_cvtsi_to_xmm_mem(&text_seg, pref, 1, 0x2A, reg1, reg2,
                                    use_rex_w, disp1);
          } else {
            encode_cvtsi_to_xmm(&text_seg, pref, 1, 0x2A, reg1, reg2,
                                use_rex_w);
          }
        } else if (strcmp(mnemonic, "addq") == 0 ||
                   strcmp(mnemonic, "subq") == 0 ||
                   strcmp(mnemonic, "cmpq") == 0 ||
                   strcmp(mnemonic, "imulq") == 0 ||
                   strcmp(mnemonic, "andq") == 0 ||
                   strcmp(mnemonic, "orq") == 0 ||
                   strcmp(mnemonic, "xorq") == 0) {
          if (op1[0] == '$') {
            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);
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
            } else if (strcmp(op2, "(%rsp)") == 0 && imm == 0 &&
                       strcmp(mnemonic, "orq") == 0) {
              unsigned char b[4] = {
                  0x48, 0x85, 0x04,
                  0x24}; /* testq %rax, (%rsp) - 4-byte stack page probe */
              seg_append(&text_seg, b, 4);
            } else if (strcmp(op2, "(%rsp)") == 0 && imm >= -128 &&
                       imm <= 127) {
              unsigned char subcode = 0x00;
              if (strcmp(mnemonic, "orq") == 0)
                subcode = 0x08;
              else if (strcmp(mnemonic, "andq") == 0)
                subcode = 0x20;
              else if (strcmp(mnemonic, "subq") == 0)
                subcode = 0x28;
              else if (strcmp(mnemonic, "xorq") == 0)
                subcode = 0x30;
              else if (strcmp(mnemonic, "cmpq") == 0)
                subcode = 0x38;
              unsigned char b[5] = {0x48, 0x83, (unsigned char)(0x04 | subcode),
                                    0x24, (unsigned char)imm};
              seg_append(&text_seg, b, 5);
            } else {
              unsigned char rex = 0x48;
              unsigned char opcode = 0x81;
              unsigned char modrm = 0xc0;
              if (strcmp(mnemonic, "addq") == 0)
                modrm = 0xc0;
              else if (strcmp(mnemonic, "orq") == 0)
                modrm = 0xc8;
              else if (strcmp(mnemonic, "andq") == 0)
                modrm = 0xe0;
              else if (strcmp(mnemonic, "subq") == 0)
                modrm = 0xe8;
              else if (strcmp(mnemonic, "xorq") == 0)
                modrm = 0xf0;
              else if (strcmp(mnemonic, "cmpq") == 0)
                modrm = 0xf8;
              modrm |= (reg2 & 7);
              if (reg2 & 8)
                rex |= 0x01;

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
            unsigned char mop = (strcmp(mnemonic, "addq") == 0) ? 0x00 : (strcmp(mnemonic, "orq") == 0) ? 0x08 : (strcmp(mnemonic, "andq") == 0) ? 0x20 : (strcmp(mnemonic, "subq") == 0) ? 0x28 : (strcmp(mnemonic, "xorq") == 0) ? 0x30 : (strcmp(mnemonic, "cmpq") == 0) ? 0x38 : 0xff;
            if (mop != 0xff && strchr(op1, '(')) {
              encode_cmpq_mem(&text_seg, op1, reg2, mop | 0x03);
            } else if (mop != 0xff && strchr(op2, '(')) {
              encode_cmpq_mem(&text_seg, op2, reg1, mop | 0x01);
            } else {
              encode_binop(&text_seg, mnemonic, reg1, reg2);
            }
          }
        } else if (strcmp(mnemonic, "sarq") == 0 ||
                   strcmp(mnemonic, "shlq") == 0 ||
                   strcmp(mnemonic, "shrq") == 0 ||
                   strcmp(mnemonic, "rolq") == 0 ||
                   strcmp(mnemonic, "rorq") == 0 ||
                   strcmp(mnemonic, "sarl") == 0 ||
                   strcmp(mnemonic, "shll") == 0 ||
                   strcmp(mnemonic, "shrl") == 0 ||
                   strcmp(mnemonic, "roll") == 0 ||
                   strcmp(mnemonic, "rorl") == 0 ||
                   strcmp(mnemonic, "sarw") == 0 ||
                   strcmp(mnemonic, "shlw") == 0 ||
                   strcmp(mnemonic, "shrw") == 0 ||
                   strcmp(mnemonic, "sarb") == 0 ||
                   strcmp(mnemonic, "shlb") == 0 ||
                   strcmp(mnemonic, "shrb") == 0) {
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
            } else if (size_char == 'b' &&
                       (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7)) {
              rex = 0x40;
            }

            if (reg2 & 8)
              rex |= 0x01;

            if (size_char == 'b') {
              opcode = 0xc0;
            } else {
              opcode = 0xc1;
            }

            if (strncmp(mnemonic, "sar", 3) == 0) {
              modrm = 0xf8 | (reg2 & 7);
            } else if (strncmp(mnemonic, "shr", 3) == 0) {
              modrm = 0xe8 | (reg2 & 7);
            } else if (strncmp(mnemonic, "rol", 3) == 0) {
              modrm = 0xc0 | (reg2 & 7);
            } else if (strncmp(mnemonic, "ror", 3) == 0) {
              modrm = 0xc8 | (reg2 & 7);
            } else {
              modrm = 0xe0 | (reg2 & 7);
            }

            if (prefix)
              seg_append(&text_seg, &prefix, 1);
            if (rex)
              seg_append(&text_seg, &rex, 1);
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
            } else if (size_char == 'b' &&
                       (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7)) {
              rex = 0x40;
            }

            if (reg2 & 8)
              rex |= 0x01;

            if (size_char == 'b') {
              opcode = 0xd2;
            } else {
              opcode = 0xd3;
            }

            if (strncmp(mnemonic, "sar", 3) == 0) {
              modrm = 0xf8 | (reg2 & 7);
            } else if (strncmp(mnemonic, "shr", 3) == 0) {
              modrm = 0xe8 | (reg2 & 7);
            } else if (strncmp(mnemonic, "rol", 3) == 0) {
              modrm = 0xc0 | (reg2 & 7);
            } else if (strncmp(mnemonic, "ror", 3) == 0) {
              modrm = 0xc8 | (reg2 & 7);
            } else {
              modrm = 0xe0 | (reg2 & 7);
            }

            if (prefix)
              seg_append(&text_seg, &prefix, 1);
            if (rex)
              seg_append(&text_seg, &rex, 1);
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
          }
        } else if (strcmp(mnemonic, "addl") == 0 ||
                   strcmp(mnemonic, "subl") == 0 ||
                   strcmp(mnemonic, "cmpl") == 0) {
          if (op1[0] == '$') {
            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);
            unsigned char rex = 0x40;
            unsigned char opcode = 0x81;
            unsigned char modrm = 0xc0;
            if (strcmp(mnemonic, "addl") == 0)
              modrm = 0xc0 | (reg2 & 7);
            else if (strcmp(mnemonic, "subl") == 0)
              modrm = 0xe8 | (reg2 & 7);
            else if (strcmp(mnemonic, "cmpl") == 0)
              modrm = 0xf8 | (reg2 & 7);
            if (reg2 & 8)
              rex |= 0x01;

            if (imm >= -128 && imm <= 127) {
              opcode = 0x83;
              if (rex != 0x40)
                seg_append(&text_seg, &rex, 1);
              seg_append(&text_seg, &opcode, 1);
              seg_append(&text_seg, &modrm, 1);
              unsigned char b = (unsigned char)imm;
              seg_append(&text_seg, &b, 1);
            } else {
              if (rex != 0x40)
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
            unsigned char rex = 0x40;
            unsigned char opcode = 0x01;
            if (strcmp(mnemonic, "addl") == 0)
              opcode = 0x01;
            else if (strcmp(mnemonic, "subl") == 0)
              opcode = 0x29;
            else if (strcmp(mnemonic, "cmpl") == 0)
              opcode = 0x39;
            unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
            if (reg1 & 8)
              rex |= 0x04;
            if (reg2 & 8)
              rex |= 0x01;
            if (rex != 0x40)
              seg_append(&text_seg, &rex, 1);
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
          }
        } else if (strcmp(mnemonic, "xorl") == 0 ||
                   strcmp(mnemonic, "andl") == 0 ||
                   strcmp(mnemonic, "orl") == 0) {
          if (op1[0] == '$') {
            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);
            unsigned char rex = 0x40;
            unsigned char opcode = 0x81;
            unsigned char modrm = 0xc0;
            if (strcmp(mnemonic, "orl") == 0)
              modrm = 0xc8;
            else if (strcmp(mnemonic, "andl") == 0)
              modrm = 0xe0;
            else if (strcmp(mnemonic, "xorl") == 0)
              modrm = 0xf0;
            modrm |= (reg2 & 7);
            if (reg2 & 8)
              rex |= 0x01;

            if (imm >= -128 && imm <= 127) {
              opcode = 0x83;
              if (rex != 0x40)
                seg_append(&text_seg, &rex, 1);
              seg_append(&text_seg, &opcode, 1);
              seg_append(&text_seg, &modrm, 1);
              unsigned char b = (unsigned char)imm;
              seg_append(&text_seg, &b, 1);
            } else {
              if (rex != 0x40)
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
            unsigned char rex = 0x40;
            unsigned char opcode = 0x31;
            if (strcmp(mnemonic, "orl") == 0)
              opcode = 0x09;
            else if (strcmp(mnemonic, "andl") == 0)
              opcode = 0x21;
            else if (strcmp(mnemonic, "xorl") == 0)
              opcode = 0x31;
            unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
            if (reg1 & 8)
              rex |= 0x04;
            if (reg2 & 8)
              rex |= 0x01;
            if (rex != 0x40)
              seg_append(&text_seg, &rex, 1);
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
          }
        } else if (strcmp(mnemonic, "xorb") == 0 ||
                   strcmp(mnemonic, "andb") == 0 ||
                   strcmp(mnemonic, "orb") == 0 ||
                   strcmp(mnemonic, "addb") == 0 ||
                   strcmp(mnemonic, "subb") == 0 ||
                   strcmp(mnemonic, "cmpb") == 0) {
          if (op1[0] == '$') {
            long long imm = (op1[1] == '-') ? strtoll(op1 + 1, NULL, 0) : (long long)strtoull(op1 + 1, NULL, 0);
            unsigned char rex = 0x40;
            unsigned char opcode = 0x80;
            unsigned char modrm = 0xc0;
            if (strcmp(mnemonic, "addb") == 0)
              modrm = 0xc0;
            else if (strcmp(mnemonic, "orb") == 0)
              modrm = 0xc8;
            else if (strcmp(mnemonic, "subb") == 0)
              modrm = 0xe8;
            else if (strcmp(mnemonic, "andb") == 0)
              modrm = 0xe0;
            else if (strcmp(mnemonic, "xorb") == 0)
              modrm = 0xf0;
            else if (strcmp(mnemonic, "cmpb") == 0)
              modrm = 0xf8;
            modrm |= (reg2 & 7);
            if (reg2 & 8)
              rex |= 0x01;
            if (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7)
              rex |= 0x40;

            if (rex != 0x40 ||
                (reg2 == 4 || reg2 == 5 || reg2 == 6 || reg2 == 7)) {
              seg_append(&text_seg, &rex, 1);
            }
            seg_append(&text_seg, &opcode, 1);
            seg_append(&text_seg, &modrm, 1);
            unsigned char b = (unsigned char)imm;
            seg_append(&text_seg, &b, 1);
          } else {
            unsigned char rex = 0x40;
            unsigned char opcode = 0x30;
            if (strcmp(mnemonic, "addb") == 0)
              opcode = 0x00;
            else if (strcmp(mnemonic, "orb") == 0)
              opcode = 0x08;
            else if (strcmp(mnemonic, "subb") == 0)
              opcode = 0x28;
            else if (strcmp(mnemonic, "andb") == 0)
              opcode = 0x20;
            else if (strcmp(mnemonic, "xorb") == 0)
              opcode = 0x30;
            else if (strcmp(mnemonic, "cmpb") == 0)
              opcode = 0x38;
            unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
            if (reg1 & 8)
              rex |= 0x04;
            if (reg2 & 8)
              rex |= 0x01;
            if (reg1 == 4 || reg1 == 5 || reg1 == 6 || reg1 == 7 || reg2 == 4 ||
                reg2 == 5 || reg2 == 6 || reg2 == 7) {
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
          if (reg1 & 8)
            rex |= 0x04;
          if (reg2 & 8)
            rex |= 0x01;
          unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
          seg_append(&text_seg, &rex, 1);
          seg_append(&text_seg, &opcode, 1);
          seg_append(&text_seg, &modrm, 1);
        } else if (strcmp(mnemonic, "testl") == 0) {
          /* testl %eSrc, %eDst — [REX] 0x85 ModRM(11,src,dst) */
          unsigned char rex = 0x40;
          unsigned char opcode = 0x85;
          if (reg1 & 8)
            rex |= 0x04;
          if (reg2 & 8)
            rex |= 0x01;
          unsigned char modrm = 0xc0 | ((reg1 & 7) << 3) | (reg2 & 7);
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
          seg_append(&text_seg, &opcode, 1);
          seg_append(&text_seg, &modrm, 1);
        } else if (strcmp(mnemonic, "bsfl") == 0 ||
                   strcmp(mnemonic, "bsfq") == 0 ||
                   strcmp(mnemonic, "bsrl") == 0 ||
                   strcmp(mnemonic, "bsrq") == 0 ||
                   strcmp(mnemonic, "popcntl") == 0 ||
                   strcmp(mnemonic, "popcntq") == 0) {
          /* AT&T: op %src(rm=reg1), %dst(reg=reg2) -> [0xF3] [REX] 0x0F op2 ModRM(11,reg2,reg1) */
          int is_64 = (mnemonic[strlen(mnemonic) - 1] == 'q');
          int is_popcnt = (strncmp(mnemonic, "popcnt", 6) == 0);
          unsigned char opcode2 = is_popcnt ? 0xB8 : ((strncmp(mnemonic, "bsf", 3) == 0) ? 0xBC : 0xBD);
          unsigned char rex = is_64 ? 0x48 : 0x40;
          if (reg2 & 8)
            rex |= 0x04;
          if (reg1 & 8)
            rex |= 0x01;
          if (is_popcnt) {
            unsigned char pref = 0xF3;
            seg_append(&text_seg, &pref, 1);
          }
          if (rex != 0x40)
            seg_append(&text_seg, &rex, 1);
          unsigned char esc = 0x0F;
          unsigned char modrm = 0xc0 | ((reg2 & 7) << 3) | (reg1 & 7);
          seg_append(&text_seg, &esc, 1);
          seg_append(&text_seg, &opcode2, 1);
          seg_append(&text_seg, &modrm, 1);
        } else {
          fprintf(stderr,
                  "assembler error: unrecognized 2-operand instruction '%s'\n",
                  mnemonic);
          exit(1);
        }
        matched = 1;
      }
    }

    if (!matched) {
      fprintf(stderr,
              "assembler error: unrecognized instruction '%s' with args '%s'\n",
              mnemonic, args_start ? args_start : "");
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
  printf("codegen: PASS 1 DONE, text_seg.size=%d, data_seg.size=%d, labels=%d, "
         "relocs=%d\n",
         (int)text_seg.size, (int)data_seg.size, (int)label_count,
         (int)reloc_count);

  /* Pass 1.5: Register external symbols that were called but not defined */
  {
    size_t i;
    for (i = 0; i < reloc_count; i++) {
      if (find_label_idx(relocs[i].target_name) < 0) {
        /* Add external target as an undefined global symbol */
        size_t idx;
        if (label_count >= 131072) {
          fprintf(stderr,
                  "error: assembler label table limit exceeded (max 131072)\n");
          free(text_seg.data);
          free(data_seg.data);
          if (tls_seg.data)
            free(tls_seg.data);
          return -1;
        }
        idx = label_count++;
        strncpy(labels[idx].name, relocs[i].target_name,
                sizeof(labels[idx].name) - 1);
        labels[idx].name[sizeof(labels[idx].name) - 1] = '\0';
        labels[idx].is_global = 1;
        labels[idx].segment = 0; /* SHN_UNDEF */
        labels[idx].size = 0;
        labels[idx].offset = 0;
        register_label_hash(idx);
      }
    }
  }

  /* For Win64 PE: Synthesize import stubs for unresolved external symbols */
  const char *win_import_funcs[512];
  size_t win_import_count = 0;
  size_t win_stub_offsets[512];
  size_t win_bss_size = 0;
  int is_win64_exe = (is_win64_pe && !g_win64_is_shared);
  int win_stdio_stdin_idx = -1;
  int win_stdio_stdout_idx = -1;
  int win_stdio_stderr_idx = -1;
  int win_iob_func_idx = -1;

  if (is_win64_pe) {
    size_t k;
    /* 1. Allocate .comm symbols (segment == 3): virtual BSS for .exe (v9), or data_seg for DLL (v8) */
    for (k = 0; k < label_count; k++) {
      if (labels[k].segment == 3) {
        size_t align = (labels[k].offset > 0 && labels[k].offset <= 64)
                           ? (size_t)labels[k].offset
                           : 8;
        size_t sz = (labels[k].size > 0) ? (size_t)labels[k].size : 8;
        if (is_win64_exe) {
          win_bss_size = (win_bss_size + align - 1) & ~(align - 1);
          labels[k].offset = win_bss_size;
          win_bss_size += sz;
        } else {
          size_t pad = (align - (data_seg.size % align)) % align;
          if (pad > 0) {
            unsigned char pad_z[64] = {0};
            seg_append(&data_seg, pad_z, pad);
          }
          labels[k].offset = data_seg.size;
          labels[k].segment = 2;
          unsigned char *zeros = (unsigned char *)calloc(1, sz);
          if (zeros) {
            seg_append(&data_seg, zeros, sz);
            free(zeros);
          }
        }
      }
    }

    /* 2. Handle inline SSE math builtins (fmaxf, fminf, fmax, fmin, fabsf) &
     * stdio globals */
    int needs_iob_func = 0;
    for (k = 0; k < label_count; k++) {
      if (labels[k].segment == 0 && labels[k].is_global) {
        const char *sname = labels[k].name;
        if (strcmp(sname, "stdin") == 0 || strcmp(sname, "stdout") == 0 ||
            strcmp(sname, "stderr") == 0) {
          size_t pad = (8 - (data_seg.size % 8)) % 8;
          if (pad > 0) {
            unsigned char pad_z[8] = {0};
            seg_append(&data_seg, pad_z, pad);
          }
          labels[k].offset = data_seg.size;
          labels[k].segment = 2;
          labels[k].size = 8;
          unsigned char z8[8] = {0};
          seg_append(&data_seg, z8, 8);
          if (strcmp(sname, "stdin") == 0)
            win_stdio_stdin_idx = (int)k;
          else if (strcmp(sname, "stdout") == 0)
            win_stdio_stdout_idx = (int)k;
          else if (strcmp(sname, "stderr") == 0)
            win_stdio_stderr_idx = (int)k;
          needs_iob_func = 1;
        } else if (strcmp(sname, "fmaxf") == 0 || strcmp(sname, "fminf") == 0 ||
                   strcmp(sname, "fmax") == 0 || strcmp(sname, "fmin") == 0 ||
                   strcmp(sname, "fabsf") == 0) {
          size_t pad = (16 - (text_seg.size % 16)) % 16;
          if (pad > 0) {
            unsigned char pad_bytes[16] = {0};
            seg_append(&text_seg, pad_bytes, pad);
          }
          labels[k].segment = 1;
          labels[k].offset = text_seg.size;
          labels[k].size = 16;
          if (strcmp(sname, "fmaxf") == 0) {
            unsigned char b[16] = {0xF3, 0x0F, 0x5F, 0xC1, 0x66, 0x0F,
                                   0x7E, 0xC0, 0xC3, 0x90, 0x90, 0x90,
                                   0x90, 0x90, 0x90, 0x90};
            seg_append(&text_seg, b, 16);
          } else if (strcmp(sname, "fminf") == 0) {
            unsigned char b[16] = {0xF3, 0x0F, 0x5D, 0xC1, 0x66, 0x0F,
                                   0x7E, 0xC0, 0xC3, 0x90, 0x90, 0x90,
                                   0x90, 0x90, 0x90, 0x90};
            seg_append(&text_seg, b, 16);
          } else if (strcmp(sname, "fmax") == 0) {
            unsigned char b[16] = {0xF2, 0x0F, 0x5F, 0xC1, 0x66, 0x48,
                                   0x0F, 0x7E, 0xC0, 0xC3, 0x90, 0x90,
                                   0x90, 0x90, 0x90, 0x90};
            seg_append(&text_seg, b, 16);
          } else if (strcmp(sname, "fmin") == 0) {
            unsigned char b[16] = {0xF2, 0x0F, 0x5D, 0xC1, 0x66, 0x48,
                                   0x0F, 0x7E, 0xC0, 0xC3, 0x90, 0x90,
                                   0x90, 0x90, 0x90, 0x90};
            seg_append(&text_seg, b, 16);
          } else {
            unsigned char b[16] = {0x66, 0x0F, 0x7E, 0xC0, 0x25, 0xFF,
                                   0xFF, 0xFF, 0x7F, 0x66, 0x0F, 0x6E,
                                   0xC0, 0xC3, 0x90, 0x90};
            seg_append(&text_seg, b, 16);
          }
        }
      }
    }
    if (needs_iob_func) {
      int found_iob = find_label_idx("__iob_func");
      if (found_iob >= 0) {
        win_iob_func_idx = found_iob;
      } else if (label_count < 131072) {
        size_t idx = label_count++;
        strcpy(labels[idx].name, "__iob_func");
        labels[idx].is_global = 1;
        labels[idx].segment = 0;
        labels[idx].size = 0;
        labels[idx].offset = 0;
        register_label_hash(idx);
        win_iob_func_idx = (int)idx;
      }
    }

    for (k = 0; k < label_count; k++) {
      if (labels[k].segment == 0 && labels[k].is_global) {
        if (win_import_count >= 512) {
          fprintf(stderr, "error: win64 PE import limit (512) exceeded at '%s'\n",
                  labels[k].name);
          free(text_seg.data);
          free(data_seg.data);
          if (tls_seg.data) free(tls_seg.data);
          return -1;
        }
        {
          /* Pad text segment to 16 bytes */
          size_t pad = (16 - (text_seg.size % 16)) % 16;
          if (pad > 0) {
            unsigned char pad_bytes[16] = {0};
            seg_append(&text_seg, pad_bytes, pad);
          }
          labels[k].segment = 1;
          labels[k].offset = text_seg.size;

          const char *iat_name =
              win64_remap_msvcrt_sym(labels[k].name, g_win64_ms_abi);
          if (g_win64_ms_abi) {
            /* MS-ABI caller already places args in rcx/rdx/r8/r9 + xmm0..3,
             * 32-byte shadow space, and stack args 5..N at 0x20+(%rsp).
             * Tail-jump through IAT so return address and all stack args
             * stay at exact MS-ABI offsets without shifting %rsp.
             * For v*printf, unwrap ZCC's __va_list_struct* (ap+8 = overflow ptr). */
            labels[k].size = 32;
            win_stub_offsets[win_import_count] = text_seg.size;
            win_import_funcs[win_import_count] = iat_name;
            unsigned char ms_stub_tmpl[32] = {
                0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
                0x90, 0x90, 0x90, 0x90, 0x90, /* align +21 */
                0xFF, 0x25, 0x00, 0x00, 0x00,
                0x00,                   /* jmp *disp32(%rip) at offset 21 */
                0x90, 0x90, 0x90, 0x90, 0x90
            };
            if (strcmp(labels[k].name, "vprintf") == 0) {
              ms_stub_tmpl[0] = 0x48;
              ms_stub_tmpl[1] = 0x8B;
              ms_stub_tmpl[2] = 0x52;
              ms_stub_tmpl[3] = 0x08; /* movq 8(%rdx), %rdx */
            } else if (strcmp(labels[k].name, "vfprintf") == 0 ||
                       strcmp(labels[k].name, "vsprintf") == 0) {
              ms_stub_tmpl[0] = 0x4D;
              ms_stub_tmpl[1] = 0x8B;
              ms_stub_tmpl[2] = 0x40;
              ms_stub_tmpl[3] = 0x08; /* movq 8(%r8), %r8 */
            } else if (strcmp(labels[k].name, "vsnprintf") == 0 ||
                       strcmp(labels[k].name, "_vsnprintf") == 0) {
              ms_stub_tmpl[0] = 0x4D;
              ms_stub_tmpl[1] = 0x8B;
              ms_stub_tmpl[2] = 0x49;
              ms_stub_tmpl[3] = 0x08; /* movq 8(%r9), %r9 */
            }
            seg_append(&text_seg, ms_stub_tmpl, 32);
          } else {
            /* Emit SysV float-vararg bridge pre-stub for printf / fprintf /
             * sprintf / snprintf / v*printf */
            if (strcmp(labels[k].name, "printf") == 0) {
              static const unsigned char pre_printf[234] = {
                  0x48, 0x83, 0xEC, 0x78, 0x48, 0x89, 0x74, 0x24, 0x38, 0x48,
                  0x89, 0x54, 0x24, 0x40, 0x48, 0x89, 0x4C, 0x24, 0x48, 0x4C,
                  0x89, 0x44, 0x24, 0x50, 0x4C, 0x89, 0x4C, 0x24, 0x58, 0x0F,
                  0x13, 0x44, 0x24, 0x60, 0x0F, 0x13, 0x4C, 0x24, 0x68, 0x0F,
                  0x13, 0x54, 0x24, 0x70, 0x48, 0x89, 0x74, 0x24, 0x08, 0x48,
                  0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x18, 0x4C,
                  0x89, 0x44, 0x24, 0x20, 0x4C, 0x89, 0x4C, 0x24, 0x28, 0x49,
                  0x89, 0xF9, 0x4C, 0x8D, 0x54, 0x24, 0x38, 0x4C, 0x8D, 0x5C,
                  0x24, 0x60, 0x48, 0x8D, 0x4C, 0x24, 0x08, 0xBA, 0x06, 0x00,
                  0x00, 0x00, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x74, 0x72,
                  0x49, 0xFF, 0xC1, 0x3C, 0x25, 0x75, 0xF1, 0x41, 0x80, 0x39,
                  0x25, 0x75, 0x05, 0x49, 0xFF, 0xC1, 0xEB, 0xE6, 0x41, 0x0F,
                  0xB6, 0x01, 0x84, 0xC0, 0x74, 0x58, 0x49, 0xFF, 0xC1, 0x3C,
                  0x2A, 0x74, 0x2B, 0x3C, 0x41, 0x72, 0xED, 0x0C, 0x20, 0x3C,
                  0x6C, 0x74, 0xE7, 0x3C, 0x68, 0x74, 0xE3, 0x3C, 0x7A, 0x74,
                  0xDF, 0x3C, 0x66, 0x74, 0x29, 0x3C, 0x65, 0x74, 0x25, 0x3C,
                  0x67, 0x74, 0x21, 0x3C, 0x61, 0x74, 0x1D, 0x49, 0x8B, 0x02,
                  0x49, 0x83, 0xC2, 0x08, 0xEB, 0x1B, 0x49, 0x8B, 0x02, 0x49,
                  0x83, 0xC2, 0x08, 0x48, 0x89, 0x01, 0x48, 0x83, 0xC1, 0x08,
                  0xFF, 0xCA, 0x75, 0xB4, 0xEB, 0x12, 0x49, 0x8B, 0x03, 0x49,
                  0x83, 0xC3, 0x08, 0x48, 0x89, 0x01, 0x48, 0x83, 0xC1, 0x08,
                  0xFF, 0xCA, 0x75, 0x86, 0x48, 0x8B, 0x74, 0x24, 0x08, 0x48,
                  0x8B, 0x54, 0x24, 0x10, 0x48, 0x8B, 0x4C, 0x24, 0x18, 0x4C,
                  0x8B, 0x44, 0x24, 0x20};
              seg_append(&text_seg, pre_printf, 234);
              labels[k].size = 266;
            } else if (strcmp(labels[k].name, "fprintf") == 0 ||
                       strcmp(labels[k].name, "sprintf") == 0) {
              static const unsigned char pre_fprintf[198] = {
                  0x85, 0xC0, 0x0F, 0x84, 0xBE, 0x00, 0x00, 0x00, 0x48, 0x83,
                  0xEC, 0x50, 0x48, 0x89, 0x14, 0x24, 0x48, 0x89, 0x4C, 0x24,
                  0x08, 0x4C, 0x89, 0x44, 0x24, 0x10, 0x66, 0x0F, 0xD6, 0x44,
                  0x24, 0x18, 0x66, 0x0F, 0xD6, 0x4C, 0x24, 0x20, 0x66, 0x0F,
                  0xD6, 0x54, 0x24, 0x28, 0x48, 0x89, 0x54, 0x24, 0x30, 0x48,
                  0x89, 0x4C, 0x24, 0x38, 0x4C, 0x89, 0x44, 0x24, 0x40, 0x49,
                  0x89, 0xF1, 0x4C, 0x8D, 0x14, 0x24, 0x4C, 0x8D, 0x5C, 0x24,
                  0x18, 0x48, 0x8D, 0x4C, 0x24, 0x30, 0xBA, 0x03, 0x00, 0x00,
                  0x00, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x74, 0x5A, 0x49,
                  0xFF, 0xC1, 0x3C, 0x25, 0x75, 0xF1, 0x41, 0x80, 0x39, 0x25,
                  0x75, 0x05, 0x49, 0xFF, 0xC1, 0xEB, 0xE6, 0x41, 0x0F, 0xB6,
                  0x01, 0x84, 0xC0, 0x74, 0x40, 0x49, 0xFF, 0xC1, 0x3C, 0x41,
                  0x72, 0xF1, 0x0C, 0x20, 0x3C, 0x6C, 0x74, 0xEB, 0x3C, 0x68,
                  0x74, 0xE7, 0x3C, 0x7A, 0x74, 0xE3, 0x3C, 0x66, 0x74, 0x15,
                  0x3C, 0x65, 0x74, 0x11, 0x3C, 0x67, 0x74, 0x0D, 0x3C, 0x61,
                  0x74, 0x09, 0x49, 0x8B, 0x02, 0x49, 0x83, 0xC2, 0x08, 0xEB,
                  0x07, 0x49, 0x8B, 0x03, 0x49, 0x83, 0xC3, 0x08, 0x48, 0x89,
                  0x01, 0x48, 0x83, 0xC1, 0x08, 0xFF, 0xCA, 0x75, 0x9E, 0x48,
                  0x8B, 0x54, 0x24, 0x30, 0x48, 0x8B, 0x4C, 0x24, 0x38, 0x4C,
                  0x8B, 0x44, 0x24, 0x40, 0x48, 0x83, 0xC4, 0x50};
              seg_append(&text_seg, pre_fprintf, 198);
              labels[k].size = 230;
            } else if (strcmp(labels[k].name, "snprintf") == 0 ||
                       strcmp(labels[k].name, "_snprintf") == 0) {
              static const unsigned char pre_snprintf[265] = {
                  0x48, 0x81, 0xEC, 0xA8, 0x00, 0x00, 0x00, 0x48, 0x89, 0x4C,
                  0x24, 0x40, 0x4C, 0x89, 0x44, 0x24, 0x48, 0x4C, 0x89, 0x4C,
                  0x24, 0x50, 0x48, 0x8B, 0x84, 0x24, 0xB0, 0x00, 0x00, 0x00,
                  0x48, 0x89, 0x44, 0x24, 0x58, 0x48, 0x8B, 0x84, 0x24, 0xB8,
                  0x00, 0x00, 0x00, 0x48, 0x89, 0x44, 0x24, 0x60, 0x48, 0x8B,
                  0x84, 0x24, 0xC0, 0x00, 0x00, 0x00, 0x48, 0x89, 0x44, 0x24,
                  0x68, 0x0F, 0x13, 0x44, 0x24, 0x70, 0x0F, 0x13, 0x4C, 0x24,
                  0x78, 0x0F, 0x13, 0x94, 0x24, 0x80, 0x00, 0x00, 0x00, 0x0F,
                  0x13, 0x9C, 0x24, 0x88, 0x00, 0x00, 0x00, 0x0F, 0x13, 0xA4,
                  0x24, 0x90, 0x00, 0x00, 0x00, 0x0F, 0x13, 0xAC, 0x24, 0x98,
                  0x00, 0x00, 0x00, 0x49, 0x89, 0xD1, 0x4C, 0x8D, 0x54, 0x24,
                  0x40, 0x4C, 0x8D, 0x5C, 0x24, 0x70, 0x48, 0x89, 0xE1, 0x41,
                  0xB8, 0x08, 0x00, 0x00, 0x00, 0x41, 0x0F, 0xB6, 0x01, 0x84,
                  0xC0, 0x74, 0x74, 0x49, 0xFF, 0xC1, 0x3C, 0x25, 0x75, 0xF1,
                  0x41, 0x80, 0x39, 0x25, 0x75, 0x05, 0x49, 0xFF, 0xC1, 0xEB,
                  0xE6, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x74, 0x5A, 0x49,
                  0xFF, 0xC1, 0x3C, 0x2A, 0x74, 0x2B, 0x3C, 0x41, 0x72, 0xED,
                  0x0C, 0x20, 0x3C, 0x6C, 0x74, 0xE7, 0x3C, 0x68, 0x74, 0xE3,
                  0x3C, 0x7A, 0x74, 0xDF, 0x3C, 0x66, 0x74, 0x2A, 0x3C, 0x65,
                  0x74, 0x26, 0x3C, 0x67, 0x74, 0x22, 0x3C, 0x61, 0x74, 0x1E,
                  0x49, 0x8B, 0x02, 0x49, 0x83, 0xC2, 0x08, 0xEB, 0x1C, 0x49,
                  0x8B, 0x02, 0x49, 0x83, 0xC2, 0x08, 0x48, 0x89, 0x01, 0x48,
                  0x83, 0xC1, 0x08, 0x41, 0xFF, 0xC8, 0x75, 0xB3, 0xEB, 0x13,
                  0x49, 0x8B, 0x03, 0x49, 0x83, 0xC3, 0x08, 0x48, 0x89, 0x01,
                  0x48, 0x83, 0xC1, 0x08, 0x41, 0xFF, 0xC8, 0x75, 0x84, 0x48,
                  0x89, 0xE1, 0xE8, 0x08, 0x00, 0x00, 0x00, 0x48, 0x81, 0xC4,
                  0xA8, 0x00, 0x00, 0x00, 0xC3};
              seg_append(&text_seg, pre_snprintf, 265);
              labels[k].size = 297;
            } else if (strcmp(labels[k].name, "vprintf") == 0) {
              static const unsigned char pre_vprintf[199] = {
                  0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x49, 0x89, 0xF9,
                  0x49, 0x89, 0xF0, 0x48, 0x8D, 0x0C, 0x24, 0xBE, 0x10, 0x00,
                  0x00, 0x00, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x0F, 0x84,
                  0x95, 0x00, 0x00, 0x00, 0x49, 0xFF, 0xC1, 0x3C, 0x25, 0x75,
                  0xED, 0x41, 0x80, 0x39, 0x25, 0x75, 0x05, 0x49, 0xFF, 0xC1,
                  0xEB, 0xE2, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x74, 0x7B,
                  0x49, 0xFF, 0xC1, 0x3C, 0x2A, 0x74, 0x22, 0x3C, 0x41, 0x72,
                  0xED, 0x0C, 0x20, 0x3C, 0x6C, 0x74, 0xE7, 0x3C, 0x68, 0x74,
                  0xE3, 0x3C, 0x7A, 0x74, 0xDF, 0x3C, 0x66, 0x74, 0x24, 0x3C,
                  0x65, 0x74, 0x20, 0x3C, 0x67, 0x74, 0x1C, 0x3C, 0x61, 0x74,
                  0x18, 0x41, 0x8B, 0x00, 0x83, 0xF8, 0x30, 0x73, 0x2C, 0x4D,
                  0x8B, 0x58, 0x10, 0x4D, 0x8B, 0x14, 0x03, 0x83, 0xC0, 0x08,
                  0x41, 0x89, 0x00, 0xEB, 0x2B, 0x41, 0x8B, 0x40, 0x04, 0x3D,
                  0xB0, 0x00, 0x00, 0x00, 0x73, 0x11, 0x4D, 0x8B, 0x58, 0x10,
                  0x4D, 0x8B, 0x14, 0x03, 0x83, 0xC0, 0x10, 0x41, 0x89, 0x40,
                  0x04, 0xEB, 0x0F, 0x4D, 0x8B, 0x58, 0x08, 0x4D, 0x8B, 0x13,
                  0x49, 0x83, 0xC3, 0x08, 0x4D, 0x89, 0x58, 0x08, 0x4C, 0x89,
                  0x11, 0x48, 0x83, 0xC1, 0x08, 0xFF, 0xCE, 0x0F, 0x85, 0x5F,
                  0xFF, 0xFF, 0xFF, 0x48, 0x89, 0xE6, 0xE8, 0x08, 0x00, 0x00,
                  0x00, 0x48, 0x81, 0xC4, 0x88, 0x00, 0x00, 0x00, 0xC3};
              seg_append(&text_seg, pre_vprintf, 199);
              labels[k].size = 231;
            } else if (strcmp(labels[k].name, "vfprintf") == 0 ||
                       strcmp(labels[k].name, "vsprintf") == 0) {
              static const unsigned char pre_vfprintf[197] = {
                  0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x49, 0x89, 0xF1,
                  0x49, 0x89, 0xD0, 0x48, 0x8D, 0x0C, 0x24, 0x41, 0xBB, 0x10,
                  0x00, 0x00, 0x00, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x0F,
                  0x84, 0x92, 0x00, 0x00, 0x00, 0x49, 0xFF, 0xC1, 0x3C, 0x25,
                  0x75, 0xED, 0x41, 0x80, 0x39, 0x25, 0x75, 0x05, 0x49, 0xFF,
                  0xC1, 0xEB, 0xE2, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x74,
                  0x78, 0x49, 0xFF, 0xC1, 0x3C, 0x2A, 0x74, 0x22, 0x3C, 0x41,
                  0x72, 0xED, 0x0C, 0x20, 0x3C, 0x6C, 0x74, 0xE7, 0x3C, 0x68,
                  0x74, 0xE3, 0x3C, 0x7A, 0x74, 0xDF, 0x3C, 0x66, 0x74, 0x22,
                  0x3C, 0x65, 0x74, 0x1E, 0x3C, 0x67, 0x74, 0x1A, 0x3C, 0x61,
                  0x74, 0x16, 0x41, 0x8B, 0x00, 0x83, 0xF8, 0x30, 0x73, 0x28,
                  0x4D, 0x8B, 0x50, 0x10, 0x49, 0x8B, 0x04, 0x02, 0x41, 0x83,
                  0x00, 0x08, 0xEB, 0x29, 0x41, 0x8B, 0x40, 0x04, 0x3D, 0xB0,
                  0x00, 0x00, 0x00, 0x73, 0x0F, 0x4D, 0x8B, 0x50, 0x10, 0x49,
                  0x8B, 0x04, 0x02, 0x41, 0x83, 0x40, 0x04, 0x10, 0xEB, 0x0F,
                  0x4D, 0x8B, 0x50, 0x08, 0x49, 0x8B, 0x02, 0x49, 0x83, 0xC2,
                  0x08, 0x4D, 0x89, 0x50, 0x08, 0x48, 0x89, 0x01, 0x48, 0x83,
                  0xC1, 0x08, 0x41, 0xFF, 0xCB, 0x0F, 0x85, 0x62, 0xFF, 0xFF,
                  0xFF, 0x48, 0x89, 0xE2, 0xE8, 0x08, 0x00, 0x00, 0x00, 0x48,
                  0x81, 0xC4, 0x88, 0x00, 0x00, 0x00, 0xC3};
              seg_append(&text_seg, pre_vfprintf, 197);
              labels[k].size = 229;
            } else if (strcmp(labels[k].name, "vsnprintf") == 0 ||
                       strcmp(labels[k].name, "_vsnprintf") == 0) {
              static const unsigned char pre_vsnprintf[197] = {
                  0x48, 0x81, 0xEC, 0x88, 0x00, 0x00, 0x00, 0x49, 0x89, 0xD1,
                  0x49, 0x89, 0xC8, 0x48, 0x8D, 0x0C, 0x24, 0x41, 0xBB, 0x10,
                  0x00, 0x00, 0x00, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x0F,
                  0x84, 0x92, 0x00, 0x00, 0x00, 0x49, 0xFF, 0xC1, 0x3C, 0x25,
                  0x75, 0xED, 0x41, 0x80, 0x39, 0x25, 0x75, 0x05, 0x49, 0xFF,
                  0xC1, 0xEB, 0xE2, 0x41, 0x0F, 0xB6, 0x01, 0x84, 0xC0, 0x74,
                  0x78, 0x49, 0xFF, 0xC1, 0x3C, 0x2A, 0x74, 0x22, 0x3C, 0x41,
                  0x72, 0xED, 0x0C, 0x20, 0x3C, 0x6C, 0x74, 0xE7, 0x3C, 0x68,
                  0x74, 0xE3, 0x3C, 0x7A, 0x74, 0xDF, 0x3C, 0x66, 0x74, 0x22,
                  0x3C, 0x65, 0x74, 0x1E, 0x3C, 0x67, 0x74, 0x1A, 0x3C, 0x61,
                  0x74, 0x16, 0x41, 0x8B, 0x00, 0x83, 0xF8, 0x30, 0x73, 0x28,
                  0x4D, 0x8B, 0x50, 0x10, 0x49, 0x8B, 0x04, 0x02, 0x41, 0x83,
                  0x00, 0x08, 0xEB, 0x29, 0x41, 0x8B, 0x40, 0x04, 0x3D, 0xB0,
                  0x00, 0x00, 0x00, 0x73, 0x0F, 0x4D, 0x8B, 0x50, 0x10, 0x49,
                  0x8B, 0x04, 0x02, 0x41, 0x83, 0x40, 0x04, 0x10, 0xEB, 0x0F,
                  0x4D, 0x8B, 0x50, 0x08, 0x49, 0x8B, 0x02, 0x49, 0x83, 0xC2,
                  0x08, 0x4D, 0x89, 0x50, 0x08, 0x48, 0x89, 0x01, 0x48, 0x83,
                  0xC1, 0x08, 0x41, 0xFF, 0xCB, 0x0F, 0x85, 0x62, 0xFF, 0xFF,
                  0xFF, 0x48, 0x89, 0xE1, 0xE8, 0x08, 0x00, 0x00, 0x00, 0x48,
                  0x81, 0xC4, 0x88, 0x00, 0x00, 0x00, 0xC3};
              seg_append(&text_seg, pre_vsnprintf, 197);
              labels[k].size = 229;
            } else {
              labels[k].size = 32;
            }

            win_stub_offsets[win_import_count] = text_seg.size;
            win_import_funcs[win_import_count] = iat_name;

            /* Template for 32-byte Win64 import stub */
            unsigned char stub_tmpl[32] = {
                0x48, 0x83, 0xEC, 0x28,       /* sub $0x28, %rsp */
                0x4C, 0x89, 0x44, 0x24, 0x20, /* mov %r8, 0x20(%rsp) (arg 5) */
                0x49, 0x89, 0xC9,             /* mov %rcx, %r9       (arg 4) */
                0x49, 0x89, 0xD0,             /* mov %rdx, %r8       (arg 3) */
                0x48, 0x89, 0xF2,             /* mov %rsi, %rdx      (arg 2) */
                0x48, 0x89, 0xF9,             /* mov %rdi, %rcx      (arg 1) */
                0xFF, 0x15, 0x00, 0x00, 0x00,
                0x00,                   /* call *disp32(%rip) at offset 21 */
                0x48, 0x83, 0xC4, 0x28, /* add $0x28, %rsp */
                0xC3                    /* ret */
            };
            if (strcmp(labels[k].name, "malloc") == 0) {
              unsigned char malloc_stub[48] = {
                  72, 129, 236, 40, 4, 0, 0,
                  144, 144,
                  73, 137, 201,
                  73, 137, 208,
                  72, 137, 242,
                  72, 137, 249,
                  255, 21, 0, 0, 0, 0,
                  72, 129, 196, 40, 4, 0, 0,
                  195,
                  144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144, 144
              };
              seg_append(&text_seg, malloc_stub, 48);
            } else {
              if (strcmp(labels[k].name, "printf") == 0) {
                memset(stub_tmpl, 0x90, 4);
                stub_tmpl[30] = 0x78; /* free pre_printf 0x78 frame */
              }
              seg_append(&text_seg, stub_tmpl, 32);
            }
          }
          win_import_count++;
        }
      }
    }
  }

  /* Pass 2: Local relocation resolution */
  size_t i;
  for (i = 0; i < reloc_count; i++) {
    if (relocs[i].type == R_X86_64_TPOFF32) {
      int j = find_label_idx(relocs[i].target_name);
      if (j >= 0) {
        long long val = (long long)labels[j].offset + relocs[i].addend;
        text_seg.data[relocs[i].offset] = val & 0xFF;
        text_seg.data[relocs[i].offset + 1] = (val >> 8) & 0xFF;
        text_seg.data[relocs[i].offset + 2] = (val >> 16) & 0xFF;
        text_seg.data[relocs[i].offset + 3] = (val >> 24) & 0xFF;
        relocs[i].type = R_X86_64_NONE;
      }
      continue;
    }
    if (relocs[i].type == R_X86_64_64) continue;
    int j = find_label_idx(relocs[i].target_name);
    if (j >= 0 && labels[j].segment == 1) {
      /* Relative offset to local label */
      long long disp = (long long)labels[j].offset + relocs[i].addend -
                       (long long)relocs[i].offset;

      text_seg.data[relocs[i].offset] = disp & 0xFF;
      text_seg.data[relocs[i].offset + 1] = (disp >> 8) & 0xFF;
      text_seg.data[relocs[i].offset + 2] = (disp >> 16) & 0xFF;
      text_seg.data[relocs[i].offset + 3] = (disp >> 24) & 0xFF;

      /* Relocation is resolved, mark it as NONE so elf_emit ignores it */
      relocs[i].type = R_X86_64_NONE;
    }
  }

  /* Convert assembler symbols and relocations into elf_emit API structures */
  size_t final_sym_count = label_count;
  zcc_elf_sym_t *elf_syms =
      (zcc_elf_sym_t *)malloc(sizeof(zcc_elf_sym_t) * final_sym_count);
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
    } else if (labels[i].segment == 4) {
      elf_syms[i].type = STT_TLS;
      elf_syms[i].shndx = 1;
    } else {
      elf_syms[i].type = STT_NOTYPE;
      elf_syms[i].shndx = SHN_UNDEF;
    }
  }

  size_t orig_text_size = text_seg.size;
  if (!is_win64_exe) {
    /* Legacy merge of data_seg into text_seg (ELF and Win64 DLL v8) */
    if (data_seg.size > 0) {
      size_t pad_size = (16 - (text_seg.size % 16)) % 16;
      if (pad_size > 0) {
        unsigned char pad[16] = {0};
        seg_append(&text_seg, pad, pad_size);
      }
      size_t data_start = text_seg.size;
      seg_append(&text_seg, data_seg.data, data_seg.size);

      for (i = 0; i < final_sym_count; i++) {
        if (labels[i].segment == 2) {
          elf_syms[i].value = data_start + labels[i].offset;
          elf_syms[i].shndx = 1;
        }
      }
      for (i = 0; i < reloc_count; i++) {
        if (relocs[i].type == R_X86_64_64) {
          relocs[i].offset += data_start;
        }
      }
    }
  }

  if (is_win64_pe) {
    size_t max_win_relocs = reloc_count + 16;
    uint32_t *win_reloc_rvas =
        (uint32_t *)malloc(sizeof(uint32_t) * max_win_relocs);
    size_t win_reloc_count = 0;
    uint64_t image_base = 0x140000000ULL;
    uint32_t text_rva = 0x1000;

    /* Determine entry point offset: GUI WinMain, then mainCRTStartup, then main */
    uint32_t entry_offset = 0;
    int found_entry = 0;
    if (g_win64_gui_subsystem) {
      for (i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, "WinMain") == 0 ||
            strcmp(labels[i].name, "_WinMain") == 0 ||
            strcmp(labels[i].name, "wWinMain") == 0 ||
            strcmp(labels[i].name, "_wWinMain") == 0) {
          entry_offset = (uint32_t)labels[i].offset;
          found_entry = 1;
          break;
        }
      }
    }
    if (!found_entry) {
      for (i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, "mainCRTStartup") == 0 ||
            strcmp(labels[i].name, "_mainCRTStartup") == 0) {
          entry_offset = (uint32_t)labels[i].offset;
          found_entry = 1;
          break;
        }
      }
    }
    if (!found_entry) {
      for (i = 0; i < label_count; i++) {
        if (strcmp(labels[i].name, "main") == 0 ||
            strcmp(labels[i].name, "_main") == 0) {
          entry_offset = (uint32_t)labels[i].offset;
          break;
        }
      }
    }

    size_t data_start_v8 = orig_text_size + ((16 - (orig_text_size % 16)) % 16);
    long long win_crt_in_patch = -1;
    long long win_crt_out_patch = -1;
    long long win_crt_err_patch = -1;

    /* If stdin/stdout/stderr globals are used, synthesize CRT stdio init wrapper */
    if (!g_win64_is_shared && win_iob_func_idx >= 0 &&
        (win_stdio_stdin_idx >= 0 || win_stdio_stdout_idx >= 0 ||
         win_stdio_stderr_idx >= 0)) {
      size_t pad = (16 - (text_seg.size % 16)) % 16;
      if (pad > 0) {
        unsigned char pad_b[16] = {0};
        seg_append(&text_seg, pad_b, pad);
      }
      uint32_t crt_entry = (uint32_t)text_seg.size;
      unsigned char sub_rsp[4] = {0x48, 0x83, 0xEC, 0x28};
      seg_append(&text_seg, sub_rsp, 4);
      int32_t call_disp = (int32_t)((int64_t)labels[win_iob_func_idx].offset -
                                    (int64_t)(text_seg.size + 5));
      unsigned char call_ins[5] = {0xE8, (unsigned char)(call_disp & 0xFF),
                                   (unsigned char)((call_disp >> 8) & 0xFF),
                                   (unsigned char)((call_disp >> 16) & 0xFF),
                                   (unsigned char)((call_disp >> 24) & 0xFF)};
      seg_append(&text_seg, call_ins, 5);
      if (win_stdio_stdin_idx >= 0) {
        unsigned char mov_in[7] = {0x48, 0x89, 0x05, 0, 0, 0, 0};
        win_crt_in_patch = (long long)(text_seg.size + 3);
        seg_append(&text_seg, mov_in, 7);
      }
      if (win_stdio_stdout_idx >= 0) {
        unsigned char lea_out[4] = {0x48, 0x8D, 0x48, 0x30};
        seg_append(&text_seg, lea_out, 4);
        unsigned char mov_out[7] = {0x48, 0x89, 0x0D, 0, 0, 0, 0};
        win_crt_out_patch = (long long)(text_seg.size + 3);
        seg_append(&text_seg, mov_out, 7);
      }
      if (win_stdio_stderr_idx >= 0) {
        unsigned char lea_err[4] = {0x48, 0x8D, 0x48, 0x60};
        seg_append(&text_seg, lea_err, 4);
        unsigned char mov_err[7] = {0x48, 0x89, 0x0D, 0, 0, 0, 0};
        win_crt_err_patch = (long long)(text_seg.size + 3);
        seg_append(&text_seg, mov_err, 7);
      }
      unsigned char add_rsp[4] = {0x48, 0x83, 0xC4, 0x28};
      seg_append(&text_seg, add_rsp, 4);
      int32_t jmp_disp =
          (int32_t)((int64_t)entry_offset - (int64_t)(text_seg.size + 5));
      unsigned char jmp_ins[5] = {0xE9, (unsigned char)(jmp_disp & 0xFF),
                                  (unsigned char)((jmp_disp >> 8) & 0xFF),
                                  (unsigned char)((jmp_disp >> 16) & 0xFF),
                                  (unsigned char)((jmp_disp >> 24) & 0xFF)};
      seg_append(&text_seg, jmp_ins, 5);
      entry_offset = crt_entry;
      orig_text_size = text_seg.size;
    }

    size_t bss_start = 0;
    size_t data_vsize = 0;
    uint32_t sep_data_rva = text_rva + (uint32_t)data_start_v8;
    if (is_win64_exe) {
      size_t dpad = (16 - (data_seg.size % 16)) % 16;
      if (dpad > 0) {
        unsigned char z16[16] = {0};
        seg_append(&data_seg, z16, dpad);
      }
      bss_start = data_seg.size;
      data_vsize = ((bss_start + win_bss_size) > 16)
                       ? (bss_start + win_bss_size)
                       : 16;
      uint32_t raw_tlen = (text_seg.size > 0) ? (uint32_t)text_seg.size : 16;
      sep_data_rva = text_rva + win64_pe_align_to(raw_tlen, 0x1000);
    }

    if (win_crt_in_patch >= 0) {
      uint32_t tgt_rva = sep_data_rva + (uint32_t)labels[win_stdio_stdin_idx].offset;
      int32_t d = (int32_t)((int64_t)tgt_rva - (int64_t)(text_rva + (uint32_t)win_crt_in_patch + 4));
      memcpy(&text_seg.data[win_crt_in_patch], &d, 4);
    }
    if (win_crt_out_patch >= 0) {
      uint32_t tgt_rva = sep_data_rva + (uint32_t)labels[win_stdio_stdout_idx].offset;
      int32_t d = (int32_t)((int64_t)tgt_rva - (int64_t)(text_rva + (uint32_t)win_crt_out_patch + 4));
      memcpy(&text_seg.data[win_crt_out_patch], &d, 4);
    }
    if (win_crt_err_patch >= 0) {
      uint32_t tgt_rva = sep_data_rva + (uint32_t)labels[win_stdio_stderr_idx].offset;
      int32_t d = (int32_t)((int64_t)tgt_rva - (int64_t)(text_rva + (uint32_t)win_crt_err_patch + 4));
      memcpy(&text_seg.data[win_crt_err_patch], &d, 4);
    }

    /* Resolve relocations against separated .data/.bss (exe v9) or merged .text (dll v8) */
    for (i = 0; i < reloc_count; i++) {
      if (relocs[i].type == R_X86_64_64) {
        int j = find_label_idx(relocs[i].target_name);
        if (j >= 0) {
          if (is_win64_exe) {
            uint64_t tgt_rva = 0;
            if (labels[j].segment == 1)
              tgt_rva = (uint64_t)(text_rva + labels[j].offset);
            else if (labels[j].segment == 2)
              tgt_rva = (uint64_t)(sep_data_rva + labels[j].offset);
            else if (labels[j].segment == 3)
              tgt_rva = (uint64_t)(sep_data_rva + bss_start + labels[j].offset);
            uint64_t abs_va = image_base + tgt_rva + (uint64_t)relocs[i].addend;
            if (data_seg.data && relocs[i].offset + 8 <= data_seg.size) {
              memcpy(&data_seg.data[relocs[i].offset], &abs_va, sizeof(uint64_t));
            }
            if (win_reloc_rvas && win_reloc_count < max_win_relocs) {
              win_reloc_rvas[win_reloc_count++] =
                  sep_data_rva + (uint32_t)relocs[i].offset;
            }
          } else {
            uint64_t target_offset = 0;
            if (labels[j].segment == 1)
              target_offset = (uint64_t)labels[j].offset;
            else if (labels[j].segment == 2)
              target_offset = (uint64_t)(data_start_v8 + labels[j].offset);
            target_offset += (uint64_t)relocs[i].addend;
            uint64_t abs_va = image_base + text_rva + target_offset;
            if (relocs[i].offset + 8 <= text_seg.size) {
              memcpy(&text_seg.data[relocs[i].offset], &abs_va, sizeof(uint64_t));
            }
            if (win_reloc_rvas && win_reloc_count < max_win_relocs) {
              win_reloc_rvas[win_reloc_count++] =
                  text_rva + (uint32_t)relocs[i].offset;
            }
          }
          relocs[i].type = R_X86_64_NONE;
        }
      } else if (relocs[i].type == R_X86_64_PC32) {
        int j = find_label_idx(relocs[i].target_name);
        if (j >= 0 && (labels[j].segment == 2 || labels[j].segment == 3)) {
          long long disp = 0;
          if (is_win64_exe) {
            uint64_t tgt_rva = (labels[j].segment == 3)
                                   ? (uint64_t)(sep_data_rva + bss_start + labels[j].offset)
                                   : (uint64_t)(sep_data_rva + labels[j].offset);
            disp = (long long)tgt_rva + relocs[i].addend -
                   (long long)(text_rva + relocs[i].offset);
          } else if (data_seg.size > 0 && labels[j].segment == 2) {
            disp = (long long)(data_start_v8 + labels[j].offset) +
                   relocs[i].addend - (long long)relocs[i].offset;
          } else {
            continue;
          }
          text_seg.data[relocs[i].offset] = disp & 0xFF;
          text_seg.data[relocs[i].offset + 1] = (disp >> 8) & 0xFF;
          text_seg.data[relocs[i].offset + 2] = (disp >> 16) & 0xFF;
          text_seg.data[relocs[i].offset + 3] = (disp >> 24) & 0xFF;
          relocs[i].type = R_X86_64_NONE;
        }
      }
    }

    /* If imports present, calculate IAT RVAs and patch stub call displacements */
    if (win_import_count > 0) {
      uint32_t iat_rvas[512];
      zcc_win64_pe_calc_iat_rvas_ex(text_seg.size, is_win64_exe ? data_vsize : 0,
                                    win_import_funcs, win_import_count, iat_rvas);
      size_t k;
      for (k = 0; k < win_import_count; k++) {
        uint32_t stub_rva = 0x1000 + (uint32_t)win_stub_offsets[k];
        int32_t disp = (int32_t)(iat_rvas[k] - (stub_rva + 27));
        text_seg.data[win_stub_offsets[k] + 23] = disp & 0xFF;
        text_seg.data[win_stub_offsets[k] + 24] = (disp >> 8) & 0xFF;
        text_seg.data[win_stub_offsets[k] + 25] = (disp >> 16) & 0xFF;
        text_seg.data[win_stub_offsets[k] + 26] = (disp >> 24) & 0xFF;
      }
    }

    /* Extract function extents for Win64 PE .pdata exception directory */
    size_t max_extents = (label_count > 256) ? (label_count + 1) : 256;
    win64_pe_func_extent_t *win_extents = (win64_pe_func_extent_t *)malloc(
        sizeof(win64_pe_func_extent_t) * max_extents);
    uint32_t *func_offsets = (uint32_t *)malloc(sizeof(uint32_t) * max_extents);
    size_t win_extent_count = 0;
    size_t func_count = 0;
    if (win_extents && func_offsets) {
      int has_any_seh = 0;
      for (i = 0; i < label_count; i++) {
        if (labels[i].segment == 1 && labels[i].name[0] != '.' &&
            labels[i].name[0] != '\0') {
          if (strncmp(labels[i].name, "seh_", 4) == 0 ||
              strncmp(labels[i].name, "__seh_", 6) == 0 ||
              strstr(labels[i].name, "_seh_") ||
              strstr(labels[i].name, "try_begin") ||
              strstr(labels[i].name, "_handler")) {
            has_any_seh = 1;
          }
          if (strncmp(labels[i].name, "seh_", 4) != 0 &&
              strncmp(labels[i].name, "__seh_", 6) != 0 &&
              func_count < max_extents) {
            func_offsets[func_count++] = (uint32_t)labels[i].offset;
          }
        }
      }
      if (func_count > 1) {
        qsort(func_offsets, func_count, sizeof(uint32_t), codegen_cmp_u32);
      }
      size_t fi;
      size_t ucount = 0;
      for (fi = 0; fi < func_count; fi++) {
        if (ucount == 0 || func_offsets[fi] != func_offsets[ucount - 1]) {
          func_offsets[ucount++] = func_offsets[fi];
        }
      }
      memset(win_extents, 0, sizeof(win64_pe_func_extent_t) * max_extents);
      for (fi = 0; fi < ucount; fi++) {
        uint32_t b = func_offsets[fi];
        uint32_t e =
            (fi + 1 < ucount) ? func_offsets[fi + 1] : (uint32_t)orig_text_size;
        if (e > b && win_extent_count < max_extents) {
          win_extents[win_extent_count].begin_offset = b;
          win_extents[win_extent_count].end_offset = e;
          win_extents[win_extent_count].flags = WIN64_UNW_FLAG_NHANDLER;
          win_extents[win_extent_count].handler_rva = 0;
          win_extents[win_extent_count].num_scopes = 0;

          if (has_any_seh) {
            const char *fn_name = "";
            size_t li;
            for (li = 0; li < label_count; li++) {
              if (labels[li].segment == 1 && (uint32_t)labels[li].offset == b &&
                  labels[li].name[0] != '.' && labels[li].name[0] != '\0') {
                fn_name = labels[li].name;
                break;
              }
            }
            char companion_name[128];
            snprintf(companion_name, sizeof(companion_name), "%s_handler",
                     fn_name);
            uint32_t handler_rva = 0;
            for (li = 0; li < label_count; li++) {
              if (labels[li].segment == 1 &&
                  (strcmp(labels[li].name, companion_name) == 0 ||
                   strcmp(labels[li].name, "__seh_handler") == 0 ||
                   strcmp(labels[li].name, "_seh_handler") == 0)) {
                handler_rva = 0x1000 + (uint32_t)labels[li].offset;
                break;
              }
            }
            uint32_t try_b = 0, try_e = 0, target = 0;
            int has_try_b = 0, has_try_e = 0, has_target = 0;
            for (li = 0; li < label_count; li++) {
              if (labels[li].segment == 1 && (uint32_t)labels[li].offset >= b &&
                  (uint32_t)labels[li].offset < e) {
                const char *lname = labels[li].name;
                if (strstr(lname, "try_end") || strstr(lname, "seh_end")) {
                  try_e = (uint32_t)labels[li].offset;
                  has_try_e = 1;
                } else if (strstr(lname, "try_begin") ||
                           strstr(lname, "seh_begin") ||
                           strstr(lname, "seh_try")) {
                  try_b = (uint32_t)labels[li].offset;
                  has_try_b = 1;
                } else if (strstr(lname, "target") || strstr(lname, "except") ||
                           strstr(lname, "seh_catch")) {
                  target = (uint32_t)labels[li].offset;
                  has_target = 1;
                }
              }
            }
            if (handler_rva != 0 || (has_try_b && has_try_e && has_target)) {
              win_extents[win_extent_count].flags = WIN64_UNW_FLAG_EHANDLER;
              win_extents[win_extent_count].handler_rva = handler_rva;
              if (has_try_b && has_try_e && has_target) {
                win_extents[win_extent_count].num_scopes = 1;
                win_extents[win_extent_count].scopes[0].try_begin_offset = try_b;
                win_extents[win_extent_count].scopes[0].try_end_offset = try_e;
                win_extents[win_extent_count].scopes[0].handler_offset = 1;
                win_extents[win_extent_count].scopes[0].target_offset = target;
              }
            }
          }
          win_extent_count++;
        }
      }
    }

    /* Read certificate data if specified */
    win64_pe_cert_info_t cert_info;
    memset(&cert_info, 0, sizeof(cert_info));
    uint8_t *cert_data_buf = NULL;
    if (g_win64_cert_file) {
      FILE *cf = fopen(g_win64_cert_file, "rb");
      if (cf) {
        fseek(cf, 0, SEEK_END);
        long csz = ftell(cf);
        fseek(cf, 0, SEEK_SET);
        if (csz > 0) {
          cert_data_buf = (uint8_t *)malloc(csz);
          if (cert_data_buf &&
              fread(cert_data_buf, 1, csz, cf) == (size_t)csz) {
            cert_info.cert_data = cert_data_buf;
            cert_info.cert_len = (size_t)csz;
            cert_info.revision = WIN_CERT_REVISION_2_0;
            cert_info.cert_type = WIN_CERT_TYPE_PKCS_SIGNED_DATA;
          }
        }
        fclose(cf);
      }
    }

    /* Build delay import info if specified */
    win64_pe_delay_dll_t delay_dll_items[16];
    win64_pe_delay_info_t delay_info;
    memset(&delay_info, 0, sizeof(delay_info));
    if (g_win64_delay_dll_count > 0) {
      size_t d;
      for (d = 0; d < g_win64_delay_dll_count; d++) {
        delay_dll_items[d].dll_name = g_win64_delay_dlls[d];
        delay_dll_items[d].func_names = (const char **)g_win64_delay_funcs[d];
        delay_dll_items[d].num_funcs = g_win64_delay_func_counts[d];
      }
      delay_info.dlls = delay_dll_items;
      delay_info.num_dlls = g_win64_delay_dll_count;
    }

    int ret = 0;
    if (g_win64_is_shared) {
      /* Gather exported functions from labels */
      size_t max_exp = label_count + 1;
      win64_pe_export_t *win_exports =
          (win64_pe_export_t *)malloc(sizeof(win64_pe_export_t) * max_exp);
      size_t win_export_count = 0;
      if (win_exports) {
        for (i = 0; i < label_count; i++) {
          if (labels[i].segment == 1 && labels[i].name[0] != '.' &&
              labels[i].name[0] != '\0' &&
              strcmp(labels[i].name, "DllMain") != 0 &&
              strcmp(labels[i].name, "_DllMain") != 0 &&
              strcmp(labels[i].name, "main") != 0 &&
              strcmp(labels[i].name, "_main") != 0) {
            win_exports[win_export_count].name = labels[i].name;
            win_exports[win_export_count].func_rva =
                0x1000 + (uint32_t)labels[i].offset;
            win_export_count++;
          }
        }
      }
      /* Default DLL name from out_o_filename */
      const char *dname = strrchr(out_o_filename, '/');
      if (!dname)
        dname = strrchr(out_o_filename, '\\');
      dname = dname ? (dname + 1) : out_o_filename;

      win64_pe_tls_info_t tls_info;
      memset(&tls_info, 0, sizeof(tls_info));
      int has_tls = (tls_seg.size > 0 || tls_zero_fill > 0);
      if (has_tls) {
        tls_info.raw_data = tls_seg.data;
        tls_info.raw_data_size = tls_seg.size;
        tls_info.zero_fill_size = (uint32_t)tls_zero_fill;
      }

      ret = zcc_emit_win64_pe_dll_v8(
          out_o_filename, dname, text_seg.data, text_seg.size, entry_offset,
          win_import_count > 0 ? win_import_funcs : NULL, win_import_count,
          win_exports, win_export_count,
          win_reloc_count > 0 ? win_reloc_rvas : NULL, win_reloc_count,
          win_extent_count > 0 ? win_extents : NULL, win_extent_count,
          has_tls ? &tls_info : NULL, cert_info.cert_data ? &cert_info : NULL,
          g_win64_delay_dll_count > 0 ? &delay_info : NULL);

      /* Automatically emit .def and MSVC .lib import library */
      if (ret == 0 && win_export_count > 0) {
        char def_path[1024];
        char lib_path[1024];
        strncpy(def_path, out_o_filename, sizeof(def_path) - 1);
        def_path[sizeof(def_path) - 1] = '\0';
        strncpy(lib_path, out_o_filename, sizeof(lib_path) - 1);
        lib_path[sizeof(lib_path) - 1] = '\0';
        char *dot_def = strrchr(def_path, '.');
        char *dot_lib = strrchr(lib_path, '.');
        if (dot_def)
          strcpy(dot_def, ".def");
        else
          strncat(def_path, ".def", sizeof(def_path) - strlen(def_path) - 1);
        if (dot_lib)
          strcpy(dot_lib, ".lib");
        else
          strncat(lib_path, ".lib", sizeof(lib_path) - strlen(lib_path) - 1);
        zcc_emit_win64_def_file(def_path, dname, win_exports, win_export_count);
        zcc_emit_win64_implib(lib_path, dname, win_exports, win_export_count);
      }
      if (win_exports)
        free(win_exports);
    } else {
      uint16_t target_subsystem = g_win64_gui_subsystem ? 2 : 3;
      win64_pe_tls_info_t tls_info;
      memset(&tls_info, 0, sizeof(tls_info));
      int has_tls = (tls_seg.size > 0 || tls_zero_fill > 0);
      if (has_tls) {
        tls_info.raw_data = tls_seg.data;
        tls_info.raw_data_size = tls_seg.size;
        tls_info.zero_fill_size = (uint32_t)tls_zero_fill;
      }
      ret = zcc_emit_win64_pe_file_v9(
          out_o_filename, text_seg.data, text_seg.size, entry_offset,
          data_seg.data, data_seg.size, win_bss_size,
          win_import_count > 0 ? win_import_funcs : NULL, win_import_count,
          win_reloc_count > 0 ? win_reloc_rvas : NULL, win_reloc_count,
          win_extent_count > 0 ? win_extents : NULL, win_extent_count,
          target_subsystem, NULL, 0, has_tls ? &tls_info : NULL,
          cert_info.cert_data ? &cert_info : NULL,
          g_win64_delay_dll_count > 0 ? &delay_info : NULL);
    }
    if (win_reloc_rvas)
      free(win_reloc_rvas);
    if (win_extents)
      free(win_extents);
    if (func_offsets)
      free(func_offsets);
    free(elf_syms);
    free(text_seg.data);
    free(data_seg.data);
    if (tls_seg.data)
      free(tls_seg.data);
    if (cert_data_buf)
      free(cert_data_buf);
    return ret;
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
    elf_relas =
        (zcc_elf_rela_t *)malloc(sizeof(zcc_elf_rela_t) * final_reloc_count);
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
  int ret = elf_emit_obj(out_o_filename, text_seg.data, text_seg.size, elf_syms,
                         final_sym_count, elf_relas, final_reloc_count);

  /* Free allocated resources */
  if (elf_relas)
    free(elf_relas);
  free(elf_syms);
  free(text_seg.data);
  free(data_seg.data);

  return ret;
}

#ifdef main
#undef main
#endif

extern int zld_link(const char **obj_files, int obj_count, const char *out_path,
                    const char *script_path, const char *tensor_attest_bin_path,
                    const char *tensor_note_json_path,
                    const char *build_attest_bin_path);

static void zcc_hardware_profile_buffer(const char *buf, size_t sz,
                                        const char *source_desc, int is_json) {
  if (!buf || sz == 0)
    return;
  int total_insns = 0, alu_cycles = 0, mul_cycles = 0, div_cycles = 0,
      mem_cycles = 0, branch_cycles = 0, stack_spills = 0;
  const char *p = buf, *end = buf + sz;
  while (p < end) {
    const char *eol = (const char *)memchr(p, '\n', end - p);
    if (!eol)
      eol = end;
    size_t line_len = eol - p;
    if (line_len > 0) {
      char line[512];
      size_t copy_len =
          line_len < sizeof(line) - 1 ? line_len : sizeof(line) - 1;
      memcpy(line, p, copy_len);
      line[copy_len] = '\0';
      char *s = line;
      while (*s == ' ' || *s == '\t')
        s++;
      if (*s != '\0' && *s != '.' && *s != '#' && *s != '/' && *s != ';') {
        char *colon = strchr(s, ':');
        if (colon && *(colon + 1) == '\0') {
          // label
        } else {
          total_insns++;
          char op[32] = {0};
          sscanf(s, "%31s", op);
          for (int k = 0; op[k]; k++) {
            if (op[k] >= 'A' && op[k] <= 'Z')
              op[k] += 32;
          }
          if (strcmp(op, "idiv") == 0 || strcmp(op, "idivq") == 0 ||
              strcmp(op, "idivl") == 0 || strcmp(op, "div") == 0 ||
              strcmp(op, "divq") == 0 || strcmp(op, "divl") == 0) {
            div_cycles += 35;
          } else if (strcmp(op, "imul") == 0 || strcmp(op, "imulq") == 0 ||
                     strcmp(op, "imull") == 0 || strcmp(op, "mul") == 0 ||
                     strcmp(op, "mulq") == 0) {
            mul_cycles += 3;
          } else if (op[0] == 'j') {
            branch_cycles += 3;
          } else if (strstr(s, "(%rbp)") || strstr(s, "(%rsp)")) {
            mem_cycles += 4;
            stack_spills++;
          } else {
            alu_cycles += 1;
          }
        }
      }
    }
    p = eol + 1;
  }
  int total_est_cycles =
      alu_cycles + mul_cycles + div_cycles + mem_cycles + branch_cycles;
  if (is_json) {
    printf("{\"target\":\"%s\",\"insns\":%d,\"cycles\":%d,\"alu\":%d,\"mul\":%"
           "d,\"div\":%d,\"mem\":%d,\"branch\":%d,\"spills\":%d}\n",
           source_desc ? source_desc : "target", total_insns, total_est_cycles,
           alu_cycles, mul_cycles / 3, div_cycles / 35, mem_cycles / 4,
           branch_cycles / 3, stack_spills);
  } else {
    fprintf(stderr, "\n========================================================"
                    "======================\n");
    fprintf(stderr, " [ZCC HARDWARE SILICON PROFILE REPORT] %s\n",
            source_desc ? source_desc : "");
    fprintf(stderr, "=========================================================="
                    "====================\n");
    fprintf(stderr, "  * Emitted Executable Instructions: %d\n", total_insns);
    fprintf(stderr, "  * Estimated Hardware Cycles:     ~%d cycles\n",
            total_est_cycles);
    fprintf(stderr, "  * Micro-Architectural Breakdown:\n");
    fprintf(stderr, "    - ALU Operations:   %3d insns  (%d cycles)\n",
            alu_cycles, alu_cycles);
    fprintf(stderr, "    - Multiplications:  %3d insns  (%d cycles)\n",
            mul_cycles / 3, mul_cycles);
    fprintf(stderr, "    - Division Latency: %3d insns  (%d cycles)\n",
            div_cycles / 35, div_cycles);
    fprintf(stderr,
            "    - Memory Accesses:  %3d insns  (%d cycles, %d stack spills)\n",
            mem_cycles / 4, mem_cycles, stack_spills);
    fprintf(stderr, "    - Branch Hazards:   %3d jumps  (%d cycles)\n",
            branch_cycles / 3, branch_cycles);
    fprintf(stderr, "=========================================================="
                    "====================\n\n");
  }
}

static void zcc_hardware_profile_file(const char *filename, int is_json) {
  if (!filename)
    return;
  FILE *f = fopen(filename, "rb");
  if (!f)
    return;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz > 0 && sz < 32 * 1024 * 1024) {
    char *buf = (char *)malloc(sz + 1);
    if (buf) {
      size_t r = fread(buf, 1, sz, f);
      buf[r] = '\0';
      zcc_hardware_profile_buffer(buf, r, filename, is_json);
      free(buf);
    }
  }
  fclose(f);
}

static const char *rt_asm_exit =
    ".text\n"
    ".globl exit\n"
    ".globl _exit\n"
    "exit:\n"
    "  movl $231, %eax\n"
    "  syscall\n"
    "  movl $60, %eax\n"
    "  syscall\n"
    "_exit:\n"
    "  movl $60, %eax\n"
    "  syscall\n";

static const char *rt_asm_write =
    ".text\n"
    ".globl write\n"
    "write:\n"
    "  movl $1, %eax\n"
    "  syscall\n"
    "  ret\n";

static const char *rt_asm_read =
    ".text\n"
    ".globl read\n"
    "read:\n"
    "  xorl %eax, %eax\n"
    "  syscall\n"
    "  ret\n";

static const char *rt_asm_open =
    ".text\n"
    ".globl open\n"
    "open:\n"
    "  movl $2, %eax\n"
    "  syscall\n"
    "  ret\n";

static const char *rt_asm_close =
    ".text\n"
    ".globl close\n"
    "close:\n"
    "  movl $3, %eax\n"
    "  syscall\n"
    "  ret\n";

static const char *rt_asm_putchar =
    ".text\n"
    ".globl putchar\n"
    "putchar:\n"
    "  subq $16, %rsp\n"
    "  movq %rdi, %r8\n"
    "  movq %rsp, %rsi\n"
    "  movb %dil, (%rsi)\n"
    "  movl $1, %edi\n"
    "  movl $1, %edx\n"
    "  movl $1, %eax\n"
    "  syscall\n"
    "  movq %r8, %rax\n"
    "  andl $255, %eax\n"
    "  addq $16, %rsp\n"
    "  ret\n";

static const char *rt_asm_puts =
    ".text\n"
    ".globl puts\n"
    "puts:\n"
    "  testq %rdi, %rdi\n"
    "  jne .Lputs_ok\n"
    "  movl $-1, %eax\n"
    "  ret\n"
    ".Lputs_ok:\n"
    "  movq %rdi, %rsi\n"
    "  xorq %rdx, %rdx\n"
    ".Lputs_len:\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpb $0, %al\n"
    "  je .Lputs_wr\n"
    "  incq %rdi\n"
    "  incq %rdx\n"
    "  jmp .Lputs_len\n"
    ".Lputs_wr:\n"
    "  movl $1, %edi\n"
    "  movl $1, %eax\n"
    "  syscall\n"
    "  subq $16, %rsp\n"
    "  movl $10, %eax\n"
    "  movq %rsp, %rsi\n"
    "  movb %al, (%rsi)\n"
    "  movl $1, %edi\n"
    "  movl $1, %edx\n"
    "  movl $1, %eax\n"
    "  syscall\n"
    "  addq $16, %rsp\n"
    "  movl $1, %eax\n"
    "  ret\n";

static const char *rt_asm_strlen =
    ".text\n"
    ".globl strlen\n"
    "strlen:\n"
    "  xorq %rax, %rax\n"
    ".Lsl_lp:\n"
    "  movzbq (%rdi), %rcx\n"
    "  cmpb $0, %cl\n"
    "  je .Lsl_dn\n"
    "  incq %rdi\n"
    "  incq %rax\n"
    "  jmp .Lsl_lp\n"
    ".Lsl_dn:\n"
    "  ret\n";

static const char *rt_asm_strcmp =
    ".text\n"
    ".globl strcmp\n"
    "strcmp:\n"
    ".Lsc_lp:\n"
    "  movzbq (%rdi), %rax\n"
    "  movzbq (%rsi), %rcx\n"
    "  cmpq %rcx, %rax\n"
    "  jne .Lsc_df\n"
    "  cmpq $0, %rax\n"
    "  je .Lsc_eq\n"
    "  incq %rdi\n"
    "  incq %rsi\n"
    "  jmp .Lsc_lp\n"
    ".Lsc_df:\n"
    "  subq %rcx, %rax\n"
    "  ret\n"
    ".Lsc_eq:\n"
    "  xorl %eax, %eax\n"
    "  ret\n";

static const char *rt_asm_strncmp =
    ".text\n"
    ".globl strncmp\n"
    "strncmp:\n"
    ".Lsn_lp:\n"
    "  testq %rdx, %rdx\n"
    "  je .Lsn_eq\n"
    "  movzbq (%rdi), %rax\n"
    "  movzbq (%rsi), %rcx\n"
    "  cmpq %rcx, %rax\n"
    "  jne .Lsn_df\n"
    "  cmpq $0, %rax\n"
    "  je .Lsn_eq\n"
    "  incq %rdi\n"
    "  incq %rsi\n"
    "  decq %rdx\n"
    "  jmp .Lsn_lp\n"
    ".Lsn_df:\n"
    "  subq %rcx, %rax\n"
    "  ret\n"
    ".Lsn_eq:\n"
    "  xorl %eax, %eax\n"
    "  ret\n";

static const char *rt_asm_strcpy =
    ".text\n"
    ".globl strcpy\n"
    "strcpy:\n"
    "  movq %rdi, %rax\n"
    ".Lscp_lp:\n"
    "  movzbq (%rsi), %rcx\n"
    "  movb %cl, (%rdi)\n"
    "  incq %rsi\n"
    "  incq %rdi\n"
    "  cmpq $0, %rcx\n"
    "  jne .Lscp_lp\n"
    "  ret\n";

static const char *rt_asm_memcpy =
    ".text\n"
    ".globl memcpy\n"
    "memcpy:\n"
    "  movq %rdi, %rax\n"
    "  movq %rdx, %rcx\n"
    "  rep movsb\n"
    "  ret\n";

static const char *rt_asm_memset =
    ".text\n"
    ".globl memset\n"
    "memset:\n"
    "  movq %rdi, %rax\n"
    "  movq %rdx, %rcx\n"
    "  testq %rcx, %rcx\n"
    "  je .Lms_dn\n"
    ".Lms_lp:\n"
    "  movb %sil, (%rdi)\n"
    "  incq %rdi\n"
    "  decq %rcx\n"
    "  jne .Lms_lp\n"
    ".Lms_dn:\n"
    "  ret\n";

static const char *rt_asm_memmove =
    ".text\n"
    ".globl memmove\n"
    "memmove:\n"
    "  movq %rdi, %rax\n"
    "  movq %rdx, %rcx\n"
    "  testq %rcx, %rcx\n"
    "  je .Lmm_dn\n"
    "  cmpq %rsi, %rdi\n"
    "  jbe .Lmm_fwd\n"
    "  addq %rcx, %rdi\n"
    "  addq %rcx, %rsi\n"
    ".Lmm_bwd:\n"
    "  decq %rdi\n"
    "  decq %rsi\n"
    "  movzbq (%rsi), %r8\n"
    "  movb %r8b, (%rdi)\n"
    "  decq %rcx\n"
    "  jne .Lmm_bwd\n"
    "  ret\n"
    ".Lmm_fwd:\n"
    "  rep movsb\n"
    ".Lmm_dn:\n"
    "  ret\n";

static const char *rt_asm_memcmp =
    ".text\n"
    ".globl memcmp\n"
    "memcmp:\n"
    ".Lmc_lp:\n"
    "  testq %rdx, %rdx\n"
    "  je .Lmc_eq\n"
    "  movzbq (%rdi), %rax\n"
    "  movzbq (%rsi), %rcx\n"
    "  cmpq %rcx, %rax\n"
    "  jne .Lmc_df\n"
    "  incq %rdi\n"
    "  incq %rsi\n"
    "  decq %rdx\n"
    "  jmp .Lmc_lp\n"
    ".Lmc_df:\n"
    "  subq %rcx, %rax\n"
    "  ret\n"
    ".Lmc_eq:\n"
    "  xorl %eax, %eax\n"
    "  ret\n";

static const char *rt_asm_alloc =
    ".text\n"
    ".globl malloc\n"
    ".globl calloc\n"
    ".globl realloc\n"
    ".globl free\n"
    ".comm __zcc_heap, 16777216, 16\n"
    ".comm __zcc_heap_off, 8, 8\n"
    "malloc:\n"
    "  movq %rdi, %r8\n"
    "  addq $31, %rdi\n"
    "  andq $-16, %rdi\n"
    "  movq __zcc_heap_off(%rip), %rax\n"
    "  movq %rax, %rcx\n"
    "  addq %rdi, %rcx\n"
    "  cmpq $16777216, %rcx\n"
    "  ja .Lmal_mmap\n"
    "  movq %rcx, __zcc_heap_off(%rip)\n"
    "  leaq __zcc_heap(%rip), %rdx\n"
    "  addq %rdx, %rax\n"
    "  movq %r8, 0(%rax)\n"
    "  addq $16, %rax\n"
    "  ret\n"
    ".Lmal_mmap:\n"
    "  pushq %r8\n"
    "  movq %rdi, %rsi\n"
    "  xorq %rdi, %rdi\n"
    "  movl $3, %edx\n"
    "  movl $34, %r10d\n"
    "  movq $-1, %r8\n"
    "  xorq %r9, %r9\n"
    "  movl $9, %eax\n"
    "  syscall\n"
    "  popq %r8\n"
    "  cmpq $-4096, %rax\n"
    "  ja .Lmal_fail\n"
    "  movq %r8, 0(%rax)\n"
    "  addq $16, %rax\n"
    "  ret\n"
    ".Lmal_fail:\n"
    "  xorq %rax, %rax\n"
    "  ret\n"
    "calloc:\n"
    "  imulq %rsi, %rdi\n"
    "  pushq %rdi\n"
    "  call malloc\n"
    "  popq %rcx\n"
    "  testq %rax, %rax\n"
    "  je .Lcal_ret\n"
    "  movq %rax, %r8\n"
    "  movq %rax, %rdi\n"
    "  xorl %eax, %eax\n"
    "  rep stosb\n"
    "  movq %r8, %rax\n"
    ".Lcal_ret:\n"
    "  ret\n"
    "realloc:\n"
    "  testq %rdi, %rdi\n"
    "  jne .Lrea_nz\n"
    "  movq %rsi, %rdi\n"
    "  jmp malloc\n"
    ".Lrea_nz:\n"
    "  movq -16(%rdi), %rdx\n"
    "  cmpq %rdx, %rsi\n"
    "  ja .Lrea_grow\n"
    "  movq %rsi, -16(%rdi)\n"
    "  movq %rdi, %rax\n"
    "  ret\n"
    ".Lrea_grow:\n"
    "  pushq %rdi\n"
    "  pushq %rdx\n"
    "  movq %rsi, %rdi\n"
    "  call malloc\n"
    "  popq %rcx\n"
    "  popq %rsi\n"
    "  testq %rax, %rax\n"
    "  je .Lrea_ret\n"
    "  movq %rax, %rdi\n"
    "  movq %rax, %r8\n"
    "  rep movsb\n"
    "  movq %r8, %rax\n"
    ".Lrea_ret:\n"
    "  ret\n"
    "free:\n"
    "  ret\n";

static const char *rt_asm_printf_0 =
    ".text\n"
    ".globl printf\n"
    ".globl fprintf\n"
    ".globl sprintf\n"
    ".globl snprintf\n"
    ".globl vprintf\n"
    ".globl vfprintf\n"
    ".globl vsprintf\n"
    ".globl vsnprintf\n"
    "vprintf:\n"
    "  movq %rsi, %r8\n"
    "  movq %rdi, %rcx\n"
    "  movq $1, %rdi\n"
    "  xorq %rsi, %rsi\n"
    "  xorq %rdx, %rdx\n"
    "  jmp .Lpf_core\n"
    "vfprintf:\n"
    "  movq %rdx, %r8\n"
    "  movq %rsi, %rcx\n"
    "  cmpq $256, %rdi\n"
    "  jb .Lpf_vfp_fd\n"
    "  subq $256, %rdi\n"
    ".Lpf_vfp_fd:\n"
    "  xorq %rsi, %rsi\n"
    "  xorq %rdx, %rdx\n"
    "  jmp .Lpf_core\n"
    "vsprintf:\n"
    "  movq %rdx, %r8\n"
    "  movq %rsi, %rcx\n"
    "  movq %rdi, %rsi\n"
    "  movq $-1, %rdi\n"
    "  movl $2147483647, %edx\n"
    "  jmp .Lpf_core\n"
    "vsnprintf:\n"
    "  movq %rcx, %r8\n"
    "  movq %rdx, %rcx\n"
    "  movq %rsi, %rdx\n"
    "  movq %rdi, %rsi\n"
    "  movq $-1, %rdi\n"
    "  jmp .Lpf_core\n"
    "printf:\n"
    "  xorl %r10d, %r10d\n"
    "  jmp .Lpf_var_save\n"
    "fprintf:\n"
    "  movl $1, %r10d\n"
    "  jmp .Lpf_var_save\n"
    "sprintf:\n"
    "  movl $2, %r10d\n"
    "  jmp .Lpf_var_save\n"
    "snprintf:\n"
    "  movl $3, %r10d\n"
    ".Lpf_var_save:\n"
    "  subq $216, %rsp\n"
    "  movq %rdi, 0(%rsp)\n"
    "  movq %rsi, 8(%rsp)\n"
    "  movq %rdx, 16(%rsp)\n"
    "  movq %rcx, 24(%rsp)\n"
    "  movq %r8, 32(%rsp)\n"
    "  movq %r9, 40(%rsp)\n"
    "  movsd %xmm0, 48(%rsp)\n"
    "  movsd %xmm1, 64(%rsp)\n"
    "  movsd %xmm2, 80(%rsp)\n"
    "  movsd %xmm3, 96(%rsp)\n"
    "  movsd %xmm4, 112(%rsp)\n"
    "  movsd %xmm5, 128(%rsp)\n"
    "  movsd %xmm6, 144(%rsp)\n"
    "  movsd %xmm7, 160(%rsp)\n"
    "  movl $48, %eax\n"
    "  movl %eax, 180(%rsp)\n"
    "  leaq 224(%rsp), %rax\n"
    "  movq %rax, 184(%rsp)\n"
    "  movq %rsp, %rax\n"
    "  movq %rax, 192(%rsp)\n"
    "  leaq 176(%rsp), %r8\n"
    "  cmpq $0, %r10\n"
    "  jne .Lpf_vs_1\n"
    "  movl $8, %eax\n"
    "  movl %eax, 176(%rsp)\n"
    "  movq $1, %rdi\n"
    "  xorq %rsi, %rsi\n"
    "  xorq %rdx, %rdx\n"
    "  movq 0(%rsp), %rcx\n"
    "  jmp .Lpf_vs_call\n"
    ".Lpf_vs_1:\n"
    "  cmpq $1, %r10\n"
    "  jne .Lpf_vs_2\n"
    "  movl $16, %eax\n"
    "  movl %eax, 176(%rsp)\n"
    "  movq 0(%rsp), %rdi\n"
    "  cmpq $256, %rdi\n"
    "  jb .Lpf_vs1_fd\n"
    "  subq $256, %rdi\n"
    ".Lpf_vs1_fd:\n"
    "  xorq %rsi, %rsi\n"
    "  xorq %rdx, %rdx\n"
    "  movq 8(%rsp), %rcx\n"
    "  jmp .Lpf_vs_call\n"
    ".Lpf_vs_2:\n"
    "  cmpq $2, %r10\n"
    "  jne .Lpf_vs_3\n"
    "  movl $16, %eax\n"
    "  movl %eax, 176(%rsp)\n"
    "  movq $-1, %rdi\n"
    "  movq 0(%rsp), %rsi\n"
    "  movl $2147483647, %edx\n"
    "  movq 8(%rsp), %rcx\n"
    "  jmp .Lpf_vs_call\n"
    ".Lpf_vs_3:\n"
    "  movl $24, %eax\n"
    "  movl %eax, 176(%rsp)\n"
    "  movq $-1, %rdi\n"
    "  movq 0(%rsp), %rsi\n"
    "  movq 8(%rsp), %rdx\n"
    "  movq 16(%rsp), %rcx\n"
    ".Lpf_vs_call:\n"
    "  call .Lpf_core\n"
    "  addq $216, %rsp\n"
    "  ret\n";

static const char *rt_asm_printf_1 =
    ".Lpf_core:\n"
    "  pushq %rbp\n"
    "  pushq %rbx\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  subq $480, %rsp\n"
    "  movq %rsp, %rbp\n"
    "  movq %rdi, 440(%rbp)\n"
    "  movq %rsi, 448(%rbp)\n"
    "  movq %rdx, 472(%rbp)\n"
    "  cmpq $0, %rdx\n"
    "  jle .Lpf_cap0\n"
    "  decq %rdx\n"
    "  jmp .Lpf_capst\n"
    ".Lpf_cap0:\n"
    "  xorq %rdx, %rdx\n"
    ".Lpf_capst:\n"
    "  movq %rdx, 456(%rbp)\n"
    "  movq %r8, 464(%rbp)\n"
    "  movq %rcx, %r12\n"
    "  xorq %rbx, %rbx\n"
    "  leaq 256(%rbp), %r14\n"
    "  leaq 384(%rbp), %r15\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_flush:\n"
    "  movq 440(%rbp), %rdi\n"
    "  cmpq $0, %rdi\n"
    "  jl .Lpf_fl_dn\n"
    "  leaq 256(%rbp), %rsi\n"
    "  movq %r14, %rdx\n"
    "  subq %rsi, %rdx\n"
    "  cmpq $0, %rdx\n"
    "  jle .Lpf_fl_dn\n"
    "  movl $1, %eax\n"
    "  syscall\n"
    "  leaq 256(%rbp), %r14\n"
    ".Lpf_fl_dn:\n"
    "  ret\n"
    ".Lpf_putc:\n"
    "  incq %rbx\n"
    "  movq 440(%rbp), %rdx\n"
    "  cmpq $0, %rdx\n"
    "  jge .Lpf_pc_stream\n"
    "  movq 456(%rbp), %rcx\n"
    "  cmpq $0, %rcx\n"
    "  jle .Lpf_pc_dn\n"
    "  decq %rcx\n"
    "  movq %rcx, 456(%rbp)\n"
    "  movq 448(%rbp), %rdx\n"
    "  movb %al, (%rdx)\n"
    "  incq %rdx\n"
    "  movq %rdx, 448(%rbp)\n"
    "  ret\n"
    ".Lpf_pc_stream:\n"
    "  movb %al, (%r14)\n"
    "  incq %r14\n"
    "  cmpq %r15, %r14\n"
    "  jb .Lpf_pc_dn\n"
    "  jmp .Lpf_flush\n"
    ".Lpf_pc_dn:\n"
    "  ret\n"
    ".Lpf_next_gp:\n"
    "  movq 464(%rbp), %rcx\n"
    "  movl 0(%rcx), %eax\n"
    "  cmpq $48, %rax\n"
    "  jae .Lpf_ngp_ovf\n"
    "  movq 16(%rcx), %rdx\n"
    "  addq %rax, %rdx\n"
    "  addq $8, %rax\n"
    "  movl %eax, 0(%rcx)\n"
    "  movq (%rdx), %rax\n"
    "  ret\n"
    ".Lpf_ngp_ovf:\n"
    "  movq 8(%rcx), %rdx\n"
    "  movq (%rdx), %rax\n"
    "  addq $8, %rdx\n"
    "  movq %rdx, 8(%rcx)\n"
    "  ret\n"
    ".Lpf_next_xmm:\n"
    "  movq 464(%rbp), %rcx\n"
    "  movl 4(%rcx), %eax\n"
    "  cmpq $176, %rax\n"
    "  jae .Lpf_nxm_ovf\n"
    "  movq 16(%rcx), %rdx\n"
    "  addq %rax, %rdx\n"
    "  addq $16, %rax\n"
    "  movl %eax, 4(%rcx)\n"
    "  movsd (%rdx), %xmm0\n"
    "  ret\n"
    ".Lpf_nxm_ovf:\n"
    "  movq 8(%rcx), %rdx\n"
    "  movsd (%rdx), %xmm0\n"
    "  addq $8, %rdx\n"
    "  movq %rdx, 8(%rcx)\n"
    "  ret\n"
    ".Lpf_fmt_uint:\n"
    "  leaq 255(%rbp), %r13\n"
    "  xorq %r8, %r8\n"
    ".Lpf_fu_div:\n"
    "  xorq %rdx, %rdx\n"
    "  divq %r9\n"
    "  cmpq $10, %rdx\n"
    "  jb .Lpf_fu_lt10\n"
    "  addq %r10, %rdx\n"
    "  jmp .Lpf_fu_next\n"
    ".Lpf_fu_lt10:\n"
    "  addq $48, %rdx\n"
    ".Lpf_fu_next:\n"
    "  decq %r13\n"
    "  movb %dl, (%r13)\n"
    "  incq %r8\n"
    "  testq %rax, %rax\n"
    "  jne .Lpf_fu_div\n"
    "  movq 392(%rbp), %r9\n"
    "  subq %r8, %r9\n"
    "  movq 416(%rbp), %rdx\n"
    "  subq %rdx, %r9\n"
    "  movq 384(%rbp), %rax\n"
    "  cmpq $48, %rax\n"
    "  je .Lpf_fu_zeropad\n"
    ".Lpf_fu_sp_lp:\n"
    "  cmpq $0, %r9\n"
    "  jle .Lpf_fu_sign\n"
    "  movl $32, %eax\n"
    "  call .Lpf_putc\n"
    "  decq %r9\n"
    "  jmp .Lpf_fu_sp_lp\n"
    ".Lpf_fu_zeropad:\n"
    "  movq 416(%rbp), %rdx\n"
    "  cmpq $0, %rdx\n"
    "  je .Lpf_fu_zp_lp\n"
    "  movl $45, %eax\n"
    "  call .Lpf_putc\n"
    "  movq $0, 416(%rbp)\n"
    ".Lpf_fu_zp_lp:\n"
    "  cmpq $0, %r9\n"
    "  jle .Lpf_fu_emit\n"
    "  movl $48, %eax\n"
    "  call .Lpf_putc\n"
    "  decq %r9\n"
    "  jmp .Lpf_fu_zp_lp\n"
    ".Lpf_fu_sign:\n"
    "  movq 416(%rbp), %rdx\n"
    "  cmpq $0, %rdx\n"
    "  je .Lpf_fu_emit\n"
    "  movl $45, %eax\n"
    "  call .Lpf_putc\n"
    ".Lpf_fu_emit:\n"
    ".Lpf_fu_em_lp:\n"
    "  cmpq $0, %r8\n"
    "  jle .Lpf_fu_done\n"
    "  movzbq (%r13), %rax\n"
    "  call .Lpf_putc\n"
    "  incq %r13\n"
    "  decq %r8\n"
    "  jmp .Lpf_fu_em_lp\n"
    ".Lpf_fu_done:\n"
    "  ret\n";

static const char *rt_asm_printf_2 =
    ".Lpf_loop:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $0, %rax\n"
    "  je .Lpf_done\n"
    "  incq %r12\n"
    "  cmpq $37, %rax\n"
    "  je .Lpf_pct\n"
    "  call .Lpf_putc\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_pct:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $0, %rax\n"
    "  je .Lpf_done\n"
    "  cmpq $37, %rax\n"
    "  jne .Lpf_spec_init\n"
    "  incq %r12\n"
    "  call .Lpf_putc\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_spec_init:\n"
    "  movq $32, 384(%rbp)\n"
    "  movq $0, 392(%rbp)\n"
    "  movq $-1, 400(%rbp)\n"
    "  movq $0, 408(%rbp)\n"
    "  movq $0, 416(%rbp)\n"
    ".Lpf_flags:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $48, %rax\n"
    "  jne .Lpf_fl1\n"
    "  movq $48, 384(%rbp)\n"
    "  incq %r12\n"
    "  jmp .Lpf_flags\n"
    ".Lpf_fl1:\n"
    "  cmpq $45, %rax\n"
    "  je .Lpf_fl_skip\n"
    "  cmpq $43, %rax\n"
    "  je .Lpf_fl_skip\n"
    "  cmpq $32, %rax\n"
    "  je .Lpf_fl_skip\n"
    "  cmpq $35, %rax\n"
    "  je .Lpf_fl_skip\n"
    "  jmp .Lpf_width\n"
    ".Lpf_fl_skip:\n"
    "  incq %r12\n"
    "  jmp .Lpf_flags\n"
    ".Lpf_width:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $48, %rax\n"
    "  jb .Lpf_prec_chk\n"
    "  cmpq $57, %rax\n"
    "  ja .Lpf_prec_chk\n"
    "  subq $48, %rax\n"
    "  movq 392(%rbp), %rcx\n"
    "  imulq $10, %rcx\n"
    "  addq %rax, %rcx\n"
    "  movq %rcx, 392(%rbp)\n"
    "  incq %r12\n"
    "  jmp .Lpf_width\n"
    ".Lpf_prec_chk:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $46, %rax\n"
    "  jne .Lpf_len\n"
    "  incq %r12\n"
    "  movq $0, 400(%rbp)\n"
    ".Lpf_prec_lp:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $48, %rax\n"
    "  jb .Lpf_len\n"
    "  cmpq $57, %rax\n"
    "  ja .Lpf_len\n"
    "  subq $48, %rax\n"
    "  movq 400(%rbp), %rcx\n"
    "  imulq $10, %rcx\n"
    "  addq %rax, %rcx\n"
    "  movq %rcx, 400(%rbp)\n"
    "  incq %r12\n"
    "  jmp .Lpf_prec_lp\n"
    ".Lpf_len:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $108, %rax\n"
    "  je .Lpf_len_long\n"
    "  cmpq $122, %rax\n"
    "  je .Lpf_len_long\n"
    "  cmpq $104, %rax\n"
    "  je .Lpf_len_skip\n"
    "  jmp .Lpf_conv\n"
    ".Lpf_len_long:\n"
    "  movq $1, 408(%rbp)\n"
    ".Lpf_len_skip:\n"
    "  incq %r12\n"
    "  jmp .Lpf_len\n"
    ".Lpf_conv:\n"
    "  movzbq (%r12), %rax\n"
    "  cmpq $0, %rax\n"
    "  je .Lpf_done\n"
    "  incq %r12\n"
    "  cmpq $99, %rax\n"
    "  je .Lpf_c\n"
    "  cmpq $115, %rax\n"
    "  je .Lpf_s\n"
    "  cmpq $100, %rax\n"
    "  je .Lpf_d\n"
    "  cmpq $105, %rax\n"
    "  je .Lpf_d\n"
    "  cmpq $117, %rax\n"
    "  je .Lpf_u\n"
    "  cmpq $120, %rax\n"
    "  je .Lpf_x\n"
    "  cmpq $88, %rax\n"
    "  je .Lpf_X\n"
    "  cmpq $112, %rax\n"
    "  je .Lpf_p\n"
    "  cmpq $102, %rax\n"
    "  je .Lpf_f\n"
    "  cmpq $70, %rax\n"
    "  je .Lpf_f\n"
    "  cmpq $101, %rax\n"
    "  je .Lpf_f\n"
    "  cmpq $69, %rax\n"
    "  je .Lpf_f\n"
    "  cmpq $103, %rax\n"
    "  je .Lpf_f\n"
    "  cmpq $71, %rax\n"
    "  je .Lpf_f\n"
    "  call .Lpf_putc\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_c:\n"
    "  call .Lpf_next_gp\n"
    "  call .Lpf_putc\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_s:\n"
    "  call .Lpf_next_gp\n"
    "  testq %rax, %rax\n"
    "  jne .Lpf_s_ok\n"
    "  movl $40, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $110, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $117, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $108, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $108, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $41, %eax\n"
    "  call .Lpf_putc\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_s_ok:\n"
    "  movq %rax, %r13\n"
    ".Lpf_s_lp:\n"
    "  movzbq (%r13), %rax\n"
    "  cmpq $0, %rax\n"
    "  je .Lpf_loop\n"
    "  call .Lpf_putc\n"
    "  incq %r13\n"
    "  jmp .Lpf_s_lp\n"
    ".Lpf_d:\n"
    "  call .Lpf_next_gp\n"
    "  movq 408(%rbp), %rdx\n"
    "  cmpq $0, %rdx\n"
    "  jne .Lpf_d64\n"
    "  cltq\n"
    ".Lpf_d64:\n"
    "  testq %rax, %rax\n"
    "  jns .Lpf_dpos\n"
    "  negq %rax\n"
    "  movq $1, 416(%rbp)\n"
    ".Lpf_dpos:\n"
    "  movq $10, %r9\n"
    "  call .Lpf_fmt_uint\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_u:\n"
    "  call .Lpf_next_gp\n"
    "  movq 408(%rbp), %rdx\n"
    "  cmpq $0, %rdx\n"
    "  jne .Lpf_u64\n"
    "  movl %eax, %eax\n"
    ".Lpf_u64:\n"
    "  movq $10, %r9\n"
    "  call .Lpf_fmt_uint\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_x:\n"
    "  call .Lpf_next_gp\n"
    "  movq 408(%rbp), %rdx\n"
    "  cmpq $0, %rdx\n"
    "  jne .Lpf_x64\n"
    "  movl %eax, %eax\n"
    ".Lpf_x64:\n"
    "  movq $16, %r9\n"
    "  movq $87, %r10\n"
    "  call .Lpf_fmt_uint\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_X:\n"
    "  call .Lpf_next_gp\n"
    "  movq 408(%rbp), %rdx\n"
    "  cmpq $0, %rdx\n"
    "  jne .Lpf_X64\n"
    "  movl %eax, %eax\n"
    ".Lpf_X64:\n"
    "  movq $16, %r9\n"
    "  movq $55, %r10\n"
    "  call .Lpf_fmt_uint\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_p:\n"
    "  call .Lpf_next_gp\n"
    "  testq %rax, %rax\n"
    "  jne .Lpf_p_nz\n"
    "  movl $40, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $110, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $105, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $108, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $41, %eax\n"
    "  call .Lpf_putc\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_p_nz:\n"
    "  movq %rax, %r13\n"
    "  movl $48, %eax\n"
    "  call .Lpf_putc\n"
    "  movl $120, %eax\n"
    "  call .Lpf_putc\n"
    "  movq %r13, %rax\n"
    "  movq $16, %r9\n"
    "  movq $87, %r10\n"
    "  call .Lpf_fmt_uint\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_f:\n"
    "  call .Lpf_next_xmm\n"
    "  movq %xmm0, %rax\n"
    "  testq %rax, %rax\n"
    "  jns .Lpf_f_pos\n"
    "  movl $45, %eax\n"
    "  call .Lpf_putc\n"
    "  pxor %xmm1, %xmm1\n"
    "  subsd %xmm0, %xmm1\n"
    "  movsd %xmm1, %xmm0\n"
    ".Lpf_f_pos:\n"
    "  movq 400(%rbp), %rax\n"
    "  cmpq $0, %rax\n"
    "  jge .Lpf_f_pn\n"
    "  movq $6, %rax\n"
    ".Lpf_f_pn:\n"
    "  cmpq $9, %rax\n"
    "  jle .Lpf_f_pm\n"
    "  movq $9, %rax\n"
    ".Lpf_f_pm:\n"
    "  movq %rax, 432(%rbp)\n"
    "  movq $1, %r9\n"
    "  movq %rax, %rcx\n"
    ".Lpf_f_pw:\n"
    "  cmpq $0, %rcx\n"
    "  jle .Lpf_f_pw_dn\n"
    "  imulq $10, %r9\n"
    "  decq %rcx\n"
    "  jmp .Lpf_f_pw\n"
    ".Lpf_f_pw_dn:\n"
    "  cvttsd2siq %xmm0, %r8\n"
    "  cvtsi2sdq %r8, %xmm1\n"
    "  subsd %xmm1, %xmm0\n"
    "  cvtsi2sdq %r9, %xmm2\n"
    "  mulsd %xmm2, %xmm0\n"
    "  movabsq $4602678819172646912, %rax\n"
    "  movq %rax, %xmm1\n"
    "  addsd %xmm1, %xmm0\n"
    "  cvttsd2siq %xmm0, %r10\n"
    "  cmpq %r9, %r10\n"
    "  jb .Lpf_f_nc\n"
    "  subq %r9, %r10\n"
    "  incq %r8\n"
    ".Lpf_f_nc:\n"
    "  movq %r10, 424(%rbp)\n"
    "  movq $0, 392(%rbp)\n"
    "  movq $32, 384(%rbp)\n"
    "  movq $0, 416(%rbp)\n"
    "  movq %r8, %rax\n"
    "  movq $10, %r9\n"
    "  call .Lpf_fmt_uint\n"
    "  movq 432(%rbp), %rcx\n"
    "  cmpq $0, %rcx\n"
    "  jle .Lpf_loop\n"
    "  movl $46, %eax\n"
    "  call .Lpf_putc\n"
    "  movq 432(%rbp), %rcx\n"
    "  movq %rcx, 392(%rbp)\n"
    "  movq $48, 384(%rbp)\n"
    "  movq $0, 416(%rbp)\n"
    "  movq 424(%rbp), %rax\n"
    "  movq $10, %r9\n"
    "  call .Lpf_fmt_uint\n"
    "  jmp .Lpf_loop\n"
    ".Lpf_done:\n"
    "  movq 440(%rbp), %rdi\n"
    "  cmpq $0, %rdi\n"
    "  jl .Lpf_done_mem\n"
    "  call .Lpf_flush\n"
    "  jmp .Lpf_done_ret\n"
    ".Lpf_done_mem:\n"
    "  movq 472(%rbp), %rcx\n"
    "  cmpq $0, %rcx\n"
    "  jle .Lpf_done_ret\n"
    "  movq 448(%rbp), %rdx\n"
    "  xorl %eax, %eax\n"
    "  movb %al, (%rdx)\n"
    ".Lpf_done_ret:\n"
    "  movq %rbx, %rax\n"
    "  addq $480, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbx\n"
    "  popq %rbp\n"
    "  ret\n";

static const char *rt_asm_stdio =
    ".data\n"
    ".align 8\n"
    ".globl stdin\n"
    "stdin:\n"
    "  .quad 256\n"
    ".globl stdout\n"
    "stdout:\n"
    "  .quad 257\n"
    ".globl stderr\n"
    "stderr:\n"
    "  .quad 258\n"
    ".text\n"
    ".globl fopen\n"
    ".globl fclose\n"
    ".globl fread\n"
    ".globl fwrite\n"
    ".globl fputs\n"
    ".globl fputc\n"
    ".globl fflush\n"
    ".globl fseek\n"
    ".globl ftell\n"
    ".globl remove\n"
    "fopen:\n"
    "  testq %rdi, %rdi\n"
    "  je .Lfo_null\n"
    "  testq %rsi, %rsi\n"
    "  je .Lfo_null\n"
    "  movzbq 0(%rsi), %rax\n"
    "  movl $0, %edx\n"
    "  cmpq $114, %rax\n"
    "  je .Lfo_chk_p\n"
    "  movl $577, %edx\n"
    "  cmpq $119, %rax\n"
    "  je .Lfo_chk_p\n"
    "  movl $1089, %edx\n"
    "  cmpq $97, %rax\n"
    "  je .Lfo_chk_p\n"
    ".Lfo_null:\n"
    "  xorq %rax, %rax\n"
    "  ret\n"
    ".Lfo_chk_p:\n"
    "  incq %rsi\n"
    "  movzbq 0(%rsi), %rcx\n"
    "  cmpq $0, %rcx\n"
    "  je .Lfo_open\n"
    "  cmpq $43, %rcx\n"
    "  jne .Lfo_chk_p\n"
    "  cmpq $114, %rax\n"
    "  jne .Lfo_wrp\n"
    "  movl $2, %edx\n"
    "  jmp .Lfo_open\n"
    ".Lfo_wrp:\n"
    "  addl $1, %edx\n"
    ".Lfo_open:\n"
    "  movq %rdx, %rsi\n"
    "  movl $420, %edx\n"
    "  movl $2, %eax\n"
    "  syscall\n"
    "  testq %rax, %rax\n"
    "  js .Lfo_null\n"
    "  addq $256, %rax\n"
    "  ret\n"
    "fclose:\n"
    "  testq %rdi, %rdi\n"
    "  jne .Lfc_ok\n"
    "  movl $-1, %eax\n"
    "  ret\n"
    ".Lfc_ok:\n"
    "  cmpq $256, %rdi\n"
    "  jb .Lfc_fd\n"
    "  subq $256, %rdi\n"
    ".Lfc_fd:\n"
    "  movl $3, %eax\n"
    "  syscall\n"
    "  ret\n"
    "fread:\n"
    "  xorl %r10d, %r10d\n"
    "  jmp .Lfio_rw\n"
    "fwrite:\n"
    "  movl $1, %r10d\n"
    ".Lfio_rw:\n"
    "  testq %rcx, %rcx\n"
    "  je .Lfio_zero\n"
    "  testq %rsi, %rsi\n"
    "  je .Lfio_zero\n"
    "  testq %rdx, %rdx\n"
    "  je .Lfio_zero\n"
    "  pushq %rbx\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  movq %rdi, %rbx\n"
    "  movq %rsi, %r12\n"
    "  imulq %rdx, %rsi\n"
    "  movq %rsi, %r13\n"
    "  movq %rcx, %r14\n"
    "  cmpq $256, %r14\n"
    "  jb .Lfio_fd\n"
    "  subq $256, %r14\n"
    ".Lfio_fd:\n"
    "  xorq %r15, %r15\n"
    "  movq %r10, %rcx\n"
    "  pushq %rcx\n"
    ".Lfio_lp:\n"
    "  cmpq $0, %r13\n"
    "  jle .Lfio_dn\n"
    "  movq %r14, %rdi\n"
    "  movq %rbx, %rsi\n"
    "  movq %r13, %rdx\n"
    "  movq 0(%rsp), %rax\n"
    "  syscall\n"
    "  cmpq $0, %rax\n"
    "  jle .Lfio_dn\n"
    "  addq %rax, %rbx\n"
    "  addq %rax, %r15\n"
    "  subq %rax, %r13\n"
    "  jmp .Lfio_lp\n"
    ".Lfio_dn:\n"
    "  popq %rcx\n"
    "  movq %r15, %rax\n"
    "  xorq %rdx, %rdx\n"
    "  divq %r12\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbx\n"
    "  ret\n"
    ".Lfio_zero:\n"
    "  xorq %rax, %rax\n"
    "  ret\n"
    "fputs:\n"
    "  testq %rdi, %rdi\n"
    "  je .Lfp_err\n"
    "  testq %rsi, %rsi\n"
    "  je .Lfp_err\n"
    "  movq %rdi, %r8\n"
    "  xorq %rdx, %rdx\n"
    ".Lfp_len:\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpb $0, %al\n"
    "  je .Lfp_wr\n"
    "  incq %rdi\n"
    "  incq %rdx\n"
    "  jmp .Lfp_len\n"
    ".Lfp_wr:\n"
    "  movq %rsi, %rdi\n"
    "  cmpq $256, %rdi\n"
    "  jb .Lfp_fd\n"
    "  subq $256, %rdi\n"
    ".Lfp_fd:\n"
    "  movq %r8, %rsi\n"
    "  movl $1, %eax\n"
    "  syscall\n"
    "  movl $1, %eax\n"
    "  ret\n"
    ".Lfp_err:\n"
    "  movl $-1, %eax\n"
    "  ret\n"
    "fputc:\n"
    "  testq %rsi, %rsi\n"
    "  je .Lfp_err\n"
    "  subq $16, %rsp\n"
    "  movl %edi, %r8d\n"
    "  movq %rsp, %rcx\n"
    "  movb %dil, (%rcx)\n"
    "  movq %rsi, %rdi\n"
    "  cmpq $256, %rdi\n"
    "  jb .Lfpc_fd\n"
    "  subq $256, %rdi\n"
    ".Lfpc_fd:\n"
    "  movq %rcx, %rsi\n"
    "  movl $1, %edx\n"
    "  movl $1, %eax\n"
    "  syscall\n"
    "  addq $16, %rsp\n"
    "  movl %r8d, %eax\n"
    "  andl $255, %eax\n"
    "  ret\n"
    "fflush:\n"
    "  xorl %eax, %eax\n"
    "  ret\n"
    "fseek:\n"
    "  testq %rdi, %rdi\n"
    "  je .Lfp_err\n"
    "  cmpq $256, %rdi\n"
    "  jb .Lfsk_fd\n"
    "  subq $256, %rdi\n"
    ".Lfsk_fd:\n"
    "  movl $8, %eax\n"
    "  syscall\n"
    "  testq %rax, %rax\n"
    "  js .Lfp_err\n"
    "  xorl %eax, %eax\n"
    "  ret\n"
    "ftell:\n"
    "  testq %rdi, %rdi\n"
    "  je .Lfp_err\n"
    "  cmpq $256, %rdi\n"
    "  jb .Lft_fd\n"
    "  subq $256, %rdi\n"
    ".Lft_fd:\n"
    "  xorq %rsi, %rsi\n"
    "  movl $1, %edx\n"
    "  movl $8, %eax\n"
    "  syscall\n"
    "  ret\n"
    "remove:\n"
    "  movl $87, %eax\n"
    "  syscall\n"
    "  ret\n";

static const char *rt_asm_math_1 =
    ".data\n"
    ".align 8\n"
    ".globl __zcc_rand_seed\n"
    "__zcc_rand_seed:\n"
    "  .quad 1\n"
    ".text\n"
    ".globl srand\n"
    ".globl rand\n"
    ".globl fminf\n"
    ".globl fmaxf\n"
    ".globl fmin\n"
    ".globl fmax\n"
    ".globl fabsf\n"
    ".globl fabs\n"
    ".globl sqrtf\n"
    ".globl sqrt\n"
    ".globl floor\n"
    ".globl floorf\n"
    ".globl fmodf\n"
    "srand:\n"
    "  movl %edi, %eax\n"
    "  movq %rax, __zcc_rand_seed(%rip)\n"
    "  ret\n"
    "rand:\n"
    "  movq __zcc_rand_seed(%rip), %rax\n"
    "  movabsq $6364136223846793005, %rcx\n"
    "  imulq %rcx, %rax\n"
    "  movabsq $1442695040888963407, %rcx\n"
    "  addq %rcx, %rax\n"
    "  movq %rax, __zcc_rand_seed(%rip)\n"
    "  shrq $33, %rax\n"
    "  andl $2147483647, %eax\n"
    "  ret\n"
    "fminf:\n"
    "  ucomiss %xmm0, %xmm1\n"
    "  jae .Lmfmn_r\n"
    "  movss %xmm1, %xmm0\n"
    ".Lmfmn_r:\n"
    "  ret\n"
    "fmaxf:\n"
    "  ucomiss %xmm0, %xmm1\n"
    "  jbe .Lmfmx_r\n"
    "  movss %xmm1, %xmm0\n"
    ".Lmfmx_r:\n"
    "  ret\n"
    "fmin:\n"
    "  ucomisd %xmm0, %xmm1\n"
    "  jae .Lmdmn_r\n"
    "  movsd %xmm1, %xmm0\n"
    ".Lmdmn_r:\n"
    "  ret\n"
    "fmax:\n"
    "  ucomisd %xmm0, %xmm1\n"
    "  jbe .Lmdmx_r\n"
    "  movsd %xmm1, %xmm0\n"
    ".Lmdmx_r:\n"
    "  ret\n"
    "fabsf:\n"
    "  movq %xmm0, %rax\n"
    "  movl $2147483647, %ecx\n"
    "  andq %rcx, %rax\n"
    "  movq %rax, %xmm0\n"
    "  ret\n"
    "fabs:\n"
    "  movq %xmm0, %rax\n"
    "  movabsq $9223372036854775807, %rcx\n"
    "  andq %rcx, %rax\n"
    "  movq %rax, %xmm0\n"
    "  ret\n"
    "sqrtf:\n"
    "  sqrtss %xmm0, %xmm0\n"
    "  ret\n"
    "sqrt:\n"
    "  sqrtsd %xmm0, %xmm0\n"
    "  ret\n"
    "floor:\n"
    "  movq %xmm0, %rax\n"
    "  movabsq $9223372036854775807, %rcx\n"
    "  andq %rcx, %rax\n"
    "  movabsq $4841369599423283200, %rcx\n"
    "  cmpq %rcx, %rax\n"
    "  jae .Lmfl_ret\n"
    "  cvttsd2siq %xmm0, %rax\n"
    "  cvtsi2sdq %rax, %xmm1\n"
    "  ucomisd %xmm0, %xmm1\n"
    "  jbe .Lmfl_ok\n"
    "  decq %rax\n"
    "  cvtsi2sdq %rax, %xmm1\n"
    ".Lmfl_ok:\n"
    "  movsd %xmm1, %xmm0\n"
    ".Lmfl_ret:\n"
    "  ret\n"
    "floorf:\n"
    "  cvtss2sd %xmm0, %xmm0\n"
    "  call floor\n"
    "  cvtsd2ss %xmm0, %xmm0\n"
    "  ret\n"
    "fmodf:\n"
    "  cvtss2sd %xmm0, %xmm0\n"
    "  cvtss2sd %xmm1, %xmm1\n"
    "  pxor %xmm2, %xmm2\n"
    "  ucomisd %xmm2, %xmm1\n"
    "  je .Lmfm_z\n"
    "  movsd %xmm0, %xmm2\n"
    "  divsd %xmm1, %xmm2\n"
    "  movq %xmm2, %rax\n"
    "  movabsq $9223372036854775807, %rcx\n"
    "  andq %rcx, %rax\n"
    "  movabsq $4841369599423283200, %rcx\n"
    "  cmpq %rcx, %rax\n"
    "  jae .Lmfm_z\n"
    "  cvttsd2siq %xmm2, %rax\n"
    "  cvtsi2sdq %rax, %xmm2\n"
    "  mulsd %xmm1, %xmm2\n"
    "  subsd %xmm2, %xmm0\n"
    "  cvtsd2ss %xmm0, %xmm0\n"
    "  ret\n"
    ".Lmfm_z:\n"
    "  pxor %xmm0, %xmm0\n"
    "  cvtsd2ss %xmm0, %xmm0\n"
    "  ret\n";

static const char *rt_asm_math_2 =
    ".text\n"
    ".globl sinf\n"
    ".globl cosf\n"
    ".globl tanf\n"
    ".globl expf\n"
    ".Lrt_sin_d:\n"
    "  movabsq $4599405781057128579, %rax\n"
    "  movq %rax, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movq %xmm1, %rax\n"
    "  testq %rax, %rax\n"
    "  js .Lsin_neg\n"
    "  movabsq $4602678819172646912, %rax\n"
    "  jmp .Lsin_rnd\n"
    ".Lsin_neg:\n"
    "  movabsq $13826050856027422720, %rax\n"
    ".Lsin_rnd:\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  cvttsd2siq %xmm1, %r8\n"
    "  cvtsi2sdq %r8, %xmm2\n"
    "  movabsq $4614256656552045848, %rax\n"
    "  movq %rax, %xmm3\n"
    "  mulsd %xmm3, %xmm2\n"
    "  subsd %xmm2, %xmm0\n"
    "  movsd %xmm0, %xmm4\n"
    "  mulsd %xmm0, %xmm4\n"
    "  movabsq $4460272573143870729, %rax\n"
    "  movq %rax, %xmm1\n"
    "  mulsd %xmm4, %xmm1\n"
    "  movabsq $13716528800881525988, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm4, %xmm1\n"
    "  movabsq $4523617214285662004, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm4, %xmm1\n"
    "  movabsq $13774824197408792602, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm4, %xmm1\n"
    "  movabsq $4575957461383581969, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm4, %xmm1\n"
    "  movabsq $13818544856648471893, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm4, %xmm1\n"
    "  movabsq $4607182418800017408, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  andq $1, %r8\n"
    "  je .Lsin_pos\n"
    "  pxor %xmm0, %xmm0\n"
    "  subsd %xmm1, %xmm0\n"
    "  ret\n"
    ".Lsin_pos:\n"
    "  movsd %xmm1, %xmm0\n"
    "  ret\n"
    "sinf:\n"
    "  cvtss2sd %xmm0, %xmm0\n"
    "  call .Lrt_sin_d\n"
    "  cvtsd2ss %xmm0, %xmm0\n"
    "  ret\n"
    "cosf:\n"
    "  cvtss2sd %xmm0, %xmm0\n"
    "  movabsq $4609753056924675352, %rax\n"
    "  movq %rax, %xmm1\n"
    "  addsd %xmm1, %xmm0\n"
    "  call .Lrt_sin_d\n"
    "  cvtsd2ss %xmm0, %xmm0\n"
    "  ret\n"
    "tanf:\n"
    "  subq $24, %rsp\n"
    "  cvtss2sd %xmm0, %xmm0\n"
    "  movsd %xmm0, 0(%rsp)\n"
    "  call .Lrt_sin_d\n"
    "  movsd %xmm0, 8(%rsp)\n"
    "  movsd 0(%rsp), %xmm0\n"
    "  movabsq $4609753056924675352, %rax\n"
    "  movq %rax, %xmm1\n"
    "  addsd %xmm1, %xmm0\n"
    "  call .Lrt_sin_d\n"
    "  movsd 8(%rsp), %xmm1\n"
    "  divsd %xmm0, %xmm1\n"
    "  cvtsd2ss %xmm1, %xmm0\n"
    "  addq $24, %rsp\n"
    "  ret\n"
    "expf:\n"
    "  cvtss2sd %xmm0, %xmm0\n"
    "  movabsq $4635963235168681984, %rax\n"
    "  movq %rax, %xmm1\n"
    "  ucomisd %xmm1, %xmm0\n"
    "  jae .Lexp_inf\n"
    "  movabsq $13860390803186122752, %rax\n"
    "  movq %rax, %xmm1\n"
    "  ucomisd %xmm1, %xmm0\n"
    "  jbe .Lexp_zero\n"
    "  movabsq $4609176140021203710, %rax\n"
    "  movq %rax, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movq %xmm1, %rax\n"
    "  testq %rax, %rax\n"
    "  js .Lexp_neg\n"
    "  movabsq $4602678819172646912, %rax\n"
    "  jmp .Lexp_rnd\n"
    ".Lexp_neg:\n"
    "  movabsq $13826050856027422720, %rax\n"
    ".Lexp_rnd:\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  cvttsd2siq %xmm1, %r8\n"
    "  cvtsi2sdq %r8, %xmm2\n"
    "  movabsq $4604418534313441775, %rax\n"
    "  movq %rax, %xmm3\n"
    "  mulsd %xmm3, %xmm2\n"
    "  subsd %xmm2, %xmm0\n"
    "  movabsq $4551452160554016794, %rax\n"
    "  movq %rax, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movabsq $4564047942368979991, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movabsq $4575957461383581969, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movabsq $4586165620538955093, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movabsq $4595172819793696085, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movabsq $4602678819172646912, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  movabsq $4607182418800017408, %rax\n"
    "  movq %rax, %xmm2\n"
    "  addsd %xmm2, %xmm1\n"
    "  mulsd %xmm0, %xmm1\n"
    "  addsd %xmm2, %xmm1\n"
    "  addq $1023, %r8\n"
    "  shlq $52, %r8\n"
    "  movq %r8, %xmm2\n"
    "  mulsd %xmm1, %xmm2\n"
    "  cvtsd2ss %xmm2, %xmm0\n"
    "  ret\n"
    ".Lexp_inf:\n"
    "  movl $2139095040, %eax\n"
    "  movq %rax, %xmm0\n"
    "  ret\n"
    ".Lexp_zero:\n"
    "  pxor %xmm0, %xmm0\n"
    "  cvtsd2ss %xmm0, %xmm0\n"
    "  ret\n";

static const char *rt_asm_stdlib_1 =
    ".data\n"
    ".align 8\n"
    ".globl environ\n"
    "environ:\n"
    "  .quad 0\n"
    ".globl __environ\n"
    "__environ:\n"
    "  .quad 0\n"
    ".text\n"
    ".globl _start\n"
    ".globl getenv\n"
    ".globl strtol\n"
    ".globl strtoul\n"
    ".globl atoi\n"
    ".globl atol\n"
    ".globl abs\n"
    ".globl labs\n"
    "_start:\n"
    "  popq %rdi\n"
    "  movq %rsp, %rsi\n"
    "  movq %rdi, %rdx\n"
    "  shlq $3, %rdx\n"
    "  addq %rsi, %rdx\n"
    "  addq $8, %rdx\n"
    "  movq %rdx, environ(%rip)\n"
    "  movq %rdx, __environ(%rip)\n"
    "  andq $-16, %rsp\n"
    "  call main\n"
    "  movq %rax, %rdi\n"
    "  movl $231, %eax\n"
    "  syscall\n"
    "  movl $60, %eax\n"
    "  syscall\n"
    "getenv:\n"
    "  testq %rdi, %rdi\n"
    "  je .Lge_null\n"
    "  movq environ(%rip), %rsi\n"
    "  testq %rsi, %rsi\n"
    "  je .Lge_null\n"
    ".Lge_outer:\n"
    "  movq (%rsi), %r9\n"
    "  testq %r9, %r9\n"
    "  je .Lge_null\n"
    "  movq %rdi, %r8\n"
    ".Lge_cmp:\n"
    "  movzbq (%r8), %rax\n"
    "  cmpq $0, %rax\n"
    "  je .Lge_end\n"
    "  cmpq $61, %rax\n"
    "  je .Lge_null\n"
    "  movzbq (%r9), %rcx\n"
    "  cmpq %rcx, %rax\n"
    "  jne .Lge_next\n"
    "  incq %r8\n"
    "  incq %r9\n"
    "  jmp .Lge_cmp\n"
    ".Lge_end:\n"
    "  movzbq (%r9), %rcx\n"
    "  cmpq $61, %rcx\n"
    "  jne .Lge_next\n"
    "  leaq 1(%r9), %rax\n"
    "  ret\n"
    ".Lge_next:\n"
    "  addq $8, %rsi\n"
    "  jmp .Lge_outer\n"
    ".Lge_null:\n"
    "  xorq %rax, %rax\n"
    "  ret\n"
    "strtoul:\n"
    "strtol:\n"
    "  movq %rdi, %r10\n"
    ".Lst_ws:\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpq $32, %rax\n"
    "  je .Lst_ws_adv\n"
    "  cmpq $9, %rax\n"
    "  jb .Lst_sign_chk\n"
    "  cmpq $13, %rax\n"
    "  ja .Lst_sign_chk\n"
    ".Lst_ws_adv:\n"
    "  incq %rdi\n"
    "  jmp .Lst_ws\n"
    ".Lst_sign_chk:\n"
    "  xorq %r11, %r11\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpq $45, %rax\n"
    "  je .Lst_neg\n"
    "  cmpq $43, %rax\n"
    "  jne .Lst_base\n"
    "  incq %rdi\n"
    "  jmp .Lst_base\n"
    ".Lst_neg:\n"
    "  movq $1, %r11\n"
    "  incq %rdi\n"
    ".Lst_base:\n"
    "  movslq %edx, %r9\n"
    "  testq %r9, %r9\n"
    "  jne .Lst_chk_b16\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpq $48, %rax\n"
    "  jne .Lst_b10\n"
    "  movzbq 1(%rdi), %rax\n"
    "  cmpq $120, %rax\n"
    "  je .Lst_b0_hex\n"
    "  cmpq $88, %rax\n"
    "  je .Lst_b0_hex\n"
    "  movq $8, %r9\n"
    "  jmp .Lst_loop_init\n"
    ".Lst_b0_hex:\n"
    "  movzbq 2(%rdi), %rax\n"
    "  call .Lst_is_hex\n"
    "  testq %rax, %rax\n"
    "  jne .Lst_b0_use16\n"
    "  movq $8, %r9\n"
    "  jmp .Lst_loop_init\n"
    ".Lst_b0_use16:\n"
    "  addq $2, %rdi\n"
    "  movq $16, %r9\n"
    "  jmp .Lst_loop_init\n"
    ".Lst_b10:\n"
    "  movq $10, %r9\n"
    "  jmp .Lst_loop_init\n"
    ".Lst_chk_b16:\n"
    "  cmpq $16, %r9\n"
    "  jne .Lst_loop_init\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpq $48, %rax\n"
    "  jne .Lst_loop_init\n"
    "  movzbq 1(%rdi), %rax\n"
    "  cmpq $120, %rax\n"
    "  je .Lst_b16_x\n"
    "  cmpq $88, %rax\n"
    "  jne .Lst_loop_init\n"
    ".Lst_b16_x:\n"
    "  movzbq 2(%rdi), %rax\n"
    "  call .Lst_is_hex\n"
    "  testq %rax, %rax\n"
    "  je .Lst_loop_init\n"
    "  addq $2, %rdi\n"
    ".Lst_loop_init:\n"
    "  xorq %rax, %rax\n"
    "  xorq %r8, %r8\n"
    "  cmpq $2, %r9\n"
    "  jb .Lst_done\n"
    "  cmpq $36, %r9\n"
    "  ja .Lst_done\n"
    ".Lst_dig:\n"
    "  movzbq (%rdi), %rcx\n"
    "  cmpq $48, %rcx\n"
    "  jb .Lst_done\n"
    "  cmpq $57, %rcx\n"
    "  ja .Lst_up\n"
    "  subq $48, %rcx\n"
    "  jmp .Lst_have_dig\n"
    ".Lst_up:\n"
    "  cmpq $65, %rcx\n"
    "  jb .Lst_done\n"
    "  cmpq $90, %rcx\n"
    "  ja .Lst_lo\n"
    "  subq $55, %rcx\n"
    "  jmp .Lst_have_dig\n"
    ".Lst_lo:\n"
    "  cmpq $97, %rcx\n"
    "  jb .Lst_done\n"
    "  cmpq $122, %rcx\n"
    "  ja .Lst_done\n"
    "  subq $87, %rcx\n"
    ".Lst_have_dig:\n"
    "  cmpq %r9, %rcx\n"
    "  jae .Lst_done\n"
    "  imulq %r9, %rax\n"
    "  addq %rcx, %rax\n"
    "  incq %r8\n"
    "  incq %rdi\n"
    "  jmp .Lst_dig\n"
    ".Lst_done:\n"
    "  testq %rsi, %rsi\n"
    "  je .Lst_ap_neg\n"
    "  testq %r8, %r8\n"
    "  je .Lst_no_dig\n"
    "  movq %rdi, (%rsi)\n"
    "  jmp .Lst_ap_neg\n"
    ".Lst_no_dig:\n"
    "  movq %r10, (%rsi)\n"
    ".Lst_ap_neg:\n"
    "  testq %r11, %r11\n"
    "  je .Lst_ret\n"
    "  negq %rax\n"
    ".Lst_ret:\n"
    "  ret\n"
    ".Lst_is_hex:\n"
    "  cmpq $48, %rax\n"
    "  jb .Lst_ih0\n"
    "  cmpq $57, %rax\n"
    "  jbe .Lst_ih1\n"
    "  cmpq $65, %rax\n"
    "  jb .Lst_ih0\n"
    "  cmpq $70, %rax\n"
    "  jbe .Lst_ih1\n"
    "  cmpq $97, %rax\n"
    "  jb .Lst_ih0\n"
    "  cmpq $102, %rax\n"
    "  jbe .Lst_ih1\n"
    ".Lst_ih0:\n"
    "  xorl %eax, %eax\n"
    "  ret\n"
    ".Lst_ih1:\n"
    "  movl $1, %eax\n"
    "  ret\n"
    "atoi:\n"
    "atol:\n"
    "  xorq %rsi, %rsi\n"
    "  movl $10, %edx\n"
    "  jmp strtol\n"
    "abs:\n"
    "  movl %edi, %eax\n"
    "  testl %eax, %eax\n"
    "  jge .Labs_dn\n"
    "  negl %eax\n"
    ".Labs_dn:\n"
    "  ret\n"
    "labs:\n"
    "  movq %rdi, %rax\n"
    "  testq %rax, %rax\n"
    "  jge .Llabs_dn\n"
    "  negq %rax\n"
    ".Llabs_dn:\n"
    "  ret\n";

static const char *rt_asm_stdlib_2 =
    ".text\n"
    ".globl qsort\n"
    ".globl bsearch\n"
    "qsort:\n"
    "  cmpq $1, %rsi\n"
    "  jbe .Lqs_bail\n"
    "  testq %rdx, %rdx\n"
    "  je .Lqs_bail\n"
    "  testq %rcx, %rcx\n"
    "  je .Lqs_bail\n"
    "  pushq %rbx\n"
    "  pushq %rbp\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  subq $24, %rsp\n"
    "  movq %rdi, %r12\n"
    "  movq %rsi, %r13\n"
    "  movq %rdx, %r14\n"
    "  movq %rcx, %r15\n"
    "  movq %rsi, %rbp\n"
    "  shrq $1, %rbp\n"
    ".Lqs_gap:\n"
    "  testq %rbp, %rbp\n"
    "  je .Lqs_done\n"
    "  movq %rbp, 0(%rsp)\n"
    ".Lqs_i:\n"
    "  movq 0(%rsp), %rbx\n"
    "  cmpq %r13, %rbx\n"
    "  jae .Lqs_next_gap\n"
    ".Lqs_j:\n"
    "  cmpq %rbp, %rbx\n"
    "  jb .Lqs_next_i\n"
    "  movq %rbx, %rax\n"
    "  subq %rbp, %rax\n"
    "  imulq %r14, %rax\n"
    "  addq %r12, %rax\n"
    "  movq %rax, 8(%rsp)\n"
    "  movq %rbx, %rcx\n"
    "  imulq %r14, %rcx\n"
    "  addq %r12, %rcx\n"
    "  movq %rcx, 16(%rsp)\n"
    "  movq %rax, %rdi\n"
    "  movq %rcx, %rsi\n"
    "  call *%r15\n"
    "  testl %eax, %eax\n"
    "  jle .Lqs_next_i\n"
    "  movq 8(%rsp), %rdi\n"
    "  movq 16(%rsp), %rsi\n"
    "  movq %r14, %rcx\n"
    ".Lqs_sw:\n"
    "  movzbq (%rdi), %rax\n"
    "  movzbq (%rsi), %rdx\n"
    "  movb %dl, (%rdi)\n"
    "  movb %al, (%rsi)\n"
    "  incq %rdi\n"
    "  incq %rsi\n"
    "  decq %rcx\n"
    "  jne .Lqs_sw\n"
    "  subq %rbp, %rbx\n"
    "  jmp .Lqs_j\n"
    ".Lqs_next_i:\n"
    "  movq 0(%rsp), %rax\n"
    "  incq %rax\n"
    "  movq %rax, 0(%rsp)\n"
    "  jmp .Lqs_i\n"
    ".Lqs_next_gap:\n"
    "  shrq $1, %rbp\n"
    "  jmp .Lqs_gap\n"
    ".Lqs_done:\n"
    "  addq $24, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbp\n"
    "  popq %rbx\n"
    ".Lqs_bail:\n"
    "  ret\n"
    "bsearch:\n"
    "  testq %rdx, %rdx\n"
    "  je .Lbs_null\n"
    "  testq %rcx, %rcx\n"
    "  je .Lbs_null\n"
    "  testq %r8, %r8\n"
    "  je .Lbs_null\n"
    "  pushq %rbx\n"
    "  pushq %rbp\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  subq $24, %rsp\n"
    "  movq %rdi, %r12\n"
    "  movq %rsi, %r13\n"
    "  xorq %rbx, %rbx\n"
    "  movq %rdx, %rbp\n"
    "  movq %rcx, %r14\n"
    "  movq %r8, %r15\n"
    ".Lbs_lp:\n"
    "  cmpq %rbp, %rbx\n"
    "  jae .Lbs_nf\n"
    "  movq %rbp, %rax\n"
    "  subq %rbx, %rax\n"
    "  shrq $1, %rax\n"
    "  addq %rbx, %rax\n"
    "  movq %rax, 0(%rsp)\n"
    "  imulq %r14, %rax\n"
    "  addq %r13, %rax\n"
    "  movq %rax, 8(%rsp)\n"
    "  movq %r12, %rdi\n"
    "  movq %rax, %rsi\n"
    "  call *%r15\n"
    "  cmpl $0, %eax\n"
    "  je .Lbs_hit\n"
    "  jl .Lbs_lt\n"
    "  movq 0(%rsp), %rbx\n"
    "  incq %rbx\n"
    "  jmp .Lbs_lp\n"
    ".Lbs_lt:\n"
    "  movq 0(%rsp), %rbp\n"
    "  jmp .Lbs_lp\n"
    ".Lbs_hit:\n"
    "  movq 8(%rsp), %rax\n"
    "  jmp .Lbs_ep\n"
    ".Lbs_nf:\n"
    "  xorq %rax, %rax\n"
    ".Lbs_ep:\n"
    "  addq $24, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbp\n"
    "  popq %rbx\n"
    "  ret\n"
    ".Lbs_null:\n"
    "  xorq %rax, %rax\n"
    "  ret\n";

static const char *rt_asm_str_1 =
    ".text\n"
    ".globl strchr\n"
    ".globl strrchr\n"
    ".globl memchr\n"
    ".globl strstr\n"
    ".globl strcat\n"
    ".globl strncat\n"
    ".globl strdup\n"
    ".globl isspace\n"
    ".globl isdigit\n"
    ".globl isalpha\n"
    ".globl isalnum\n"
    ".globl isupper\n"
    ".globl islower\n"
    ".globl tolower\n"
    ".globl toupper\n"
    "strchr:\n"
    "  movzbq %sil, %rsi\n"
    ".Lsch_lp:\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpq %rsi, %rax\n"
    "  je .Lsch_hit\n"
    "  testq %rax, %rax\n"
    "  je .Lsch_null\n"
    "  incq %rdi\n"
    "  jmp .Lsch_lp\n"
    ".Lsch_hit:\n"
    "  movq %rdi, %rax\n"
    "  ret\n"
    ".Lsch_null:\n"
    "  xorq %rax, %rax\n"
    "  ret\n"
    "strrchr:\n"
    "  movzbq %sil, %rsi\n"
    "  xorq %rdx, %rdx\n"
    ".Lsrc_lp:\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpq %rsi, %rax\n"
    "  jne .Lsrc_nx\n"
    "  movq %rdi, %rdx\n"
    ".Lsrc_nx:\n"
    "  testq %rax, %rax\n"
    "  je .Lsrc_dn\n"
    "  incq %rdi\n"
    "  jmp .Lsrc_lp\n"
    ".Lsrc_dn:\n"
    "  movq %rdx, %rax\n"
    "  ret\n"
    "memchr:\n"
    "  movzbq %sil, %rsi\n"
    ".Lmc2_lp:\n"
    "  testq %rdx, %rdx\n"
    "  je .Lmc2_null\n"
    "  movzbq (%rdi), %rax\n"
    "  cmpq %rsi, %rax\n"
    "  je .Lmc2_hit\n"
    "  incq %rdi\n"
    "  decq %rdx\n"
    "  jmp .Lmc2_lp\n"
    ".Lmc2_hit:\n"
    "  movq %rdi, %rax\n"
    "  ret\n"
    ".Lmc2_null:\n"
    "  xorq %rax, %rax\n"
    "  ret\n"
    "strstr:\n"
    "  movzbq (%rsi), %rax\n"
    "  testq %rax, %rax\n"
    "  jne .Lss_out\n"
    "  movq %rdi, %rax\n"
    "  ret\n"
    ".Lss_out:\n"
    "  movzbq (%rdi), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lss_null\n"
    "  movq %rdi, %r8\n"
    "  movq %rsi, %r9\n"
    ".Lss_in:\n"
    "  movzbq (%r9), %rcx\n"
    "  testq %rcx, %rcx\n"
    "  je .Lss_hit\n"
    "  movzbq (%r8), %rax\n"
    "  cmpq %rcx, %rax\n"
    "  jne .Lss_adv\n"
    "  incq %r8\n"
    "  incq %r9\n"
    "  jmp .Lss_in\n"
    ".Lss_adv:\n"
    "  incq %rdi\n"
    "  jmp .Lss_out\n"
    ".Lss_hit:\n"
    "  movq %rdi, %rax\n"
    "  ret\n"
    ".Lss_null:\n"
    "  xorq %rax, %rax\n"
    "  ret\n"
    "strcat:\n"
    "  movq %rdi, %r8\n"
    ".Lsca_seek:\n"
    "  movzbq (%rdi), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lsca_cp\n"
    "  incq %rdi\n"
    "  jmp .Lsca_seek\n"
    ".Lsca_cp:\n"
    "  movzbq (%rsi), %rax\n"
    "  movb %al, (%rdi)\n"
    "  incq %rdi\n"
    "  incq %rsi\n"
    "  testq %rax, %rax\n"
    "  jne .Lsca_cp\n"
    "  movq %r8, %rax\n"
    "  ret\n"
    "strncat:\n"
    "  movq %rdi, %r8\n"
    ".Lsna_seek:\n"
    "  movzbq (%rdi), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lsna_cp\n"
    "  incq %rdi\n"
    "  jmp .Lsna_seek\n"
    ".Lsna_cp:\n"
    "  testq %rdx, %rdx\n"
    "  je .Lsna_z\n"
    "  movzbq (%rsi), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lsna_z\n"
    "  movb %al, (%rdi)\n"
    "  incq %rdi\n"
    "  incq %rsi\n"
    "  decq %rdx\n"
    "  jmp .Lsna_cp\n"
    ".Lsna_z:\n"
    "  xorl %eax, %eax\n"
    "  movb %al, (%rdi)\n"
    "  movq %r8, %rax\n"
    "  ret\n"
    "strdup:\n"
    "  pushq %rbx\n"
    "  movq %rdi, %rbx\n"
    "  call strlen\n"
    "  incq %rax\n"
    "  movq %rax, %rdi\n"
    "  call malloc\n"
    "  testq %rax, %rax\n"
    "  je .Lsd_ret\n"
    "  movq %rax, %rdi\n"
    "  movq %rbx, %rsi\n"
    "  call strcpy\n"
    ".Lsd_ret:\n"
    "  popq %rbx\n"
    "  ret\n"
    "isspace:\n"
    "  movzbq %dil, %rax\n"
    "  cmpq $32, %rax\n"
    "  je .Lct_y\n"
    "  cmpq $9, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $13, %rax\n"
    "  jbe .Lct_y\n"
    "  jmp .Lct_n\n"
    "isdigit:\n"
    "  movzbq %dil, %rax\n"
    "  cmpq $48, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $57, %rax\n"
    "  jbe .Lct_y\n"
    "  jmp .Lct_n\n"
    "isupper:\n"
    "  movzbq %dil, %rax\n"
    "  cmpq $65, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $90, %rax\n"
    "  jbe .Lct_y\n"
    "  jmp .Lct_n\n"
    "islower:\n"
    "  movzbq %dil, %rax\n"
    "  cmpq $97, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $122, %rax\n"
    "  jbe .Lct_y\n"
    "  jmp .Lct_n\n"
    "isalpha:\n"
    "  movzbq %dil, %rax\n"
    "  cmpq $65, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $90, %rax\n"
    "  jbe .Lct_y\n"
    "  cmpq $97, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $122, %rax\n"
    "  jbe .Lct_y\n"
    "  jmp .Lct_n\n"
    "isalnum:\n"
    "  movzbq %dil, %rax\n"
    "  cmpq $48, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $57, %rax\n"
    "  jbe .Lct_y\n"
    "  cmpq $65, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $90, %rax\n"
    "  jbe .Lct_y\n"
    "  cmpq $97, %rax\n"
    "  jb .Lct_n\n"
    "  cmpq $122, %rax\n"
    "  jbe .Lct_y\n"
    ".Lct_n:\n"
    "  xorl %eax, %eax\n"
    "  ret\n"
    ".Lct_y:\n"
    "  movl $1, %eax\n"
    "  ret\n"
    "tolower:\n"
    "  movl %edi, %eax\n"
    "  cmpq $65, %rax\n"
    "  jb .Ltl_dn\n"
    "  cmpq $90, %rax\n"
    "  ja .Ltl_dn\n"
    "  addq $32, %rax\n"
    ".Ltl_dn:\n"
    "  ret\n"
    "toupper:\n"
    "  movl %edi, %eax\n"
    "  cmpq $97, %rax\n"
    "  jb .Ltu_dn\n"
    "  cmpq $122, %rax\n"
    "  ja .Ltu_dn\n"
    "  subq $32, %rax\n"
    ".Ltu_dn:\n"
    "  ret\n";

static const char *rt_asm_str_2 =
    ".text\n"
    ".globl sscanf\n"
    "sscanf:\n"
    "  pushq %rbx\n"
    "  pushq %rbp\n"
    "  pushq %r12\n"
    "  pushq %r13\n"
    "  pushq %r14\n"
    "  pushq %r15\n"
    "  subq $88, %rsp\n"
    "  movq %rsp, %rbp\n"
    "  movq %rdx, 16(%rbp)\n"
    "  movq %rcx, 24(%rbp)\n"
    "  movq %r8, 32(%rbp)\n"
    "  movq %r9, 40(%rbp)\n"
    "  movq $16, 48(%rbp)\n"
    "  leaq 144(%rbp), %rax\n"
    "  movq %rax, 56(%rbp)\n"
    "  movq %rdi, %r12\n"
    "  movq %rsi, %r13\n"
    "  xorq %r14, %r14\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_next_arg:\n"
    "  movq 48(%rbp), %rax\n"
    "  cmpq $48, %rax\n"
    "  jae .Lsf_na_ovf\n"
    "  movq %rbp, %rdx\n"
    "  addq %rax, %rdx\n"
    "  addq $8, %rax\n"
    "  movq %rax, 48(%rbp)\n"
    "  movq (%rdx), %rax\n"
    "  ret\n"
    ".Lsf_na_ovf:\n"
    "  movq 56(%rbp), %rdx\n"
    "  movq (%rdx), %rax\n"
    "  addq $8, %rdx\n"
    "  movq %rdx, 56(%rbp)\n"
    "  ret\n"
    ".Lsf_skip_str_ws:\n"
    "  movzbq (%r12), %rax\n"
    "  movl %eax, %edi\n"
    "  call isspace\n"
    "  testl %eax, %eax\n"
    "  je .Lsf_sws_dn\n"
    "  incq %r12\n"
    "  jmp .Lsf_skip_str_ws\n"
    ".Lsf_sws_dn:\n"
    "  ret\n"
    ".Lsf_loop:\n"
    "  movzbq (%r13), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lsf_done\n"
    "  movl %eax, %edi\n"
    "  call isspace\n"
    "  testl %eax, %eax\n"
    "  je .Lsf_not_ws\n"
    ".Lsf_fws:\n"
    "  incq %r13\n"
    "  movzbq (%r13), %rax\n"
    "  movl %eax, %edi\n"
    "  call isspace\n"
    "  testl %eax, %eax\n"
    "  jne .Lsf_fws\n"
    "  call .Lsf_skip_str_ws\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_not_ws:\n"
    "  movzbq (%r13), %rax\n"
    "  incq %r13\n"
    "  cmpq $37, %rax\n"
    "  je .Lsf_pct\n"
    "  movzbq (%r12), %rcx\n"
    "  cmpq %rax, %rcx\n"
    "  jne .Lsf_done\n"
    "  incq %r12\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_pct:\n"
    "  movzbq (%r13), %rax\n"
    "  cmpq $37, %rax\n"
    "  jne .Lsf_spec\n"
    "  incq %r13\n"
    "  movzbq (%r12), %rcx\n"
    "  cmpq $37, %rcx\n"
    "  jne .Lsf_done\n"
    "  incq %r12\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_spec:\n"
    "  movq $0, 72(%rbp)\n"
    "  movq $0, 80(%rbp)\n"
    "  movq $-1, %r15\n"
    "  cmpq $42, %rax\n"
    "  jne .Lsf_wchk\n"
    "  movq $1, 72(%rbp)\n"
    "  incq %r13\n"
    ".Lsf_wchk:\n"
    "  movzbq (%r13), %rax\n"
    "  cmpq $48, %rax\n"
    "  jb .Lsf_len\n"
    "  cmpq $57, %rax\n"
    "  ja .Lsf_len\n"
    "  cmpq $-1, %r15\n"
    "  jne .Lsf_wmul\n"
    "  xorq %r15, %r15\n"
    ".Lsf_wmul:\n"
    "  imulq $10, %r15\n"
    "  subq $48, %rax\n"
    "  addq %rax, %r15\n"
    "  incq %r13\n"
    "  jmp .Lsf_wchk\n"
    ".Lsf_len:\n"
    "  movzbq (%r13), %rax\n"
    "  cmpq $108, %rax\n"
    "  je .Lsf_llong\n"
    "  cmpq $104, %rax\n"
    "  je .Lsf_lskip\n"
    "  jmp .Lsf_conv\n"
    ".Lsf_llong:\n"
    "  movq $1, 80(%rbp)\n"
    ".Lsf_lskip:\n"
    "  incq %r13\n"
    "  jmp .Lsf_len\n"
    ".Lsf_conv:\n"
    "  movzbq (%r13), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lsf_done\n"
    "  incq %r13\n"
    "  cmpq $99, %rax\n"
    "  je .Lsf_c\n"
    "  movq %rax, %rbx\n"
    "  call .Lsf_skip_str_ws\n"
    "  movzbq (%r12), %rcx\n"
    "  testq %rcx, %rcx\n"
    "  je .Lsf_done\n"
    "  cmpq $115, %rbx\n"
    "  je .Lsf_s\n"
    "  cmpq $100, %rbx\n"
    "  je .Lsf_dec\n"
    "  cmpq $105, %rbx\n"
    "  je .Lsf_i\n"
    "  cmpq $117, %rbx\n"
    "  je .Lsf_u\n"
    "  cmpq $120, %rbx\n"
    "  je .Lsf_x\n"
    "  cmpq $88, %rbx\n"
    "  je .Lsf_x\n"
    "  cmpq $112, %rbx\n"
    "  je .Lsf_p\n"
    "  cmpq $111, %rbx\n"
    "  je .Lsf_o\n"
    "  jmp .Lsf_done\n"
    ".Lsf_c:\n"
    "  cmpq $-1, %r15\n"
    "  jne .Lsf_c_ok\n"
    "  movq $1, %r15\n"
    ".Lsf_c_ok:\n"
    "  xorq %rbx, %rbx\n"
    "  movq 72(%rbp), %rcx\n"
    "  testq %rcx, %rcx\n"
    "  jne .Lsf_c_lp\n"
    "  call .Lsf_next_arg\n"
    "  movq %rax, %rbx\n"
    ".Lsf_c_lp:\n"
    "  testq %r15, %r15\n"
    "  je .Lsf_c_dn\n"
    "  movzbq (%r12), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lsf_done\n"
    "  incq %r12\n"
    "  decq %r15\n"
    "  testq %rbx, %rbx\n"
    "  je .Lsf_c_lp\n"
    "  movb %al, (%rbx)\n"
    "  incq %rbx\n"
    "  jmp .Lsf_c_lp\n"
    ".Lsf_c_dn:\n"
    "  movq 72(%rbp), %rcx\n"
    "  testq %rcx, %rcx\n"
    "  jne .Lsf_loop\n"
    "  incq %r14\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_s:\n"
    "  xorq %rbx, %rbx\n"
    "  movq 72(%rbp), %rcx\n"
    "  testq %rcx, %rcx\n"
    "  jne .Lsf_s_lp\n"
    "  call .Lsf_next_arg\n"
    "  movq %rax, %rbx\n"
    ".Lsf_s_lp:\n"
    "  cmpq $0, %r15\n"
    "  je .Lsf_s_dn\n"
    "  movzbq (%r12), %rax\n"
    "  testq %rax, %rax\n"
    "  je .Lsf_s_dn\n"
    "  movl %eax, %edi\n"
    "  call isspace\n"
    "  testl %eax, %eax\n"
    "  jne .Lsf_s_dn\n"
    "  movzbq (%r12), %rax\n"
    "  incq %r12\n"
    "  cmpq $-1, %r15\n"
    "  je .Lsf_s_store\n"
    "  decq %r15\n"
    ".Lsf_s_store:\n"
    "  testq %rbx, %rbx\n"
    "  je .Lsf_s_lp\n"
    "  movb %al, (%rbx)\n"
    "  incq %rbx\n"
    "  jmp .Lsf_s_lp\n"
    ".Lsf_s_dn:\n"
    "  testq %rbx, %rbx\n"
    "  je .Lsf_s_sup\n"
    "  xorl %eax, %eax\n"
    "  movb %al, (%rbx)\n"
    "  incq %r14\n"
    ".Lsf_s_sup:\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_dec:\n"
    "  movl $10, %edx\n"
    "  jmp .Lsf_int\n"
    ".Lsf_i:\n"
    "  xorl %edx, %edx\n"
    "  jmp .Lsf_int\n"
    ".Lsf_u:\n"
    "  movl $10, %edx\n"
    "  jmp .Lsf_int\n"
    ".Lsf_x:\n"
    "  movl $16, %edx\n"
    "  jmp .Lsf_int\n"
    ".Lsf_p:\n"
    "  movq $1, 80(%rbp)\n"
    "  movl $16, %edx\n"
    "  jmp .Lsf_int\n"
    ".Lsf_o:\n"
    "  movl $8, %edx\n"
    ".Lsf_int:\n"
    "  movq %r12, %rdi\n"
    "  leaq 64(%rbp), %rsi\n"
    "  call strtol\n"
    "  movq 64(%rbp), %rcx\n"
    "  cmpq %r12, %rcx\n"
    "  je .Lsf_done\n"
    "  movq %rcx, %r12\n"
    "  movq 72(%rbp), %rdx\n"
    "  testq %rdx, %rdx\n"
    "  jne .Lsf_loop\n"
    "  pushq %rax\n"
    "  subq $8, %rsp\n"
    "  call .Lsf_next_arg\n"
    "  movq %rax, %rdx\n"
    "  addq $8, %rsp\n"
    "  popq %rax\n"
    "  movq 80(%rbp), %rcx\n"
    "  testq %rcx, %rcx\n"
    "  jne .Lsf_st64\n"
    "  movl %eax, (%rdx)\n"
    "  incq %r14\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_st64:\n"
    "  movq %rax, (%rdx)\n"
    "  incq %r14\n"
    "  jmp .Lsf_loop\n"
    ".Lsf_done:\n"
    "  movq %r14, %rax\n"
    "  addq $88, %rsp\n"
    "  popq %r15\n"
    "  popq %r14\n"
    "  popq %r13\n"
    "  popq %r12\n"
    "  popq %rbp\n"
    "  popq %rbx\n"
    "  ret\n";

static int build_freestanding_rt_archive(const char *ar_path) {
  static const char *rt_names[] = {
      "rt_exit.o",    "rt_write.o",  "rt_read.o",   "rt_open.o",
      "rt_close.o",   "rt_putchar.o","rt_puts.o",   "rt_strlen.o",
      "rt_strcmp.o",  "rt_strncmp.o","rt_strcpy.o", "rt_memcpy.o",
      "rt_memset.o",  "rt_memmove.o","rt_memcmp.o", "rt_alloc.o",
      "rt_stdio.o",   "rt_math.o",   "rt_printf.o", "rt_stdlib.o",
      "rt_str_ext.o",
  };
  const char *rt_srcs[21];
  rt_srcs[0] = rt_asm_exit;
  rt_srcs[1] = rt_asm_write;
  rt_srcs[2] = rt_asm_read;
  rt_srcs[3] = rt_asm_open;
  rt_srcs[4] = rt_asm_close;
  rt_srcs[5] = rt_asm_putchar;
  rt_srcs[6] = rt_asm_puts;
  rt_srcs[7] = rt_asm_strlen;
  rt_srcs[8] = rt_asm_strcmp;
  rt_srcs[9] = rt_asm_strncmp;
  rt_srcs[10] = rt_asm_strcpy;
  rt_srcs[11] = rt_asm_memcpy;
  rt_srcs[12] = rt_asm_memset;
  rt_srcs[13] = rt_asm_memmove;
  rt_srcs[14] = rt_asm_memcmp;
  rt_srcs[15] = rt_asm_alloc;
  rt_srcs[16] = rt_asm_stdio;

  size_t m1_len = strlen(rt_asm_math_1);
  size_t m2_len = strlen(rt_asm_math_2);
  char *math_full = (char *)malloc(m1_len + m2_len + 1);
  if (!math_full)
    return -1;
  memcpy(math_full, rt_asm_math_1, m1_len);
  memcpy(math_full + m1_len, rt_asm_math_2, m2_len);
  math_full[m1_len + m2_len] = '\0';
  rt_srcs[17] = math_full;

  size_t pf0_len = strlen(rt_asm_printf_0);
  size_t pf1_len = strlen(rt_asm_printf_1);
  size_t pf2_len = strlen(rt_asm_printf_2);
  char *pf_full = (char *)malloc(pf0_len + pf1_len + pf2_len + 1);
  if (!pf_full) {
    free(math_full);
    return -1;
  }
  memcpy(pf_full, rt_asm_printf_0, pf0_len);
  memcpy(pf_full + pf0_len, rt_asm_printf_1, pf1_len);
  memcpy(pf_full + pf0_len + pf1_len, rt_asm_printf_2, pf2_len);
  pf_full[pf0_len + pf1_len + pf2_len] = '\0';
  rt_srcs[18] = pf_full;

  size_t sl1_len = strlen(rt_asm_stdlib_1);
  size_t sl2_len = strlen(rt_asm_stdlib_2);
  char *stdlib_full = (char *)malloc(sl1_len + sl2_len + 1);
  if (!stdlib_full) {
    free(math_full);
    free(pf_full);
    return -1;
  }
  memcpy(stdlib_full, rt_asm_stdlib_1, sl1_len);
  memcpy(stdlib_full + sl1_len, rt_asm_stdlib_2, sl2_len);
  stdlib_full[sl1_len + sl2_len] = '\0';
  rt_srcs[19] = stdlib_full;

  size_t se1_len = strlen(rt_asm_str_1);
  size_t se2_len = strlen(rt_asm_str_2);
  char *strext_full = (char *)malloc(se1_len + se2_len + 1);
  if (!strext_full) {
    free(math_full);
    free(pf_full);
    free(stdlib_full);
    return -1;
  }
  memcpy(strext_full, rt_asm_str_1, se1_len);
  memcpy(strext_full + se1_len, rt_asm_str_2, se2_len);
  strext_full[se1_len + se2_len] = '\0';
  rt_srcs[20] = strext_full;

  FILE *ar_f = fopen(ar_path, "wb");
  if (!ar_f) {
    free(math_full);
    free(pf_full);
    free(stdlib_full);
    free(strext_full);
    return -1;
  }
  fwrite("!<arch>\n", 1, 8, ar_f);

  int u;
  int fail = 0;
  for (u = 0; u < 21; u++) {
    char tmp_o[320];
    snprintf(tmp_o, sizeof(tmp_o), "%s.%d.o", ar_path, u);
    int rc = assemble(NULL, tmp_o, rt_srcs[u], strlen(rt_srcs[u]), 0);
    if (rc != 0) {
      remove(tmp_o);
      fail = 1;
      break;
    }
    FILE *of = fopen(tmp_o, "rb");
    if (!of) {
      remove(tmp_o);
      fail = 1;
      break;
    }
    fseek(of, 0, SEEK_END);
    long sz_l = ftell(of);
    fseek(of, 0, SEEK_SET);
    if (sz_l <= 0) {
      fclose(of);
      remove(tmp_o);
      fail = 1;
      break;
    }
    size_t sz = (size_t)sz_l;
    unsigned char *buf = (unsigned char *)malloc(sz);
    if (!buf || fread(buf, 1, sz, of) != sz) {
      if (buf)
        free(buf);
      fclose(of);
      remove(tmp_o);
      fail = 1;
      break;
    }
    fclose(of);
    remove(tmp_o);

    char hdr[61];
    char mem_name[20];
    snprintf(mem_name, sizeof(mem_name), "%s/", rt_names[u]);
    snprintf(hdr, sizeof(hdr), "%-16s%-12s%-6s%-6s%-8s%-10lu%c%c", mem_name,
             "0", "0", "0", "100644", (unsigned long)sz, 0x60, 0x0a);
    fwrite(hdr, 1, 60, ar_f);
    fwrite(buf, 1, sz, ar_f);
    if (sz & 1) {
      fputc('\n', ar_f);
    }
    free(buf);
  }

  fclose(ar_f);
  free(math_full);
  free(pf_full);
  free(stdlib_full);
  free(strext_full);
  if (fail) {
    remove(ar_path);
    return -1;
  }
  return 0;
}

static int run_zld_with_freestanding_rt(const char **objs, int obj_count,
                                        const char *out_path,
                                        const char *script_path) {
  if (script_path) {
    return zld_link(objs, obj_count, out_path, script_path, NULL, NULL, NULL);
  }
  char rt_ar_path[256];
  static int rt_seq = 0;
#ifdef _WIN32
  snprintf(rt_ar_path, sizeof(rt_ar_path), ".tmp_zcc_rt_%lu_%d.a",
           (unsigned long)_getpid(), ++rt_seq);
#else
  snprintf(rt_ar_path, sizeof(rt_ar_path), ".tmp_zcc_rt_%lu_%d.a",
           (unsigned long)getpid(), ++rt_seq);
#endif
  const char *all_objs[2049];
  int n = (obj_count < 2048) ? obj_count : 2048;
  int i;
  for (i = 0; i < n; i++) {
    all_objs[i] = objs[i];
  }
  int have_rt = (build_freestanding_rt_archive(rt_ar_path) == 0);
  if (have_rt) {
    all_objs[n++] = rt_ar_path;
  }
  int ret = zld_link(all_objs, n, out_path, script_path, NULL, NULL, NULL);
  if (have_rt) {
    remove(rt_ar_path);
  }
  return ret;
}

int main(int argc, char **argv) {
  atexit(cleanup_temp_files);
  g_win64_ms_abi = 0;
  int i;
  int use_system_as = 0;
  int native_elf = 0;
  int compile_only = 0;
  int has_explicit_emit_obj = 0;
  char *input_c_file = NULL;
  char *out_filename = NULL;
  char *ld_script = NULL;
  const char *obj_files[2048];
  int obj_count = 0;
  int has_trace_abi = 0;
  int has_emit_gguf = 0;
  int has_frontend_dump = 0;
  int has_wasm_target = 0;
  int has_yul_target = 0;
  int has_stark_proof_target = 0;
  int has_arm_target = 0;
  int has_arm64_target = 0;
  int has_riscv_target = 0;
  int has_win64_target = 0;
  int has_win64_abi = 0;
  int has_profile_target = 0;
  int has_profile_json = 0;

  int has_stop_at_asm = 0;
  int has_lib_flag = 0;
  int has_explicit_zld = 0;
  int c_file_count = 0;
  const char *input_c_files[64];
  const char *input_s_files[64];
  const char *input_s_file = NULL;
  int s_file_count = 0;

  /* Parse arguments */
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--use-system-as") == 0) {
      use_system_as = 1;
    } else if (strcmp(argv[i], "--native-elf") == 0) {
      native_elf = 1;
    } else if (strcmp(argv[i], "--target=wasm32-wasi") == 0 ||
               strcmp(argv[i], "--target=wasm32") == 0 ||
               strcmp(argv[i], "--target=wasm") == 0 ||
               strcmp(argv[i], "-target=wasm") == 0 ||
               strcmp(argv[i], "-target=wasm32") == 0 ||
               strcmp(argv[i], "-target=wasm32-wasi") == 0 ||
               strcmp(argv[i], "--wasm") == 0 ||
               strcmp(argv[i], "-wasm") == 0 ||
               strcmp(argv[i], "--emit-wasm") == 0 ||
               strcmp(argv[i], "-emit-wasm") == 0) {
      has_wasm_target = 1;
    } else if ((strcmp(argv[i], "-target") == 0 ||
                strcmp(argv[i], "--target") == 0) &&
               i + 1 < argc) {
      if (strncmp(argv[i + 1], "wasm", 4) == 0) {
        has_wasm_target = 1;
        i++;
      } else if (strncmp(argv[i + 1], "arm64", 5) == 0 ||
                 strncmp(argv[i + 1], "aarch64", 7) == 0) {
        has_arm_target = 1;
        has_arm64_target = 1;
        i++;
      } else if (strncmp(argv[i + 1], "arm", 3) == 0 ||
                 strncmp(argv[i + 1], "thumb", 5) == 0) {
        has_arm_target = 1;
        i++;
      } else if (strncmp(argv[i + 1], "riscv", 5) == 0 ||
                 strncmp(argv[i + 1], "rv64", 4) == 0) {
        has_riscv_target = 1;
        i++;
      } else if (strncmp(argv[i + 1], "win64", 5) == 0 ||
                 strncmp(argv[i + 1], "pe64", 4) == 0 ||
                 strncmp(argv[i + 1], "windows", 7) == 0) {
        has_win64_target = 1;
        i++;
      }
    } else if (strcmp(argv[i], "--target=win64") == 0 ||
               strcmp(argv[i], "-target=win64") == 0 ||
               strcmp(argv[i], "--target=pe64") == 0 ||
               strcmp(argv[i], "-target=pe64") == 0 ||
               strcmp(argv[i], "-win64") == 0 ||
               strcmp(argv[i], "--win64") == 0 ||
               strcmp(argv[i], "-pe64") == 0 ||
               strcmp(argv[i], "--pe64") == 0) {
      has_win64_target = 1;
    } else if (strcmp(argv[i], "-mabi=ms") == 0 ||
               strcmp(argv[i], "-mabi=win64") == 0 ||
               strcmp(argv[i], "--mabi=ms") == 0 ||
               strcmp(argv[i], "--mabi=win64") == 0) {
      has_win64_abi = 1;
      g_win64_ms_abi = 1;
    } else if (strcmp(argv[i], "-mabi=sysv") == 0 ||
               strcmp(argv[i], "--mabi=sysv") == 0) {
      has_win64_abi = 0;
      g_win64_ms_abi = 0;
    } else if (strcmp(argv[i], "-mwindows") == 0 ||
               strcmp(argv[i], "--subsystem=windows") == 0 ||
               strcmp(argv[i], "-subsystem=windows") == 0) {
      g_win64_gui_subsystem = 1;
    } else if (strcmp(argv[i], "-shared") == 0 ||
               strcmp(argv[i], "--shared") == 0) {
      g_win64_is_shared = 1;
    } else if (strncmp(argv[i], "--embed-certificate=", 20) == 0) {
      g_win64_cert_file = argv[i] + 20;
    } else if (strcmp(argv[i], "--embed-certificate") == 0 && i + 1 < argc) {
      g_win64_cert_file = argv[++i];
    } else if (strncmp(argv[i], "--cert-sig=", 11) == 0) {
      g_win64_cert_file = argv[i] + 11;
    } else if (strcmp(argv[i], "--cert-sig") == 0 && i + 1 < argc) {
      g_win64_cert_file = argv[++i];
    } else if (strncmp(argv[i], "--delay-load=", 13) == 0 ||
               strncmp(argv[i], "-delayload:", 11) == 0) {
      const char *val = strchr(argv[i], '=');
      if (!val)
        val = strchr(argv[i], ':');
      if (val && val[1] && g_win64_delay_dll_count < 16) {
        val++;
        char *colon = strchr(val, ':');
        if (colon) {
          size_t dlen = colon - val;
          char *dname = (char *)malloc(dlen + 1);
          strncpy(dname, val, dlen);
          dname[dlen] = '\0';
          size_t cur_d = g_win64_delay_dll_count++;
          g_win64_delay_dlls[cur_d] = dname;
          char *fstart = colon + 1;
          while (*fstart) {
            char *comma = strchr(fstart, ',');
            size_t flen = comma ? (size_t)(comma - fstart) : strlen(fstart);
            if (flen > 0 && g_win64_delay_func_counts[cur_d] < 32) {
              char *fname = (char *)malloc(flen + 1);
              strncpy(fname, fstart, flen);
              fname[flen] = '\0';
              g_win64_delay_funcs[cur_d][g_win64_delay_func_counts[cur_d]++] =
                  fname;
            }
            if (!comma)
              break;
            fstart = comma + 1;
          }
        } else {
          size_t cur_d = g_win64_delay_dll_count++;
          g_win64_delay_dlls[cur_d] = val;
          g_win64_delay_funcs[cur_d][g_win64_delay_func_counts[cur_d]++] =
              "DllStubFunction";
        }
      }
    } else if (strcmp(argv[i], "--target=arm64") == 0 ||
               strcmp(argv[i], "--target=aarch64") == 0 ||
               strcmp(argv[i], "-target=arm64") == 0 ||
               strcmp(argv[i], "-target=aarch64") == 0 ||
               strncmp(argv[i], "--target=arm64-", 15) == 0 ||
               strncmp(argv[i], "--target=aarch64-", 17) == 0 ||
               strcmp(argv[i], "-arm64") == 0 ||
               strcmp(argv[i], "-aarch64") == 0 ||
               strcmp(argv[i], "--arm64") == 0 ||
               strcmp(argv[i], "--aarch64") == 0) {
      has_arm_target = 1;
      has_arm64_target = 1;
    } else if (strcmp(argv[i], "--target=arm") == 0 ||
               strcmp(argv[i], "--target=thumb") == 0) {
      has_arm_target = 1;
    } else if (strcmp(argv[i], "--target=riscv64") == 0 ||
               strcmp(argv[i], "--target=riscv") == 0 ||
               strcmp(argv[i], "-target=riscv64") == 0 ||
               strcmp(argv[i], "-target=riscv") == 0 ||
               strncmp(argv[i], "--target=riscv64-", 17) == 0 ||
               strcmp(argv[i], "-riscv64") == 0 ||
               strcmp(argv[i], "-riscv") == 0 ||
               strcmp(argv[i], "--riscv64") == 0 ||
               strcmp(argv[i], "--riscv") == 0 ||
               strcmp(argv[i], "--target=rv64gc") == 0 ||
               strcmp(argv[i], "-target=rv64gc") == 0) {
      has_riscv_target = 1;
    } else if (strcmp(argv[i], "--emit-yul") == 0 ||
               strcmp(argv[i], "-emit-yul") == 0 ||
               strcmp(argv[i], "--target=yul") == 0) {
      has_yul_target = 1;
    } else if (strcmp(argv[i], "--emit-stark-proof") == 0 ||
               strcmp(argv[i], "-emit-stark-proof") == 0 ||
               strcmp(argv[i], "--target=stark-proof") == 0) {
      has_stark_proof_target = 1;
    } else if (strcmp(argv[i], "--profile") == 0 ||
               strcmp(argv[i], "-profile") == 0 ||
               strcmp(argv[i], "--asm-diagnostics") == 0) {
      has_profile_target = 1;
    } else if (strcmp(argv[i], "--profile-json") == 0) {
      has_profile_target = 1;
      has_profile_json = 1;
    } else if (strcmp(argv[i], "--reproducible") == 0 ||
               strcmp(argv[i], "-reproducible") == 0) {
      g_win64_reproducible = 1;
    } else if (strcmp(argv[i], "--guard-cf") == 0 ||
               strcmp(argv[i], "-guard-cf") == 0) {
      g_win64_guard_cf = 1;
    } else if (strcmp(argv[i], "--trace-abi") == 0) {
      has_trace_abi = 1;
    } else if (strcmp(argv[i], "--emit-gguf") == 0) {
      has_emit_gguf = 1;
    } else if (strcmp(argv[i], "--pp-only") == 0 ||
               strcmp(argv[i], "--dump-ast-json") == 0 ||
               strncmp(argv[i], "--svg-", 6) == 0) {
      has_frontend_dump = 1;
    } else if (strcmp(argv[i], "-c") == 0) {
      compile_only = 1;
    } else if (strcmp(argv[i], "-emit-obj") == 0) {
      compile_only = 1;
      has_explicit_emit_obj = 1;
    } else if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "-E") == 0) {
      has_stop_at_asm = 1;
    } else if (strcmp(argv[i], "-o") == 0) {
      if (i + 1 < argc) {
        out_filename = argv[i + 1];
        i++;
        int olen = strlen(out_filename);
        if (olen > 4 && strcmp(out_filename + olen - 4, ".exe") == 0) {
          has_win64_target = 1;
        }
      }
    } else if (strcmp(argv[i], "-T") == 0) {
      if (i + 1 < argc) {
        ld_script = argv[i + 1];
        i++;
      }
    } else if (strcmp(argv[i], "-zld") == 0) {
      has_explicit_zld = 1;
    } else if (strncmp(argv[i], "-l", 2) == 0 ||
               strcmp(argv[i], "-shared") == 0 ||
               strcmp(argv[i], "-pie") == 0) {
      has_lib_flag = 1;
    } else if (argv[i][0] != '-') {
      int len = strlen(argv[i]);
      if (len > 2 && strcmp(argv[i] + len - 2, ".c") == 0) {
        if (c_file_count < 64) {
          input_c_files[c_file_count] = argv[i];
        }
        input_c_file = argv[i];
        c_file_count++;
      } else if ((len > 2 && (strcmp(argv[i] + len - 2, ".o") == 0 ||
                              strcmp(argv[i] + len - 2, ".a") == 0)) ||
                 (len > 4 && strcmp(argv[i] + len - 4, ".obj") == 0)) {
        if (obj_count < 2048) {
          obj_files[obj_count++] = argv[i];
        }
      } else if (len > 2 && strcmp(argv[i] + len - 2, ".s") == 0) {
        if (s_file_count < 64) {
          input_s_files[s_file_count] = argv[i];
        }
        input_s_file = argv[i];
        s_file_count++;
      }
    }
  }

  /* Fallback escape hatch: use system assembler and linker if explicitly
   * requested */
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

  /* Bootstrap pass-through: compile zcc.c using legacy compiler driver logic,
   * unless explicit -emit-obj requests native ELF object synthesis */
  int is_zcc_c = 0;
  if (input_c_file && !has_explicit_emit_obj && !has_win64_target &&
      !has_wasm_target && !has_arm64_target && !has_riscv_target) {
    const char *p = input_c_file;
    int len = strlen(p);
    if (strcmp(p, "zcc.c") == 0 ||
        (len >= 6 && strcmp(p + len - 6, "/zcc.c") == 0)) {
      is_zcc_c = 1;
    }
  }

  const char *base = input_c_file;
  if (input_c_file) {
    const char *p = input_c_file;
    while (*p) {
      if (*p == '/' || *p == '\\') {
        base = p + 1;
      }
      p++;
    }
  }

  /* Output-assembly pass-through: if compiling directly to .s, use legacy path
   */
  char derived_s_filename[256];
  if (has_stop_at_asm && !out_filename && base) {
    strncpy(derived_s_filename, base, sizeof(derived_s_filename) - 1);
    derived_s_filename[sizeof(derived_s_filename) - 1] = '\0';
    int len = strlen(derived_s_filename);
    if (len > 2 && strcmp(derived_s_filename + len - 2, ".c") == 0) {
      derived_s_filename[len - 2] = '\0';
      strcat(derived_s_filename, ".s");
    } else {
      strcat(derived_s_filename, ".s");
    }
    char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 3));
    int mod_argc = 0;
    for (i = 0; i < argc; i++) {
      mod_argv[mod_argc++] = argv[i];
    }
    mod_argv[mod_argc++] = "-o";
    mod_argv[mod_argc++] = derived_s_filename;
    mod_argv[mod_argc] = NULL;
    int ret = zcc_main(mod_argc, mod_argv);
    free(mod_argv);
    return ret;
  }

  int is_out_s = 0;
  if (out_filename) {
    int len = strlen(out_filename);
    if (len > 2 && strcmp(out_filename + len - 2, ".s") == 0) {
      is_out_s = 1;
    }
  }

  if (has_win64_target && (has_stop_at_asm || is_out_s)) {
    char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 1));
    int mod_argc = 0;
    for (i = 0; i < argc; i++) {
      if ((strcmp(argv[i], "-target") == 0 ||
           strcmp(argv[i], "--target") == 0) &&
          i + 1 < argc &&
          (strncmp(argv[i + 1], "win64", 5) == 0 ||
           strncmp(argv[i + 1], "pe64", 4) == 0 ||
           strncmp(argv[i + 1], "windows", 7) == 0)) {
        i++;
        continue;
      } else if (strcmp(argv[i], "--target=win64") == 0 ||
                 strcmp(argv[i], "-target=win64") == 0 ||
                 strcmp(argv[i], "--target=pe64") == 0 ||
                 strcmp(argv[i], "-target=pe64") == 0 ||
                 strcmp(argv[i], "-win64") == 0 ||
                 strcmp(argv[i], "--win64") == 0 ||
                 strcmp(argv[i], "-pe64") == 0 ||
                 strcmp(argv[i], "--pe64") == 0) {
        continue;
      }
      mod_argv[mod_argc++] = argv[i];
    }
    mod_argv[mod_argc] = NULL;
    int ret = zcc_main(mod_argc, mod_argv);
    free(mod_argv);
    return ret;
  }

  /* Win64 PE Direct Assembly Target: assemble .s directly to .exe (single or
   * multiple .s) */
  if (has_win64_target && s_file_count > 0 && c_file_count == 0 &&
      !compile_only && !is_out_s && !has_stop_at_asm) {
    if (!out_filename) {
      out_filename = "a.exe";
    }
    if (s_file_count == 1) {
      int ret = assemble(input_s_files[0], out_filename, NULL, 0, 1);
      if (ret != 0) {
        fprintf(stderr,
                "zcc: win64 direct assembly failed with error code %d\n", ret);
        return ret;
      }
      return 0;
    } else {
      /* Multi-assembly linking with per-unit .L local label scoping */
      char multi_s_path[256];
#ifdef _WIN32
      snprintf(multi_s_path, sizeof(multi_s_path),
               ".tmp_win64_multi_link.s");
#else
      snprintf(multi_s_path, sizeof(multi_s_path),
               ".tmp_win64_multi_link_%d.s", (int)getpid());
#endif
      FILE *mf = fopen(multi_s_path, "wb");
      if (!mf) {
        fprintf(stderr, "zcc: cannot create temp multi-asm file '%s'\n",
                multi_s_path);
        return -1;
      }
      for (i = 0; i < s_file_count; i++) {
        FILE *sf = fopen(input_s_files[i], "rb");
        if (!sf) {
          fprintf(stderr, "zcc: cannot open assembly input '%s'\n",
                  input_s_files[i]);
          fclose(mf);
          remove(multi_s_path);
          return -1;
        }
        char linebuf[65536];
        char unit_tag[32];
        snprintf(unit_tag, sizeof(unit_tag), "__u%d_", i);
        while (fgets(linebuf, sizeof(linebuf), sf)) {
          int in_str = 0;
          int pfx_ok = 1;
          for (size_t c = 0; linebuf[c]; c++) {
            unsigned char ch = (unsigned char)linebuf[c];
            if (in_str) {
              fputc(ch, mf);
              if (ch == '\\' && linebuf[c + 1]) {
                c++;
                fputc(linebuf[c], mf);
              } else if (ch == '"') {
                in_str = 0;
              }
              pfx_ok = 0;
              continue;
            }
            if (ch == '"') {
              in_str = 1;
              fputc(ch, mf);
              pfx_ok = 0;
              continue;
            }
            if (ch == '#') {
              fputs(&linebuf[c], mf);
              break;
            }
            if (pfx_ok && ch == '.' && linebuf[c + 1] == 'L' &&
                (isalnum((unsigned char)linebuf[c + 2]) ||
                 linebuf[c + 2] == '_')) {
              fputs(".L", mf);
              fputs(unit_tag, mf);
              c += 1;
              pfx_ok = 0;
              continue;
            }
            if (i > 0 && pfx_ok &&
                strncmp(&linebuf[c], "platform_main_", 14) == 0 &&
                (strncmp(&linebuf[c + 14], "begin", 5) == 0 ||
                 strncmp(&linebuf[c + 14], "end", 3) == 0)) {
              int kw_len = (linebuf[c + 14] == 'b') ? 19 : 17;
              unsigned char after = (unsigned char)linebuf[c + kw_len];
              if (!isalnum(after) && after != '_') {
                fwrite(&linebuf[c], 1, (size_t)kw_len, mf);
                fprintf(mf, "__u%d", i);
                c += (size_t)(kw_len - 1);
                pfx_ok = 0;
                continue;
              }
            }
            fputc(ch, mf);
            pfx_ok = !(isalnum(ch) || ch == '_' || ch == '.');
          }
        }
        fputc('\n', mf);
        fclose(sf);
      }
      fclose(mf);
      int ret = assemble(multi_s_path, out_filename, NULL, 0, 1);
      remove(multi_s_path);
      if (ret != 0) {
        fprintf(stderr, "zcc: win64 multi-assembly failed with error code %d\n",
                ret);
        return ret;
      }
      return 0;
    }
  }

  /* Zero-Binutils ARM64 / RV64 Direct ELF Assembly from .s inputs */
  if ((has_arm64_target || has_riscv_target) && !has_win64_target &&
      s_file_count > 0 && c_file_count == 0 && !is_out_s && !has_stop_at_asm) {
    size_t total_asm_sz = 0;
    char *multi_asm_buf = NULL;
    for (i = 0; i < s_file_count; i++) {
      FILE *sf = fopen(input_s_files[i], "rb");
      if (!sf) {
        fprintf(stderr, "zcc: cannot open assembly input %s\n", input_s_files[i]);
        if (multi_asm_buf)
          free(multi_asm_buf);
        return 1;
      }
      fseek(sf, 0, SEEK_END);
      long sz = ftell(sf);
      fseek(sf, 0, SEEK_SET);
      if (sz > 0) {
        char *nb = (char *)realloc(multi_asm_buf, total_asm_sz + sz + 2);
        if (nb) {
          multi_asm_buf = nb;
          size_t r = fread(multi_asm_buf + total_asm_sz, 1, sz, sf);
          total_asm_sz += r;
          multi_asm_buf[total_asm_sz++] = '\n';
          multi_asm_buf[total_asm_sz] = '\0';
        }
      }
      fclose(sf);
    }
    const char *target_out =
        out_filename ? out_filename : (compile_only ? "a.o" : "a.out");
    int ret = -1;
    if (compile_only) {
      ret = has_arm64_target
                ? arm64_assemble_to_elf_obj(multi_asm_buf ? multi_asm_buf : "",
                                            total_asm_sz, target_out)
                : riscv_assemble_to_elf_obj(multi_asm_buf ? multi_asm_buf : "",
                                            total_asm_sz, target_out);
    } else {
      ret = has_arm64_target
                ? arm64_assemble_to_elf_exec(multi_asm_buf ? multi_asm_buf : "",
                                             total_asm_sz, target_out)
                : riscv_assemble_to_elf_exec(multi_asm_buf ? multi_asm_buf : "",
                                             total_asm_sz, target_out);
    }
    if (multi_asm_buf)
      free(multi_asm_buf);
    if (ret != 0) {
      fprintf(stderr, "zcc: direct %s ELF assembly failed with error code %d\n",
              has_arm64_target ? "ARM64" : "RISC-V64", ret);
      return ret;
    }
    return 0;
  }

  /* Native ELF Direct Assembly Object Emission: assemble .s directly to .o */
  if (!has_win64_target && s_file_count == 1 && c_file_count == 0 &&
      compile_only && !is_out_s && !has_stop_at_asm) {
    int ret = assemble(input_s_files[0], out_filename ? out_filename : "a.o",
                       NULL, 0, 0);
    if (ret != 0) {
      fprintf(stderr,
              "zcc: native assembly to object failed with error code %d\n",
              ret);
      return ret;
    }
    return 0;
  }

  /* Multi-Source Synthesis for Win64 PE and direct ARM64/RV64 executables */
  char temp_multi_c_filename[256];
  temp_multi_c_filename[0] = '\0';
  if ((has_win64_target || has_arm64_target || has_riscv_target) &&
      c_file_count > 1 && !compile_only && !is_out_s && !has_stop_at_asm) {
#ifdef _WIN32
    snprintf(temp_multi_c_filename, sizeof(temp_multi_c_filename),
             ".tmp_codegen_multi_win64_%s.c", base ? base : "unit");
#else
    snprintf(temp_multi_c_filename, sizeof(temp_multi_c_filename),
             ".tmp_codegen_multi_win64_%d_%s.c", (int)getpid(),
             base ? base : "unit");
#endif
    strncpy(g_temp_multi_c_to_remove, temp_multi_c_filename,
            sizeof(g_temp_multi_c_to_remove) - 1);
    FILE *mf = fopen(temp_multi_c_filename, "w");
    if (!mf) {
      fprintf(stderr,
              "zcc: failed to create temporary multi-source synthesis file\n");
      return -1;
    }
    for (i = 0; i < c_file_count; i++) {
      /* Normalize backslashes to forward slashes for clean preprocessor line
       * directives */
      char clean_path[512];
      size_t pi = 0;
      const char *p_in = input_c_files[i];
      while (*p_in && pi < sizeof(clean_path) - 1) {
        clean_path[pi++] = (*p_in == '\\') ? '/' : *p_in;
        p_in++;
      }
      clean_path[pi] = '\0';
      fprintf(mf, "#line 1 \"%s\"\n", clean_path);

      FILE *src_f = fopen(input_c_files[i], "r");
      if (src_f) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), src_f)) > 0) {
          fwrite(buf, 1, n, mf);
        }
        fclose(src_f);
        fprintf(mf, "\n");
      } else {
        fprintf(stderr, "zcc: error opening input source file %s\n",
                input_c_files[i]);
        fclose(mf);
        remove(temp_multi_c_filename);
        g_temp_multi_c_to_remove[0] = '\0';
        return -1;
      }
    }
    fclose(mf);
    input_c_file = temp_multi_c_filename;
    base = "multi_win64";
  }

  if (is_zcc_c || is_out_s || has_stop_at_asm ||
      (has_lib_flag && !has_explicit_zld) ||
      (c_file_count > 1 && compile_only) || (!input_c_file && obj_count == 0) ||
      has_trace_abi || has_emit_gguf || has_frontend_dump || has_wasm_target ||
      (has_arm_target && !has_arm64_target) || has_yul_target ||
      has_stark_proof_target) {
    if (temp_multi_c_filename[0] != '\0') {
      remove(temp_multi_c_filename);
      g_temp_multi_c_to_remove[0] = '\0';
    }
    int ret = zcc_main(argc, argv);
    if (ret == 0 && has_profile_target) {
      const char *target_s = out_filename;
      if (!target_s && input_c_file) {
        static char s_buf[256];
        strncpy(s_buf, base, sizeof(s_buf) - 3);
        int blen = strlen(s_buf);
        if (blen > 2 && strcmp(s_buf + blen - 2, ".c") == 0)
          s_buf[blen - 2] = '\0';
        strcat(s_buf, ".s");
        target_s = s_buf;
      }
      if (target_s) {
        zcc_hardware_profile_file(target_s, has_profile_json);
      }
    }
    return ret;
  }

  /* Driver integration: assemble/link freestanding statically by default */
  if (input_c_file) {
    /* Generate safe temporary assembly filename in current directory */
    char temp_s_filename[256];
    sprintf(temp_s_filename, ".tmp_codegen_%s.s", base);
    strncpy(g_temp_s_to_remove, temp_s_filename,
            sizeof(g_temp_s_to_remove) - 1);

    if (compile_only) {
      char derived_o_filename[256];
      if (!out_filename) {
        strncpy(derived_o_filename, base, sizeof(derived_o_filename) - 1);
        derived_o_filename[sizeof(derived_o_filename) - 1] = '\0';
        int len = strlen(derived_o_filename);
        if (len > 2 && strcmp(derived_o_filename + len - 2, ".c") == 0) {
          derived_o_filename[len - 2] = '\0';
          strcat(derived_o_filename, has_win64_target ? ".obj" : ".o");
        } else {
          strcat(derived_o_filename, has_win64_target ? ".obj" : ".o");
        }
        out_filename = derived_o_filename;
      }

      /* Construct modified argv for zcc_main to produce temporary assembly */
      char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 4));
      int mod_argc = 0;
      mod_argv[mod_argc++] = argv[0];
      int has_input_in_mod = 0;
      for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) {
          continue;
        } else if (strcmp(argv[i], "-o") == 0) {
          if (i + 1 < argc)
            i++;
          continue;
        } else if (strcmp(argv[i], "-T") == 0) {
          if (i + 1 < argc)
            i++;
          continue;
        } else if (strcmp(argv[i], "-emit-obj") == 0) {
          continue;
        } else if ((strcmp(argv[i], "-target") == 0 ||
                    strcmp(argv[i], "--target") == 0) &&
                   i + 1 < argc &&
                   (strncmp(argv[i + 1], "win64", 5) == 0 ||
                    strncmp(argv[i + 1], "pe64", 4) == 0 ||
                    strncmp(argv[i + 1], "windows", 7) == 0)) {
          i++;
          continue;
        } else if (strcmp(argv[i], "--target=win64") == 0 ||
                   strcmp(argv[i], "-target=win64") == 0 ||
                   strcmp(argv[i], "--target=pe64") == 0 ||
                   strcmp(argv[i], "-target=pe64") == 0 ||
                   strcmp(argv[i], "-win64") == 0 ||
                   strcmp(argv[i], "--win64") == 0 ||
                   strcmp(argv[i], "-pe64") == 0 ||
                   strcmp(argv[i], "--pe64") == 0) {
          continue;
        } else {
          if (input_c_file && strcmp(argv[i], input_c_file) == 0)
            has_input_in_mod = 1;
          mod_argv[mod_argc++] = argv[i];
        }
      }
      if (!has_input_in_mod && input_c_file) {
        mod_argv[mod_argc++] = (char *)input_c_file;
      }
      if (has_win64_abi) {
        mod_argv[mod_argc++] = "-mabi=ms";
      }
      if (g_win64_gui_subsystem) {
        mod_argv[mod_argc++] = "-mwindows";
      }
      if (g_win64_is_shared) {
        mod_argv[mod_argc++] = "-shared";
      }
      mod_argv[mod_argc++] = "-o";
      mod_argv[mod_argc++] = temp_s_filename;
      mod_argv[mod_argc] = NULL;

      /* Enable in-memory assembly stream on POSIX; write temp .s on Windows */
#ifdef _WIN32
      g_use_in_mem_asm = 0;
#else
      g_use_in_mem_asm = 1;
#endif
      g_in_mem_asm_buf = NULL;
      g_in_mem_asm_size = 0;

      int compile_ret = zcc_main(mod_argc, mod_argv);
      free(mod_argv);

      if (compile_ret != 0) {
        fprintf(stderr, "zcc: AST-codegen phase failed\n");
#ifdef _WIN32
        remove(temp_s_filename);
        g_temp_s_to_remove[0] = '\0';
#endif
        if (g_in_mem_asm_buf)
          free(g_in_mem_asm_buf);
        g_in_mem_asm_buf = NULL;
        g_in_mem_asm_size = 0;
        g_use_in_mem_asm = 0;
        return compile_ret;
      }

      int assemble_ret;
      if (has_arm64_target || has_riscv_target) {
#ifdef _WIN32
        FILE *sf = fopen(temp_s_filename, "rb");
        if (sf) {
          fseek(sf, 0, SEEK_END);
          long ssz = ftell(sf);
          fseek(sf, 0, SEEK_SET);
          if (ssz > 0) {
            g_in_mem_asm_buf = (char *)malloc((size_t)ssz + 1);
            if (g_in_mem_asm_buf) {
              g_in_mem_asm_size = fread(g_in_mem_asm_buf, 1, (size_t)ssz, sf);
              g_in_mem_asm_buf[g_in_mem_asm_size] = '\0';
            }
          }
          fclose(sf);
        }
        remove(temp_s_filename);
        g_temp_s_to_remove[0] = '\0';
#endif
        assemble_ret = has_arm64_target
                           ? arm64_assemble_to_elf_obj(
                                 g_in_mem_asm_buf ? g_in_mem_asm_buf : "",
                                 g_in_mem_asm_size, out_filename)
                           : riscv_assemble_to_elf_obj(
                                 g_in_mem_asm_buf ? g_in_mem_asm_buf : "",
                                 g_in_mem_asm_size, out_filename);
      } else {
#ifdef _WIN32
        assemble_ret = assemble(temp_s_filename, out_filename, NULL, 0,
                                has_win64_target);
        remove(temp_s_filename);
        g_temp_s_to_remove[0] = '\0';
#else
        assemble_ret = assemble(NULL, out_filename, g_in_mem_asm_buf,
                                g_in_mem_asm_size, has_win64_target);
#endif
      }
      if (g_in_mem_asm_buf) {
        free(g_in_mem_asm_buf);
        g_in_mem_asm_buf = NULL;
        g_in_mem_asm_size = 0;
      }
      g_use_in_mem_asm = 0;

      if (assemble_ret != 0) {
        fprintf(stderr,
                "zcc: surgical ELF emission failed with error code %d\n",
                assemble_ret);
        return assemble_ret;
      }

      return 0;
    } else {
      /* Compile and Link */
      if (!out_filename) {
        out_filename = has_win64_target ? "a.exe" : "a.out";
      }

      if (!has_win64_target && !has_arm64_target && !has_riscv_target &&
          c_file_count > 1) {
        /* Multi-TU native ELF compilation + zld static linking (zero external
         * gcc/as/ld) */
        const char *link_objs[2048];
        char *gen_o_files[64];
        int link_obj_count = 0;
        int gen_o_count = 0;
        int c_idx;

        for (c_idx = 0; c_idx < c_file_count && c_idx < 64; c_idx++) {
          char unit_s_filename[256];
          char unit_o_filename[256];
          snprintf(unit_s_filename, sizeof(unit_s_filename),
                   ".tmp_codegen_%d_%s.s", c_idx, base);
          snprintf(unit_o_filename, sizeof(unit_o_filename),
                   ".tmp_codegen_%d_%s.o", c_idx, base);

          char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 4));
          int mod_argc = 0;
          mod_argv[mod_argc++] = argv[0];
          for (i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-T") == 0) {
              if (i + 1 < argc)
                i++;
              continue;
            } else if (strcmp(argv[i], "-emit-obj") == 0 ||
                       strcmp(argv[i], "-zld") == 0) {
              continue;
            } else if (argv[i][0] != '-') {
              int alen = strlen(argv[i]);
              if (alen > 2 && (strcmp(argv[i] + alen - 2, ".c") == 0 ||
                               strcmp(argv[i] + alen - 2, ".o") == 0 ||
                               strcmp(argv[i] + alen - 2, ".a") == 0)) {
                continue;
              }
            }
            mod_argv[mod_argc++] = argv[i];
          }
          mod_argv[mod_argc++] = (char *)input_c_files[c_idx];
          mod_argv[mod_argc++] = "-o";
          mod_argv[mod_argc++] = unit_s_filename;
          mod_argv[mod_argc] = NULL;

          g_use_in_mem_asm = 1;
          g_in_mem_asm_buf = NULL;
          g_in_mem_asm_size = 0;

          int compile_ret = zcc_main(mod_argc, mod_argv);
          free(mod_argv);

          if (compile_ret != 0) {
            fprintf(stderr, "zcc: AST-codegen phase failed for %s\n",
                    input_c_files[c_idx]);
            if (g_in_mem_asm_buf)
              free(g_in_mem_asm_buf);
            g_in_mem_asm_buf = NULL;
            g_in_mem_asm_size = 0;
            g_use_in_mem_asm = 0;
            for (i = 0; i < gen_o_count; i++) {
              remove(gen_o_files[i]);
              free(gen_o_files[i]);
            }
            return compile_ret;
          }

          int assemble_ret = assemble(NULL, unit_o_filename, g_in_mem_asm_buf,
                                      g_in_mem_asm_size, 0);
          if (g_in_mem_asm_buf) {
            free(g_in_mem_asm_buf);
            g_in_mem_asm_buf = NULL;
            g_in_mem_asm_size = 0;
          }
          g_use_in_mem_asm = 0;

          if (assemble_ret != 0) {
            fprintf(
                stderr,
                "zcc: surgical ELF emission failed for %s with error code %d\n",
                input_c_files[c_idx], assemble_ret);
            for (i = 0; i < gen_o_count; i++) {
              remove(gen_o_files[i]);
              free(gen_o_files[i]);
            }
            return assemble_ret;
          }

          gen_o_files[gen_o_count] = strdup(unit_o_filename);
          link_objs[link_obj_count++] = gen_o_files[gen_o_count];
          gen_o_count++;
        }

        for (i = 0; i < obj_count; i++) {
          if (link_obj_count < 2048) {
            link_objs[link_obj_count++] = obj_files[i];
          }
        }

        int link_ret = run_zld_with_freestanding_rt(
            link_objs, link_obj_count, out_filename, ld_script);
        for (i = 0; i < gen_o_count; i++) {
          remove(gen_o_files[i]);
          free(gen_o_files[i]);
        }

        if (link_ret != 0) {
          fprintf(stderr, "zcc: static linking failed with error code %d\n",
                  link_ret);
          return link_ret;
        }
        return 0;
      }

      char temp_o_filename[256];
      sprintf(temp_o_filename, ".tmp_codegen_%s.o", base);
      strncpy(g_temp_o_to_remove, temp_o_filename,
              sizeof(g_temp_o_to_remove) - 1);

      /* 1. Compile to temporary assembly (.s) */
      char **mod_argv = (char **)malloc(sizeof(char *) * (argc + 3));
      int mod_argc = 0;
      mod_argv[mod_argc++] = argv[0];
      for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
          if (i + 1 < argc)
            i++;
          continue;
        } else if (strcmp(argv[i], "-T") == 0) {
          if (i + 1 < argc)
            i++;
          continue;
        } else if (strcmp(argv[i], "-emit-obj") == 0) {
          continue;
        } else if ((strcmp(argv[i], "-target") == 0 ||
                    strcmp(argv[i], "--target") == 0) &&
                   i + 1 < argc &&
                   (strncmp(argv[i + 1], "win64", 5) == 0 ||
                    strncmp(argv[i + 1], "pe64", 4) == 0 ||
                    strncmp(argv[i + 1], "windows", 7) == 0)) {
          i++;
          continue;
        } else if (strcmp(argv[i], "--target=win64") == 0 ||
                   strcmp(argv[i], "-target=win64") == 0 ||
                   strcmp(argv[i], "--target=pe64") == 0 ||
                   strcmp(argv[i], "-target=pe64") == 0 ||
                   strcmp(argv[i], "-win64") == 0 ||
                   strcmp(argv[i], "--win64") == 0 ||
                   strcmp(argv[i], "-pe64") == 0 ||
                   strcmp(argv[i], "--pe64") == 0) {
          continue;
        } else {
          if (temp_multi_c_filename[0] != '\0' && argv[i][0] != '-') {
            int len = strlen(argv[i]);
            if (len > 2 && strcmp(argv[i] + len - 2, ".c") == 0) {
              continue;
            }
          }
          mod_argv[mod_argc++] = argv[i];
        }
      }
      if (temp_multi_c_filename[0] != '\0') {
        mod_argv[mod_argc++] = temp_multi_c_filename;
      }
      mod_argv[mod_argc++] = "-o";
      mod_argv[mod_argc++] = temp_s_filename;
      mod_argv[mod_argc] = NULL;

      /* Enable in-memory assembly stream on POSIX; temp file on Windows */
#ifdef _WIN32
      g_use_in_mem_asm = 0;
#else
      g_use_in_mem_asm = 1;
#endif
      g_in_mem_asm_buf = NULL;
      g_in_mem_asm_size = 0;

      int compile_ret = zcc_main(mod_argc, mod_argv);
      free(mod_argv);

      if (compile_ret != 0) {
        if (temp_multi_c_filename[0] != '\0') {
          remove(temp_multi_c_filename);
        }
#ifdef _WIN32
        remove(temp_s_filename);
        g_temp_s_to_remove[0] = '\0';
#endif
        fprintf(stderr, "zcc: AST-codegen phase failed\n");
        if (g_in_mem_asm_buf)
          free(g_in_mem_asm_buf);
        g_in_mem_asm_buf = NULL;
        g_in_mem_asm_size = 0;
        g_use_in_mem_asm = 0;
        return compile_ret;
      }

      if (has_win64_target) {
#ifdef _WIN32
        if (s_file_count > 0) {
          FILE *tf = fopen(temp_s_filename, "ab");
          if (tf) {
            for (i = 0; i < s_file_count; i++) {
              FILE *sf = fopen(input_s_files[i], "rb");
              if (sf) {
                char buf[4096];
                size_t nr;
                fputc('\n', tf);
                while ((nr = fread(buf, 1, sizeof(buf), sf)) > 0) {
                  fwrite(buf, 1, nr, tf);
                }
                fclose(sf);
              }
            }
            fclose(tf);
          }
        }
        int pe_ret = assemble(temp_s_filename, out_filename, NULL, 0, 1);
        remove(temp_s_filename);
        g_temp_s_to_remove[0] = '\0';
#else
        /* Append any auxiliary .s assembly files (mixed C + assembly support)
         */
        if (s_file_count > 0) {
          for (i = 0; i < s_file_count; i++) {
            FILE *sf = fopen(input_s_files[i], "rb");
            if (sf) {
              fseek(sf, 0, SEEK_END);
              long s_sz = ftell(sf);
              fseek(sf, 0, SEEK_SET);
              if (s_sz > 0) {
                char *nb = (char *)realloc(g_in_mem_asm_buf,
                                           g_in_mem_asm_size + s_sz + 2);
                if (nb) {
                  g_in_mem_asm_buf = nb;
                  size_t r =
                      fread(g_in_mem_asm_buf + g_in_mem_asm_size, 1, s_sz, sf);
                  g_in_mem_asm_size += r;
                  g_in_mem_asm_buf[g_in_mem_asm_size++] = '\n';
                  g_in_mem_asm_buf[g_in_mem_asm_size] = '\0';
                }
              }
              fclose(sf);
            }
          }
        }
        int pe_ret = assemble(NULL, out_filename, g_in_mem_asm_buf,
                              g_in_mem_asm_size, 1);
#endif
        if (temp_multi_c_filename[0] != '\0') {
          remove(temp_multi_c_filename);
          g_temp_multi_c_to_remove[0] = '\0';
        }
        if (g_in_mem_asm_buf) {
          free(g_in_mem_asm_buf);
          g_in_mem_asm_buf = NULL;
          g_in_mem_asm_size = 0;
        }
        g_use_in_mem_asm = 0;
        if (pe_ret != 0) {
          fprintf(stderr, "zcc: win64 PE emission failed with error code %d\n",
                  pe_ret);
          return pe_ret;
        }
        return 0;
      }

      if (has_arm64_target || has_riscv_target) {
#ifdef _WIN32
        FILE *sf = fopen(temp_s_filename, "rb");
        if (sf) {
          fseek(sf, 0, SEEK_END);
          long ssz = ftell(sf);
          fseek(sf, 0, SEEK_SET);
          if (ssz > 0) {
            g_in_mem_asm_buf = (char *)malloc((size_t)ssz + 1);
            if (g_in_mem_asm_buf) {
              g_in_mem_asm_size = fread(g_in_mem_asm_buf, 1, (size_t)ssz, sf);
              g_in_mem_asm_buf[g_in_mem_asm_size] = '\0';
            }
          }
          fclose(sf);
        }
        remove(temp_s_filename);
        g_temp_s_to_remove[0] = '\0';
#endif
        int exec_ret =
            has_arm64_target
                ? arm64_assemble_to_elf_exec(
                      g_in_mem_asm_buf ? g_in_mem_asm_buf : "",
                      g_in_mem_asm_size, out_filename)
                : riscv_assemble_to_elf_exec(
                      g_in_mem_asm_buf ? g_in_mem_asm_buf : "",
                      g_in_mem_asm_size, out_filename);
        if (temp_multi_c_filename[0] != '\0') {
          remove(temp_multi_c_filename);
          g_temp_multi_c_to_remove[0] = '\0';
        }
        if (g_in_mem_asm_buf) {
          free(g_in_mem_asm_buf);
          g_in_mem_asm_buf = NULL;
          g_in_mem_asm_size = 0;
        }
        g_use_in_mem_asm = 0;
        if (exec_ret != 0) {
          fprintf(stderr,
                  "zcc: direct %s ELF emission failed with error code %d\n",
                  has_arm64_target ? "ARM64" : "RISC-V64", exec_ret);
          return exec_ret;
        }
        return 0;
      }

      /* 2. Assemble directly to temporary object (.o) in memory (or temp file on Win32) */
#ifdef _WIN32
      int assemble_ret = assemble(temp_s_filename, temp_o_filename, NULL, 0, 0);
      remove(temp_s_filename);
      g_temp_s_to_remove[0] = '\0';
#else
      int assemble_ret = assemble(NULL, temp_o_filename, g_in_mem_asm_buf,
                                  g_in_mem_asm_size, 0);
#endif
      if (g_in_mem_asm_buf) {
        free(g_in_mem_asm_buf);
        g_in_mem_asm_buf = NULL;
        g_in_mem_asm_size = 0;
      }
      g_use_in_mem_asm = 0;

      if (assemble_ret != 0) {
        fprintf(stderr,
                "zcc: surgical ELF emission failed with error code %d\n",
                assemble_ret);
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

      int link_ret = run_zld_with_freestanding_rt(
          link_objs, link_obj_count, out_filename, ld_script);
      remove(temp_o_filename);
      g_temp_o_to_remove[0] = '\0';

      if (link_ret != 0) {
        fprintf(stderr, "zcc: static linking failed with error code %d\n",
                link_ret);
        return link_ret;
      }

      return 0;
    }
  } else if (obj_count > 0) {
    /* Only object files passed: run linker directly */
    if (!out_filename) {
      out_filename = "a.out";
    }
    int link_ret = run_zld_with_freestanding_rt(obj_files, obj_count,
                                                out_filename, ld_script);
    if (link_ret != 0) {
      fprintf(stderr, "zcc: static linking failed with error code %d\n",
              link_ret);
      return link_ret;
    }
    return 0;
  } else {
    /* Pass through standard execution */
    return zcc_main(argc, argv);
  }
}

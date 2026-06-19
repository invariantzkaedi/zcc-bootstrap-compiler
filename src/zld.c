/*
 * zld.c — ZKAEDI Freestanding ELF-64 Static Linker
 * Vector 7: Banish GNU ld — Zero Binutils dependency
 *
 * Replaces: ld -T linker.ld -o zkernel.elf boot.o kmain.o
 * Usage:    zld -T linker.ld -o zkernel.elf boot.o kmain.o
 *
 * Supports:
 *   - ELF-64 relocatable input (.o files)
 *   - linker.ld script subset: ENTRY(), SECTIONS { . = ADDR; INPUT_SECTION }
 *   - Relocations: R_X86_64_64, R_X86_64_32, R_X86_64_32S, R_X86_64_PC32
 *                  R_X86_64_PLT32 (treated as PC32)
 *   - Output: ELF-64 executable with PT_LOAD segments, Multiboot2-bootable
 *
 * Self-hosting constraint: C89 + POSIX. No libc beyond stdio/stdlib/string.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int g_verbose = 0;
static uint8_t *g_attest_data = NULL;
static size_t   g_attest_sz = 0;

/* ── ELF-64 structures (inline — no elf.h dependency) ─────────────────── */

#define ELFMAG0  0x7f
#define ELFMAG1  'E'
#define ELFMAG2  'L'
#define ELFMAG3  'F'
#define ELFCLASS64   2
#define ELFDATA2LSB  1
#define ET_EXEC      2
#define ET_REL       1
#define EM_X86_64    62
#define EV_CURRENT   1
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOTE     7
#define SHT_NOBITS   8  /* BSS */
#define SHF_ALLOC    0x2
#define SHF_EXECINSTR 0x4
#define SHF_WRITE    0x1
#define STB_GLOBAL   1
#define STB_WEAK     2
#define STV_DEFAULT  0
#define SHN_UNDEF    0
#define SHN_ABS      0xfff1
#define SHN_COMMON   0xfff2
#define PT_LOAD      1
#define PT_NOTE      4
#define PF_X         0x1
#define PF_W         0x2
#define PF_R         0x4

/* Relocation types */
#define R_X86_64_NONE    0
#define R_X86_64_64      1
#define R_X86_64_PC32    2
#define R_X86_64_32      10
#define R_X86_64_32S     11
#define R_X86_64_PLT32   4

typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

typedef struct {
    unsigned char e_ident[16];
    Elf64_Half  e_type;
    Elf64_Half  e_machine;
    Elf64_Word  e_version;
    Elf64_Addr  e_entry;
    Elf64_Off   e_phoff;
    Elf64_Off   e_shoff;
    Elf64_Word  e_flags;
    Elf64_Half  e_ehsize;
    Elf64_Half  e_phentsize;
    Elf64_Half  e_phnum;
    Elf64_Half  e_shentsize;
    Elf64_Half  e_shnum;
    Elf64_Half  e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word  sh_name;
    Elf64_Word  sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr  sh_addr;
    Elf64_Off   sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word  sh_link;
    Elf64_Word  sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
} Elf64_Shdr;

typedef struct {
    Elf64_Word  st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half  st_shndx;
    Elf64_Addr  st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

typedef struct {
    Elf64_Addr  r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

typedef struct {
    Elf64_Word  p_type;
    Elf64_Word  p_flags;
    Elf64_Off   p_offset;
    Elf64_Addr  p_vaddr;
    Elf64_Addr  p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} Elf64_Phdr;

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffULL)
#define ELF64_ST_BIND(i) ((i) >> 4)
#define ELF64_ST_TYPE(i) ((i) & 0xf)

/* ── Limits ────────────────────────────────────────────────────────────── */
#define ALIGN_UP(x,a)  (((x) + (a) - 1) & ~((a) - 1))

/* ── Linker script rule ────────────────────────────────────────────────── */
typedef struct {
    char  out_name[64];    /* output section name, e.g. ".text" */
    char  in_pattern[64];  /* input section pattern, e.g. ".text*" or "*" */
    uint64_t addr;         /* base VMA (set by ". = " directive) */
    int   has_addr;
} LdRule;

/* ── Input object ──────────────────────────────────────────────────────── */
typedef struct {
    char     *path;
    uint8_t  *data;
    size_t    size;
    Elf64_Ehdr *ehdr;
    Elf64_Shdr *shdrs;
    char     *shstrtab;
    /* per-section VMA assigned during layout */
    uint64_t *section_vma;  /* [shnum] */
    /* symbol table */
    Elf64_Sym *symtab;
    uint32_t   symcnt;
    char      *strtab;
    uint32_t   symtab_shndx; /* section index of .symtab */
} ObjFile;

/* ── Global symbol table entry ─────────────────────────────────────────── */
typedef struct {
    char      name[128];
    uint64_t  value;
    int       defined;
    int       obj_idx;
    uint32_t  sym_idx;
} GlobalSym;

/* ── Output section accumulator ────────────────────────────────────────── */
typedef struct {
    char      name[64];
    uint64_t  vma;
    uint64_t  lma;          /* same as vma for now */
    uint64_t  size;
    uint64_t  align;
    uint64_t  flags;
    /* raw content */
    uint8_t  *buf;
    uint64_t  buf_size;
    uint64_t  buf_used;
    /* contributions: which (obj,shndx) map to which offset */
    struct { int obj; uint32_t shndx; uint64_t off; } *contrib;
    int contrib_cnt;
    int contrib_cap;
} OutSection;

/* ── Linker state ──────────────────────────────────────────────────────── */
static ObjFile   *g_objs = NULL;
static int       g_objs_cap = 0;
static int       g_nobj = 0;

static GlobalSym *g_syms = NULL;
static int       g_syms_cap = 0;
static int       g_nsym = 0;

static LdRule    *g_rules = NULL;
static int       g_rules_cap = 0;
static int       g_nrules = 0;

static uint64_t  g_entry = 0;
static char      g_entry_name[64] = "_start";
static char      g_output[256] = "a.out";

static OutSection g_out[32];  /* .text .rodata .data .bss */
static int        g_nout = 0;

typedef struct {
    char name[64];
    char val_str[128];
    uint64_t value;
    int resolved;
} LdSymbol;

static LdSymbol *g_ld_syms = NULL;
static int      g_ld_syms_cap = 0;
static int      g_nld_syms = 0;

static OutSection *get_out_section(const char *name, uint64_t flags);

/* ── Utilities ─────────────────────────────────────────────────────────── */
static void die(const char *msg) {
    fprintf(stderr, "zld: fatal: %s\n", msg);
    exit(1);
}

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) die("out of memory");
    return p;
}

static void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (!p) die("out of memory");
    return p;
}

static void *xrealloc(void *ptr, size_t sz) {
    void *p = realloc(ptr, sz);
    if (!p) die("out of memory");
    return p;
}

static void cleanup_linker_state(void) {
    int i;
    if (g_attest_data) {
        free(g_attest_data);
        g_attest_data = NULL;
    }
    g_attest_sz = 0;

    if (g_objs) {
        for (i = 0; i < g_nobj; i++) {
            if (g_objs[i].data) {
                free(g_objs[i].data);
            }
            if (g_objs[i].section_vma) {
                free(g_objs[i].section_vma);
            }
        }
        free(g_objs);
        g_objs = NULL;
    }
    g_nobj = 0;
    g_objs_cap = 0;

    if (g_syms) {
        free(g_syms);
        g_syms = NULL;
    }
    g_nsym = 0;
    g_syms_cap = 0;

    if (g_rules) {
        free(g_rules);
        g_rules = NULL;
    }
    g_nrules = 0;
    g_rules_cap = 0;

    if (g_ld_syms) {
        free(g_ld_syms);
        g_ld_syms = NULL;
    }
    g_nld_syms = 0;
    g_ld_syms_cap = 0;

    for (i = 0; i < g_nout; i++) {
        if (g_out[i].buf) {
            free(g_out[i].buf);
            g_out[i].buf = NULL;
        }
        if (g_out[i].contrib) {
            free(g_out[i].contrib);
            g_out[i].contrib = NULL;
        }
    }
    memset(g_out, 0, sizeof(g_out));
    g_nout = 0;
}

static LdRule *add_rule(void) {
    if (g_nrules >= g_rules_cap) {
        int old_cap = g_rules_cap;
        g_rules_cap = g_rules_cap ? g_rules_cap * 2 : 16;
        g_rules = xrealloc(g_rules, g_rules_cap * sizeof(LdRule));
        memset(g_rules + old_cap, 0, (g_rules_cap - old_cap) * sizeof(LdRule));
    }
    return &g_rules[g_nrules++];
}

static uint8_t *read_file(const char *path, size_t *sz) {
    FILE *f;
    uint8_t *buf;
    f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "zld: cannot open %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END);
    *sz = (size_t)ftell(f);
    fseek(f, 0, 0);
    buf = xmalloc(*sz + 1);
    if (fread(buf, 1, *sz, f) != *sz) die("read error");
    fclose(f);
    buf[*sz] = 0;
    return buf;
}

static const char *sh_name(ObjFile *o, uint32_t idx) {
    return o->shstrtab + o->shdrs[idx].sh_name;
}

/* section name match: pattern may have trailing '*' */
static int sec_match(const char *name, const char *pattern) {
    size_t plen = strlen(pattern);
    if (plen && pattern[plen-1] == '*') {
        return strncmp(name, pattern, plen-1) == 0;
    }
    return strcmp(name, pattern) == 0;
}

/* ── Parse linker script (minimal subset) ──────────────────────────────── */
static void validate_ld_script(const char *src) {
    const char *p = src;
    int brace_level = 0;
    int paren_level = 0;
    int line_num = 1;

    while (*p) {
        if (*p == '\n') {
            line_num++;
        }

        /* skip whitespace and comments */
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') line_num++;
                p++;
            }
            if (*p) p += 2;
            continue;
        }

        /* Track braces and parentheses */
        if (*p == '{') {
            brace_level++;
            p++;
            continue;
        }
        if (*p == '}') {
            brace_level--;
            if (brace_level < 0) {
                fprintf(stderr, "zld: error: linker script line %d: mismatched closing brace '}'\n", line_num);
                exit(1);
            }
            p++;
            continue;
        }
        if (*p == '(') {
            paren_level++;
            p++;
            continue;
        }
        if (*p == ')') {
            paren_level--;
            if (paren_level < 0) {
                fprintf(stderr, "zld: error: linker script line %d: mismatched closing parenthesis ')'\n", line_num);
                exit(1);
            }
            p++;
            continue;
        }

        /* Check for words/identifiers */
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
            const char *start = p;
            while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_') {
                p++;
            }
            size_t len = (size_t)(p - start);
            char word[64];
            if (len >= sizeof(word)) len = sizeof(word) - 1;
            strncpy(word, start, len);
            word[len] = '\0';

            /* If it's a known unsupported directive, warn */
            if (strcmp(word, "OUTPUT_FORMAT") == 0 || strcmp(word, "OUTPUT_ARCH") == 0 ||
                strcmp(word, "PHDRS") == 0 || strcmp(word, "SEARCH_DIR") == 0 ||
                strcmp(word, "ASSERT") == 0 || strcmp(word, "PROVIDE") == 0) {
                fprintf(stderr, "zld: warning: ignoring unsupported linker script directive '%s' at line %d\n", word, line_num);
            }
            continue;
        }

        p++;
    }

    if (brace_level != 0) {
        fprintf(stderr, "zld: error: linker script has mismatched opening braces '{' (nesting depth %d)\n", brace_level);
        exit(1);
    }
    if (paren_level != 0) {
        fprintf(stderr, "zld: error: linker script has mismatched opening parentheses '(' (nesting depth %d)\n", paren_level);
        exit(1);
    }
}

/*
 * Parses:
 *   ENTRY(_start)
 *   SECTIONS {
 *     . = 0x100000;
 *     .text : { *(.text*) *(.multiboot) }
 *     .rodata : { *(.rodata*) }
 *     .data : { *(.data*) }
 *     .bss : { *(.bss*) *(COMMON) }
 *   }
 */
static void parse_ld_script(const char *path) {
    size_t sz;
    char *src = (char *)read_file(path, &sz);
    validate_ld_script(src);
    char *p = src;
    uint64_t cur_addr = 0;
    int in_sections = 0;
    char cur_out[64] = "";
    int has_pending_addr = 0;

    while (*p) {
        /* skip whitespace and comments */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }

        if (strncmp(p, "ENTRY(", 6) == 0) {
            p += 6;
            char *end = strchr(p, ')');
            if (end) {
                size_t len = (size_t)(end - p);
                if (len >= sizeof(g_entry_name)) len = sizeof(g_entry_name)-1;
                strncpy(g_entry_name, p, len);
                g_entry_name[len] = 0;
                p = end + 1;
            }
            continue;
        }

        if (strncmp(p, "KEEP(", 5) == 0) {
            p += 5;
            continue;
        }

        if (strncmp(p, "SECTIONS", 8) == 0) {
            in_sections = 1; p += 8;
            continue;
        }

        if (!in_sections) { p++; continue; }

        /* ". = ADDR;" or ". = ALIGN(ADDR);" */
        if (p[0] == '.' && p[1] == ' ' && p[2] == '=') {
            p += 3;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "ALIGN(", 6) == 0) {
                p += 6;
                uint64_t align_val = (uint64_t)strtoull(p, &p, 0);
                if (*p == 'K' || *p == 'k') { align_val *= 1024; p++; }
                else if (*p == 'M' || *p == 'm') { align_val *= 1024 * 1024; p++; }
                cur_addr = ALIGN_UP(cur_addr, align_val);
                while (*p && *p != ')') p++;
                if (*p) p++;
            } else {
                cur_addr = (uint64_t)strtoull(p, &p, 0);
                if (*p == 'M' || *p == 'm') { cur_addr *= 1024 * 1024; p++; }
                else if (*p == 'K' || *p == 'k') { cur_addr *= 1024; p++; }
                else if (*p == 'G' || *p == 'g') { cur_addr *= 1024 * 1024 * 1024; p++; }
            }
            while (*p && *p != ';') p++;
            if (*p) p++;
            has_pending_addr = 1;
            continue;
        }

        /* ".secname : {" or ".secname : ALIGN(...) {" */
        if (p[0] == '.' && (p[1] == 't' || p[1] == 'r' || p[1] == 'd' || p[1] == 'b' || p[1] == 'n')) {
            char *start = p;
            while (*p && *p != ':' && *p != '\n') p++;
            if (*p == ':') {
                /* extract section name */
                char tmp[64]; int len = (int)(p - start);
                if (len >= 64) len = 63;
                strncpy(tmp, start, len); tmp[len] = 0;
                /* trim trailing space */
                int i = len-1;
                while (i >= 0 && (tmp[i] == ' ' || tmp[i] == '\t')) tmp[i--] = 0;
                strncpy(cur_out, tmp, sizeof(cur_out)-1);
                p++; /* skip ':' */

                /* Parse section alignment if present (e.g. ALIGN(4K) before '{') */
                char *brace = strchr(p, '{');
                if (brace) {
                    char *align_ptr = strstr(p, "ALIGN(");
                    if (align_ptr && align_ptr < brace) {
                        align_ptr += 6;
                        uint64_t align_val = (uint64_t)strtoull(align_ptr, &align_ptr, 0);
                        if (*align_ptr == 'K' || *align_ptr == 'k') align_val *= 1024;
                        else if (*align_ptr == 'M' || *align_ptr == 'm') align_val *= 1024 * 1024;
                        
                        /* Save this alignment for cur_out */
                        OutSection *os = get_out_section(cur_out, SHF_ALLOC);
                        os->align = align_val;
                        if (g_verbose) {
                            printf("zld: parsed section '%s' explicit alignment: %llu\n", cur_out, (unsigned long long)align_val);
                        }
                    }
                }
                continue;
            } else {
                p = start + 1;
                continue;
            }
        }

        /* "symbol_name = ...;" inside SECTIONS block */
        if (in_sections && p[0] != '.' && ((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z') || p[0] == '_')) {
            char *start = p;
            while (*p && *p != '=' && *p != ';' && *p != '\n') p++;
            if (*p == '=') {
                char sym_name[64];
                int len = (int)(p - start);
                if (len >= 64) len = 63;
                strncpy(sym_name, start, len); sym_name[len] = 0;
                /* trim spaces */
                int i = len-1;
                while (i >= 0 && (sym_name[i] == ' ' || sym_name[i] == '\t')) sym_name[i--] = 0;
                
                p++; /* skip '=' */
                while (*p == ' ' || *p == '\t') p++;
                char expr_buf[128];
                char *expr_start = p;
                while (*p && *p != ';') p++;
                int expr_len = (int)(p - expr_start);
                if (expr_len >= 128) expr_len = 127;
                strncpy(expr_buf, expr_start, expr_len); expr_buf[expr_len] = 0;
                /* trim spaces */
                i = expr_len-1;
                while (i >= 0 && (expr_buf[i] == ' ' || expr_buf[i] == '\t' || expr_buf[i] == '\r' || expr_buf[i] == '\n')) expr_buf[i--] = 0;
                
                /* Register this custom symbol */
                if (g_nld_syms >= g_ld_syms_cap) {
                    int old_cap = g_ld_syms_cap;
                    g_ld_syms_cap = g_ld_syms_cap ? g_ld_syms_cap * 2 : 16;
                    g_ld_syms = xrealloc(g_ld_syms, g_ld_syms_cap * sizeof(LdSymbol));
                    memset(g_ld_syms + old_cap, 0, (g_ld_syms_cap - old_cap) * sizeof(LdSymbol));
                }
                LdSymbol *ls = &g_ld_syms[g_nld_syms++];
                strncpy(ls->name, sym_name, sizeof(ls->name)-1);
                strncpy(ls->val_str, expr_buf, sizeof(ls->val_str)-1);
                ls->resolved = 0;
                ls->value = 0;
                if (g_verbose) {
                    printf("zld: registered linker script symbol '%s' = '%s'\n", ls->name, ls->val_str);
                }
                if (*p) p++;
                continue;
            } else {
                p = start + 1;
                continue;
            }
        }

        if (*p == '}') {
            has_pending_addr = 0;
            p++;
            continue;
        }

        /* "*(.pattern)" or "*(.pat*)" inside section block */
        if (strncmp(p, "KEEP(", 5) == 0) {
            p += 5;
        }
        if (p[0] == '*' && p[1] == '(') {
            p += 2;
            char *end = strchr(p, ')');
            if (end && cur_out[0]) {
                /* may contain multiple space-separated patterns */
                char pat_buf[256]; size_t plen = (size_t)(end - p);
                if (plen >= sizeof(pat_buf)) plen = sizeof(pat_buf)-1;
                strncpy(pat_buf, p, plen); pat_buf[plen] = 0;
                /* tokenize by space */
                char *tok = strtok(pat_buf, " \t");
                while (tok) {
                    /* strip leading dot from *(./pat) */
                    if (tok[0] == '.') {
                        /* e.g. ".text*" */
                        LdRule *r = add_rule();
                        strncpy(r->out_name, cur_out, sizeof(r->out_name)-1);
                        strncpy(r->in_pattern, tok, sizeof(r->in_pattern)-1);
                        r->addr = has_pending_addr ? cur_addr : 0;
                        r->has_addr = has_pending_addr;
                    } else if (strcmp(tok, "COMMON") == 0) {
                        /* map COMMON to .bss */
                        LdRule *r = add_rule();
                        strncpy(r->out_name, cur_out, sizeof(r->out_name)-1);
                        strncpy(r->in_pattern, "COMMON", sizeof(r->in_pattern)-1);
                        r->addr = has_pending_addr ? cur_addr : 0;
                        r->has_addr = has_pending_addr;
                    }
                    tok = strtok(NULL, " \t");
                }
                p = end + 1;
            } else if (end) {
                p = end + 1;
            } else {
                p++;
            }
            continue;
        }

        p++;
    }
    free(src);
}

/* ── Load and parse one .o file ────────────────────────────────────────── */
static void load_obj(const char *path, int idx) {
    ObjFile *o = &g_objs[idx];
    o->path = (char *)path;
    o->data = read_file(path, &o->size);

    if (o->size < sizeof(Elf64_Ehdr)) die("input too small for ELF header");
    o->ehdr = (Elf64_Ehdr *)o->data;

    if (o->ehdr->e_ident[0] != ELFMAG0 || o->ehdr->e_ident[1] != ELFMAG1 ||
        o->ehdr->e_ident[2] != ELFMAG2 || o->ehdr->e_ident[3] != ELFMAG3)
        die("not an ELF file");
    if (o->ehdr->e_ident[4] != ELFCLASS64) die("not ELF-64");
    if (o->ehdr->e_type != ET_REL) die("input is not a relocatable object");
    if (o->ehdr->e_machine != EM_X86_64) die("not x86-64");

    o->shdrs = (Elf64_Shdr *)(o->data + o->ehdr->e_shoff);
    o->shstrtab = (char *)(o->data + o->shdrs[o->ehdr->e_shstrndx].sh_offset);

    o->section_vma = xcalloc(o->ehdr->e_shnum, sizeof(uint64_t));

    /* find symtab */
    uint32_t i;
    for (i = 0; i < o->ehdr->e_shnum; i++) {
        if (o->shdrs[i].sh_type == SHT_SYMTAB) {
            o->symtab_shndx = i;
            o->symtab = (Elf64_Sym *)(o->data + o->shdrs[i].sh_offset);
            o->symcnt = (uint32_t)(o->shdrs[i].sh_size / sizeof(Elf64_Sym));
            o->strtab = (char *)(o->data + o->shdrs[o->shdrs[i].sh_link].sh_offset);
            break;
        }
    }
}

/* ── Find or create output section ─────────────────────────────────────── */
static OutSection *get_out_section(const char *name, uint64_t flags) {
    int i;
    for (i = 0; i < g_nout; i++) {
        if (strcmp(g_out[i].name, name) == 0) {
            g_out[i].flags |= flags;
            return &g_out[i];
        }
    }
    if (g_nout >= 32) die("too many output sections");
    OutSection *s = &g_out[g_nout++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name)-1);
    s->flags = flags;
    s->align = 16;
    s->buf_size = 1024 * 1024;  /* 1MB initial */
    s->buf = xcalloc(1, s->buf_size);
    return s;
}

static void outsec_append(OutSection *s, const uint8_t *data, uint64_t len, uint64_t align) {
    s->buf_used = ALIGN_UP(s->buf_used, align);
    if (s->buf_used + len > s->buf_size) {
        s->buf_size = (s->buf_used + len) * 2;
        s->buf = realloc(s->buf, s->buf_size);
        if (!s->buf) die("realloc failed");
        memset(s->buf + s->buf_used, 0, s->buf_size - s->buf_used);
    }
    if (data) memcpy(s->buf + s->buf_used, data, len);
    s->buf_used += len;
}

/* ── Match input section to output section name ─────────────────────────── */
static const char *match_section(const char *secname, int is_common) {
    int i;
    for (i = 0; i < g_nrules; i++) {
        if (is_common && strcmp(g_rules[i].in_pattern, "COMMON") == 0)
            return g_rules[i].out_name;
        if (!is_common && sec_match(secname, g_rules[i].in_pattern))
            return g_rules[i].out_name;
    }
    /* fallback: hardcoded defaults */
    if (strcmp(secname, ".note.zcc.tensor") == 0) return ".note.zcc.tensor";
    if (strcmp(secname, ".zcc_tensor_attest") == 0) return ".zcc_tensor_attest";
    if (strncmp(secname, ".text", 5) == 0 || strncmp(secname, ".multiboot", 10) == 0)
        return ".text";
    if (strncmp(secname, ".rodata", 7) == 0) return ".rodata";
    if (strncmp(secname, ".note", 5) == 0)   return ".note";
    if (strncmp(secname, ".data", 5) == 0)   return ".data";
    if (strncmp(secname, ".bss", 4) == 0 || is_common) return ".bss";
    if (strncmp(secname, ".debug_", 7) == 0) return secname;
    return NULL; /* discard */
}

/* ── Section VMA assignment (linker script) ─────────────────────────────── */
static uint64_t get_section_base(const char *out_name) {
    int i;
    for (i = 0; i < g_nrules; i++)
        if (strcmp(g_rules[i].out_name, out_name) == 0 && g_rules[i].has_addr)
            return g_rules[i].addr;
    return 0;
}

static GlobalSym *find_sym(const char *name);
static GlobalSym *add_sym(const char *name);

/* ── Layout pass: assign VMAs to all input sections ─────────────────────── */
static void layout(void) {
    static const char *out_order[] = { ".text", ".rodata", ".note", ".note.zcc.tensor", ".zcc_tensor_attest", ".data", ".bss", NULL };
    int oi;
    int max_shnum = 0;
    int i;
    for (i = 0; i < g_nobj; i++) {
        if (g_objs[i].ehdr->e_shnum > max_shnum) {
            max_shnum = g_objs[i].ehdr->e_shnum;
        }
    }
    int stride = max_shnum > 0 ? max_shnum : 1;
    char *laid_out = xcalloc(g_nobj * stride, sizeof(char));

    for (oi = 0; out_order[oi]; oi++) {
        const char *oname = out_order[oi];
        uint64_t base = get_section_base(oname);
        /* If no rule set a base, we continue from the end of the previous output section */
        if (base == 0) {
            int prev_idx;
            uint64_t prev_end = 0x100000;
            for (prev_idx = 0; prev_idx < oi; prev_idx++) {
                OutSection *prev_sec = get_out_section(out_order[prev_idx], SHF_ALLOC);
                if (prev_sec->size > 0 && prev_sec->vma + prev_sec->size > prev_end) {
                    prev_end = prev_sec->vma + prev_sec->size;
                }
            }
            OutSection *sec = get_out_section(oname, SHF_ALLOC);
            uint64_t sec_align = sec->align ? sec->align : 16;
            /* If we transition from RX to RW (e.g. to .data/.bss), force page alignment for segment boundary safety.
             * Note: This dynamic adjustment applies only to implicit layout (when base == 0).
             * Explicit linker script directives (e.g. '. = ADDR;') bypass this and are respected exactly as specified. */
            if (strcmp(oname, ".data") == 0 || strcmp(oname, ".bss") == 0) {
                if (sec_align < 0x1000) sec_align = 0x1000;
            }
            base = ALIGN_UP(prev_end, sec_align);
        }
        
        {
            uint64_t original_base = base;
            uint64_t cursor;
            int r_idx;
            int obj_i;
            uint64_t common_cursor = base;
            int obj_i_c;

            if (strcmp(oname, ".bss") == 0) {
                /* Allocate COMMON symbols at the start of .bss */
                for (obj_i_c = 0; obj_i_c < g_nobj; obj_i_c++) {
                    ObjFile *o = &g_objs[obj_i_c];
                    uint32_t si;
                    if (!o->symtab) continue;
                    for (si = 0; si < o->symcnt; si++) {
                        Elf64_Sym *sym = &o->symtab[si];
                        int bind = ELF64_ST_BIND(sym->st_info);
                        if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
                        if (sym->st_shndx != SHN_COMMON) continue;

                        const char *name = o->strtab + sym->st_name;
                        if (!name[0]) continue;

                        GlobalSym *gs = find_sym(name);
                        if (!gs) gs = add_sym(name);
                        if (gs->defined) continue;

                        {
                            uint64_t align = sym->st_value ? sym->st_value : 1;
                            common_cursor = ALIGN_UP(common_cursor, align);

                            gs->value = common_cursor;
                            gs->defined = 1;
                            gs->obj_idx = obj_i_c;
                            gs->sym_idx = si;

                            if (g_verbose) {
                                printf("zld: allocated COMMON symbol '%s' in .bss at VMA 0x%llx (size %llu, align %llu)\n",
                                       name, (unsigned long long)common_cursor,
                                       (unsigned long long)sym->st_size, (unsigned long long)align);
                            }

                            common_cursor += sym->st_size;
                        }
                    }
                }
                base = common_cursor;
            }
            cursor = base;

            /* 1. Lay out sections matching rules in order of linker script */
            for (r_idx = 0; r_idx < g_nrules; r_idx++) {
                LdRule *rule = &g_rules[r_idx];
                if (strcmp(rule->out_name, oname) != 0) continue;

                for (obj_i = 0; obj_i < g_nobj; obj_i++) {
                    ObjFile *o = &g_objs[obj_i];
                    uint32_t si;
                    for (si = 0; si < o->ehdr->e_shnum; si++) {
                        Elf64_Shdr *sh = &o->shdrs[si];
                        if (sh->sh_type == SHT_NULL) continue;
                        if (laid_out[obj_i * stride + si]) continue;

                        /* Also allow non-alloc sections to be laid out if explicitly requested in linker script */
                        if (!(sh->sh_flags & SHF_ALLOC)) {
                            const char *sname = sh_name(o, si);
                            if (!sec_match(sname, rule->in_pattern)) continue;
                        }

                        {
                            const char *sname = sh_name(o, si);
                            if (sec_match(sname, rule->in_pattern)) {
                                uint64_t align = sh->sh_addralign ? sh->sh_addralign : 1;
                                cursor = ALIGN_UP(cursor, align);
                                o->section_vma[si] = cursor;
                                cursor += sh->sh_size;
                                laid_out[obj_i * stride + si] = 1;

                                /* record contribution */
                                uint64_t f = sh->sh_flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR);
                                OutSection *out = get_out_section(oname, f);
                                if (out->contrib_cnt >= out->contrib_cap) {
                                    int old_cap = out->contrib_cap;
                                    out->contrib_cap = out->contrib_cap ? out->contrib_cap * 2 : 16;
                                    out->contrib = xrealloc(out->contrib, out->contrib_cap * sizeof(*out->contrib));
                                    memset(out->contrib + old_cap, 0, (out->contrib_cap - old_cap) * sizeof(*out->contrib));
                                }
                                out->contrib[out->contrib_cnt].obj   = obj_i;
                                out->contrib[out->contrib_cnt].shndx = si;
                                out->contrib[out->contrib_cnt].off   = o->section_vma[si] - original_base;
                                out->contrib_cnt++;
                                if (align > out->align) out->align = align;
                            }
                        }
                    }
                }
            }

            /* 2. Fallback: Lay out any unallocated sections matching hardcoded defaults */
            for (obj_i = 0; obj_i < g_nobj; obj_i++) {
                ObjFile *o = &g_objs[obj_i];
                uint32_t si;
                for (si = 0; si < o->ehdr->e_shnum; si++) {
                    Elf64_Shdr *sh = &o->shdrs[si];
                    if (!(sh->sh_flags & SHF_ALLOC)) continue;
                    if (sh->sh_type == SHT_NULL) continue;
                    if (laid_out[obj_i * stride + si]) continue;

                    const char *sname = sh_name(o, si);
                    const char *mapped = match_section(sname, 0);
                    if (!mapped || strcmp(mapped, oname) != 0) continue;

                    {
                        uint64_t align = sh->sh_addralign ? sh->sh_addralign : 1;
                        cursor = ALIGN_UP(cursor, align);
                        o->section_vma[si] = cursor;
                        cursor += sh->sh_size;
                        laid_out[obj_i * stride + si] = 1;

                        /* record contribution */
                        uint64_t f = sh->sh_flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR);
                        OutSection *out = get_out_section(oname, f);
                        if (out->contrib_cnt >= out->contrib_cap) {
                            int old_cap = out->contrib_cap;
                            out->contrib_cap = out->contrib_cap ? out->contrib_cap * 2 : 16;
                            out->contrib = xrealloc(out->contrib, out->contrib_cap * sizeof(*out->contrib));
                            memset(out->contrib + old_cap, 0, (out->contrib_cap - old_cap) * sizeof(*out->contrib));
                        }
                        out->contrib[out->contrib_cnt].obj   = obj_i;
                        out->contrib[out->contrib_cnt].shndx = si;
                        out->contrib[out->contrib_cnt].off   = o->section_vma[si] - original_base;
                        out->contrib_cnt++;
                        if (align > out->align) out->align = align;
                    }
                }
            }

            /* finalize output section VMA and size */
            uint64_t final_flags = SHF_ALLOC;
            if (strcmp(oname, ".bss") == 0 || strcmp(oname, ".data") == 0) final_flags |= SHF_WRITE;
            if (strcmp(oname, ".text") == 0) final_flags |= SHF_EXECINSTR;
            OutSection *out = get_out_section(oname, final_flags);
            out->vma = original_base;
            out->lma = original_base;
            out->size = cursor - original_base;

            if (strcmp(oname, ".note.zcc.tensor") == 0 && g_attest_data) {
                cursor = ALIGN_UP(cursor, 4);
                out->vma = cursor;
                out->lma = cursor;
                out->align = 4;
                cursor += 88;
                out->size = 88;
            }
            if (strcmp(oname, ".zcc_tensor_attest") == 0 && g_attest_data) {
                cursor = ALIGN_UP(cursor, 32);
                out->vma = cursor;
                out->lma = cursor;
                out->align = 32;
                cursor += g_attest_sz;
                out->size = g_attest_sz;
            }
        }
    }

    /* 3. Collect and lay out non-alloc sections (debug sections, etc.) */
    {
        char debug_secs[32][64];
        int debug_sec_cnt = 0;
        int k, dbg_obj_i;

        for (dbg_obj_i = 0; dbg_obj_i < g_nobj; dbg_obj_i++) {
            ObjFile *o = &g_objs[dbg_obj_i];
            uint32_t si;
            for (si = 0; si < o->ehdr->e_shnum; si++) {
                Elf64_Shdr *sh = &o->shdrs[si];
                if (!(sh->sh_flags & SHF_ALLOC) && sh->sh_type != SHT_NULL) {
                    const char *sname = sh_name(o, si);
                    if (strncmp(sname, ".debug_", 7) == 0) {
                        int found = 0;
                        for (k = 0; k < debug_sec_cnt; k++) {
                            if (strcmp(debug_secs[k], sname) == 0) {
                                found = 1;
                                break;
                            }
                        }
                        if (!found && debug_sec_cnt < 32) {
                            strncpy(debug_secs[debug_sec_cnt], sname, 63);
                            debug_secs[debug_sec_cnt][63] = '\0';
                            debug_sec_cnt++;
                        }
                    }
                }
            }
        }

        for (k = 0; k < debug_sec_cnt; k++) {
            const char *oname = debug_secs[k];
            uint64_t base = 0;
            int prev_s;
            uint64_t prev_end = 0x100000;
            for (prev_s = 0; prev_s < g_nout; prev_s++) {
                if (g_out[prev_s].size > 0 && g_out[prev_s].vma + g_out[prev_s].size > prev_end) {
                    prev_end = g_out[prev_s].vma + g_out[prev_s].size;
                }
            }
            OutSection *sec = get_out_section(oname, 0);
            uint64_t sec_align = sec->align ? sec->align : 8;
            base = ALIGN_UP(prev_end, sec_align);

            {
                uint64_t original_base = base;
                uint64_t cursor = base;

                for (dbg_obj_i = 0; dbg_obj_i < g_nobj; dbg_obj_i++) {
                    ObjFile *o = &g_objs[dbg_obj_i];
                    uint32_t si;
                    for (si = 0; si < o->ehdr->e_shnum; si++) {
                        Elf64_Shdr *sh = &o->shdrs[si];
                        if (sh->sh_type == SHT_NULL) continue;
                        if (laid_out[dbg_obj_i * stride + si]) continue;

                        const char *sname = sh_name(o, si);
                        if (strcmp(sname, oname) == 0) {
                            uint64_t align = sh->sh_addralign ? sh->sh_addralign : 1;
                            cursor = ALIGN_UP(cursor, align);
                            o->section_vma[si] = cursor;
                            cursor += sh->sh_size;
                            laid_out[dbg_obj_i * stride + si] = 1;

                            OutSection *out = get_out_section(oname, 0);
                            if (out->contrib_cnt >= out->contrib_cap) {
                                int old_cap = out->contrib_cap;
                                out->contrib_cap = out->contrib_cap ? out->contrib_cap * 2 : 16;
                                out->contrib = xrealloc(out->contrib, out->contrib_cap * sizeof(*out->contrib));
                                memset(out->contrib + old_cap, 0, (out->contrib_cap - old_cap) * sizeof(*out->contrib));
                            }
                            out->contrib[out->contrib_cnt].obj   = dbg_obj_i;
                            out->contrib[out->contrib_cnt].shndx = si;
                            out->contrib[out->contrib_cnt].off   = o->section_vma[si] - original_base;
                            out->contrib_cnt++;
                            if (align > out->align) out->align = align;
                        }
                    }
                }
                {
                    OutSection *out = get_out_section(oname, 0);
                    out->vma = original_base;
                    out->lma = original_base;
                    out->size = cursor - original_base;
                }
            }
        }
    }

    free(laid_out);
}

/* ── Resolve global symbols ─────────────────────────────────────────────── */
static GlobalSym *find_sym(const char *name) {
    int i;
    for (i = 0; i < g_nsym; i++)
        if (strcmp(g_syms[i].name, name) == 0) return &g_syms[i];
    return NULL;
}

static GlobalSym *add_sym(const char *name) {
    if (g_nsym >= g_syms_cap) {
        int old_cap = g_syms_cap;
        g_syms_cap = g_syms_cap ? g_syms_cap * 2 : 256;
        g_syms = xrealloc(g_syms, g_syms_cap * sizeof(GlobalSym));
        memset(g_syms + old_cap, 0, (g_syms_cap - old_cap) * sizeof(GlobalSym));
    }
    GlobalSym *s = &g_syms[g_nsym++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name)-1);
    return s;
}

static void collect_symbols(void) {
    int oi, i;

    /* Pre-collect undefined __start_ and __stop_ symbols so they are registered in g_syms */
    for (oi = 0; oi < g_nobj; oi++) {
        ObjFile *o = &g_objs[oi];
        uint32_t si;
        if (!o->symtab) continue;
        for (si = 0; si < o->symcnt; si++) {
            Elf64_Sym *sym = &o->symtab[si];
            int bind = ELF64_ST_BIND(sym->st_info);
            if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
            if (sym->st_shndx != SHN_UNDEF) continue;

            const char *name = o->strtab + sym->st_name;
            if (strncmp(name, "__start_", 8) == 0 || strncmp(name, "__stop_", 7) == 0) {
                GlobalSym *gs = find_sym(name);
                if (!gs) {
                    add_sym(name);
                }
            }
        }
    }

    /* Standard symbol collection pass */
    for (oi = 0; oi < g_nobj; oi++) {
        ObjFile *o = &g_objs[oi];
        uint32_t si;
        if (!o->symtab) continue;
        for (si = 0; si < o->symcnt; si++) {
            Elf64_Sym *sym = &o->symtab[si];
            int bind = ELF64_ST_BIND(sym->st_info);
            if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
            if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_COMMON) continue;

            const char *name = o->strtab + sym->st_name;
            if (!name[0]) continue;

            GlobalSym *gs = find_sym(name);
            if (!gs) gs = add_sym(name);
            if (gs->defined && bind != STB_WEAK) {
                /* allow weak override — skip */
            }

            {
                uint64_t val = sym->st_value;
                if (sym->st_shndx == SHN_ABS) {
                    /* absolute symbol value */
                } else if (sym->st_shndx < o->ehdr->e_shnum) {
                    /* relocate symbol value to VMA */
                    val += o->section_vma[sym->st_shndx];
                }
                gs->value   = val;
                gs->defined = 1;
                gs->obj_idx = oi;
                gs->sym_idx = si;
            }
        }
    }

    /* resolve custom linker script symbols */
    {
        uint64_t end_va = 0x100000;
        int s;
        int j;
        for (s = 0; s < g_nout; s++) {
            if (g_out[s].vma + g_out[s].size > end_va) {
                end_va = g_out[s].vma + g_out[s].size;
            }
        }
        
        for (j = 0; j < g_nld_syms; j++) {
            LdSymbol *ls = &g_ld_syms[j];
            uint64_t val = 0;
            if (strcmp(ls->val_str, ".") == 0) {
                val = end_va;
            } else if (strncmp(ls->val_str, "ALIGN(", 6) == 0) {
                char *ptr = ls->val_str + 6;
                uint64_t align_val = (uint64_t)strtoull(ptr, &ptr, 0);
                if (*ptr == 'K' || *ptr == 'k') align_val *= 1024;
                else if (*ptr == 'M' || *ptr == 'm') align_val *= 1024 * 1024;
                val = ALIGN_UP(end_va, align_val);
            } else {
                val = (uint64_t)strtoull(ls->val_str, NULL, 0);
            }
            
            GlobalSym *gs = find_sym(ls->name);
            if (!gs) gs = add_sym(ls->name);
            gs->value = val;
            gs->defined = 1;
            if (g_verbose) {
                printf("zld: resolved linker script symbol '%s' = 0x%llx\n", gs->name, (unsigned long long)val);
            }
        }
    }

    /* resolve __start_ / __stop_ section symbols */
    for (i = 0; i < g_nsym; i++) {
        GlobalSym *gs = &g_syms[i];
        if (!gs->defined) {
            if (strncmp(gs->name, "__start_", 8) == 0) {
                const char *sec_name = gs->name + 8;
                int s;
                for (s = 0; s < g_nout; s++) {
                    const char *out_name = g_out[s].name;
                    int match = 0;
                    if (strcmp(out_name, sec_name) == 0) match = 1;
                    else if (out_name[0] == '.' && strcmp(out_name + 1, sec_name) == 0) match = 1;
                    else if (out_name[0] == '.' && sec_name[0] == '_' && strcmp(out_name + 1, sec_name + 1) == 0) match = 1;
                    
                    if (match) {
                        gs->value = g_out[s].vma;
                        gs->defined = 1;
                        if (g_verbose) {
                            printf("zld: resolved section start symbol '%s' = 0x%llx\n", gs->name, (unsigned long long)gs->value);
                        }
                        break;
                    }
                }
            } else if (strncmp(gs->name, "__stop_", 7) == 0) {
                const char *sec_name = gs->name + 7;
                int s;
                for (s = 0; s < g_nout; s++) {
                    const char *out_name = g_out[s].name;
                    int match = 0;
                    if (strcmp(out_name, sec_name) == 0) match = 1;
                    else if (out_name[0] == '.' && strcmp(out_name + 1, sec_name) == 0) match = 1;
                    else if (out_name[0] == '.' && sec_name[0] == '_' && strcmp(out_name + 1, sec_name + 1) == 0) match = 1;
                    
                    if (match) {
                        gs->value = g_out[s].vma + g_out[s].size;
                        gs->defined = 1;
                        if (g_verbose) {
                            printf("zld: resolved section stop symbol '%s' = 0x%llx\n", gs->name, (unsigned long long)gs->value);
                        }
                        break;
                    }
                }
            }
        }
    }

    /* resolve entry point */
    GlobalSym *entry_sym = find_sym(g_entry_name);
    if (entry_sym && entry_sym->defined) {
        g_entry = entry_sym->value;
    } else {
        /* fallback: base of .text */
        OutSection *text = NULL;
        int i;
        for (i = 0; i < g_nout; i++)
            if (strcmp(g_out[i].name, ".text") == 0) { text = &g_out[i]; break; }
        if (text) g_entry = text->vma;
        fprintf(stderr, "zld: warning: entry symbol '%s' not found, using 0x%llx\n",
                g_entry_name, (unsigned long long)g_entry);
    }
}

/* ── Copy section data into output buffers ──────────────────────────────── */
static void copy_sections(void) {
    int i, c;
    for (i = 0; i < g_nout; i++) {
        OutSection *out = &g_out[i];
        out->buf_used = 0;

        for (c = 0; c < out->contrib_cnt; c++) {
            int obj_idx = out->contrib[c].obj;
            uint32_t shndx = out->contrib[c].shndx;
            uint64_t off = out->contrib[c].off;

            ObjFile *o = &g_objs[obj_idx];
            Elf64_Shdr *sh = &o->shdrs[shndx];

            /* Pad to expected contribution offset */
            if (out->buf_used < off) {
                outsec_append(out, NULL, off - out->buf_used, 1);
            }

            if (sh->sh_type == SHT_NOBITS) {
                /* BSS — zero fill */
                outsec_append(out, NULL, sh->sh_size, 1);
            } else {
                const uint8_t *sec_data = o->data + sh->sh_offset;
                outsec_append(out, sec_data, sh->sh_size, 1);
            }
        }
    }

    if (g_attest_data) {
        /* 1. Append custom note to .note.zcc.tensor section */
        OutSection *note_out = get_out_section(".note.zcc.tensor", SHF_ALLOC);
        uint64_t aligned_used = ALIGN_UP(note_out->buf_used, 4);
        if (aligned_used > note_out->buf_used) {
            outsec_append(note_out, NULL, aligned_used - note_out->buf_used, 4);
        }
        
        {
            uint32_t namesz = 4;
            uint32_t descsz = 72;
            uint32_t type = 0x7cc;
            char name[4] = "ZCC";
            
            uint8_t desc[72];
            memset(desc, 0, 72);
            /* desc contains manifest_sha256 (32) + gguf_sha256 (32) + record_count (4) + policy_version (4) */
            memcpy(desc, g_attest_data + 16, 32);       /* manifest_sha256 */
            memcpy(desc + 32, g_attest_data + 48, 32);  /* gguf_sha256 */
            uint32_t record_count = *(uint32_t *)(g_attest_data + 12);
            uint32_t policy_version = 1;
            memcpy(desc + 64, &record_count, 4);
            memcpy(desc + 68, &policy_version, 4);
            
            outsec_append(note_out, (uint8_t *)&namesz, 4, 4);
            outsec_append(note_out, (uint8_t *)&descsz, 4, 4);
            outsec_append(note_out, (uint8_t *)&type, 4, 4);
            outsec_append(note_out, (uint8_t *)name, 4, 4);
            outsec_append(note_out, desc, 72, 4);
        }
        
        /* 2. Populate .zcc_tensor_attest section */
        OutSection *attest_out = get_out_section(".zcc_tensor_attest", SHF_ALLOC);
        outsec_append(attest_out, g_attest_data, g_attest_sz, 32);
    }

    /* finalize sizes from actual copy */
    for (i = 0; i < g_nout; i++) {
        if (g_out[i].buf_used > g_out[i].size) {
            g_out[i].size = g_out[i].buf_used;
        }
    }
}

/* ── Apply relocations ──────────────────────────────────────────────────── */
static uint64_t resolve_sym_value(ObjFile *o, uint32_t sym_idx) {
    Elf64_Sym *sym = &o->symtab[sym_idx];
    const char *name = o->strtab + sym->st_name;

    if (strcmp(name, "_kernel_end") == 0 || strcmp(name, "__kernel_end") == 0 || strcmp(name, "_kernel_end_asm") == 0) {
        uint64_t end_va = 0x100000;
        int s;
        for (s = 0; s < g_nout; s++) {
            if (g_out[s].vma + g_out[s].size > end_va) {
                end_va = g_out[s].vma + g_out[s].size;
            }
        }
        return end_va;
    }

    if (sym->st_shndx == SHN_ABS)
        return sym->st_value;
    if (sym->st_shndx == SHN_UNDEF || sym->st_shndx == SHN_COMMON) {
        /* global/common symbol lookup */
        GlobalSym *gs = find_sym(name);
        if (gs && gs->defined) return gs->value;
        fprintf(stderr, "zld: undefined symbol: %s\n", name);
        exit(1);
    }
    if (sym->st_shndx < o->ehdr->e_shnum) {
        return sym->st_value + o->section_vma[sym->st_shndx];
    }
    return sym->st_value;
}

static uint8_t *find_vma_in_buf(uint64_t vma) {
    int i;
    for (i = 0; i < g_nout; i++) {
        OutSection *s = &g_out[i];
        if (vma >= s->vma && vma < s->vma + s->size)
            return s->buf + (vma - s->vma);
    }
    return NULL;
}

static void apply_relocations(void) {
    int oi;
    for (oi = 0; oi < g_nobj; oi++) {
        ObjFile *o = &g_objs[oi];
        uint32_t si;
        for (si = 0; si < o->ehdr->e_shnum; si++) {
            Elf64_Shdr *sh = &o->shdrs[si];
            if (sh->sh_type != SHT_RELA) continue;

            /* target section */
            uint32_t target_shndx = sh->sh_info;
            if (!o->section_vma[target_shndx]) continue; /* not mapped */

            Elf64_Rela *relas = (Elf64_Rela *)(o->data + sh->sh_offset);
            uint32_t nrela = (uint32_t)(sh->sh_size / sizeof(Elf64_Rela));
            uint32_t ri;

            for (ri = 0; ri < nrela; ri++) {
                Elf64_Rela *r = &relas[ri];
                uint32_t sym_idx = (uint32_t)ELF64_R_SYM(r->r_info);
                uint32_t rtype   = (uint32_t)ELF64_R_TYPE(r->r_info);

                uint64_t S = resolve_sym_value(o, sym_idx);
                int64_t  A = r->r_addend;
                uint64_t P = o->section_vma[target_shndx] + r->r_offset;

                uint8_t *loc = find_vma_in_buf(P);
                if (!loc) {
                    fprintf(stderr, "zld: reloc target VMA 0x%llx not in output\n",
                            (unsigned long long)P);
                    continue;
                }

                switch (rtype) {
                case R_X86_64_NONE:
                    break;
                case R_X86_64_64: {
                    uint64_t val = S + A;
                    memcpy(loc, &val, 8);
                    break;
                }
                case R_X86_64_32: {
                    uint64_t val = S + A;
                    if (val >> 32) {
                        fprintf(stderr, "zld: R_X86_64_32 overflow at VMA 0x%llx\n",
                                (unsigned long long)P);
                    }
                    uint32_t v32 = (uint32_t)val;
                    memcpy(loc, &v32, 4);
                    break;
                }
                case R_X86_64_32S: {
                    int64_t val = (int64_t)(S + A);
                    int32_t v32 = (int32_t)val;
                    memcpy(loc, &v32, 4);
                    break;
                }
                case R_X86_64_PC32:
                case R_X86_64_PLT32: {
                    /* S + A - P; result must fit in signed 32-bit */
                    int64_t val = (int64_t)(S + A - P);
                    int32_t v32 = (int32_t)val;
                    memcpy(loc, &v32, 4);
                    break;
                }
                default:
                    fprintf(stderr, "zld: unhandled reloc type %u at 0x%llx\n",
                            rtype, (unsigned long long)P);
                    break;
                }
            }
        }
    }
}

/* ── Write ELF-64 executable ────────────────────────────────────────────── */
static void write_output(const char *path) {
    FILE *f = fopen(path, "wb");
    OutSection *rx_sections[32];
    int rx_count = 0;
    OutSection *rw_sections[32];
    int rw_count = 0;
    OutSection *note_sec = NULL;
    int i;
    int phnum = 0;
    uint64_t ehdr_size;
    uint64_t phdr_size;
    uint64_t headers_size;
    uint64_t data_offset;
    Elf64_Phdr phdrs[4];
    int ph_idx = 0;
    uint64_t rx_file_off;
    uint64_t rx_vaddr = 0;
    uint64_t rx_filesz = 0;
    uint64_t rx_memsz = 0;
    uint32_t rx_flags = PF_R;
    uint64_t rw_file_off = 0;
    uint64_t rw_vaddr = 0;
    uint64_t rw_filesz = 0;
    uint64_t rw_memsz = 0;
    uint32_t rw_flags = PF_R | PF_W;
    Elf64_Ehdr ehdr;
    uint64_t cur_pos;
    uint64_t next_page;

    if (!f) { fprintf(stderr, "zld: cannot write %s\n", path); exit(1); }

    for (i = 0; i < g_nout; i++) {
        OutSection *s = &g_out[i];
        if (g_verbose) {
            printf("DEBUG ZLD: write_output inspecting section '%s', size=0x%llx, flags=0x%llx\n", s->name, (unsigned long long)s->size, (unsigned long long)s->flags);
        }
        if (!(s->flags & SHF_ALLOC)) continue;
        if (s->size == 0) continue;

        if (strcmp(s->name, ".note") == 0 || strcmp(s->name, ".note.zcc.tensor") == 0) {
            note_sec = s;
        }

        if (s->flags & SHF_WRITE) {
            rw_sections[rw_count++] = s;
        } else {
            rx_sections[rx_count++] = s;
        }
    }

    if (rx_count > 0) phnum++;
    if (rw_count > 0) phnum++;
    if (note_sec) phnum++;

    ehdr_size = sizeof(Elf64_Ehdr);
    phdr_size = sizeof(Elf64_Phdr);
    headers_size = ehdr_size + phnum * phdr_size;
    data_offset = ALIGN_UP(headers_size, 0x1000);

    rx_file_off = data_offset;
    if (rx_count > 0) {
        rx_vaddr = rx_sections[0]->vma;
        rx_file_off = ALIGN_UP(headers_size, 0x1000) + (rx_vaddr & 0xFFF);
        for (i = 0; i < rx_count; i++) {
            OutSection *s = rx_sections[i];
            uint64_t sec_end = s->vma + s->size;
            if (sec_end - rx_vaddr > rx_memsz) {
                rx_memsz = sec_end - rx_vaddr;
            }
            if (s->flags & SHF_EXECINSTR) rx_flags |= PF_X;
        }
        rx_filesz = rx_memsz;

        memset(&phdrs[ph_idx], 0, sizeof(Elf64_Phdr));
        phdrs[ph_idx].p_type   = PT_LOAD;
        phdrs[ph_idx].p_flags  = rx_flags;
        phdrs[ph_idx].p_offset = rx_file_off;
        phdrs[ph_idx].p_vaddr  = rx_vaddr;
        phdrs[ph_idx].p_paddr  = rx_vaddr;
        phdrs[ph_idx].p_filesz = rx_filesz;
        phdrs[ph_idx].p_memsz  = rx_memsz;
        phdrs[ph_idx].p_align  = 0x1000;
        if (g_verbose) {
            printf("zld segment: RX_SEG, VMA=0x%llx, memsz=0x%llx, filesz=0x%llx, flags=%d\n",
                   (unsigned long long)rx_vaddr, (unsigned long long)rx_memsz,
                   (unsigned long long)rx_filesz, (int)rx_flags);
        }
        ph_idx++;
    } else {
        rx_file_off = ALIGN_UP(headers_size, 0x1000);
        rx_filesz = 0;
    }

    if (rw_count > 0) {
        rw_vaddr = rw_sections[0]->vma;
        rw_file_off = ALIGN_UP(rx_file_off + rx_filesz, 0x1000) + (rw_vaddr & 0xFFF);
        for (i = 0; i < rw_count; i++) {
            OutSection *s = rw_sections[i];
            uint64_t sec_end = s->vma + s->size;
            if (sec_end - rw_vaddr > rw_memsz) {
                rw_memsz = sec_end - rw_vaddr;
            }
            if (strcmp(s->name, ".bss") != 0) {
                if (sec_end - rw_vaddr > rw_filesz) {
                    rw_filesz = sec_end - rw_vaddr;
                }
            }
        }

        memset(&phdrs[ph_idx], 0, sizeof(Elf64_Phdr));
        phdrs[ph_idx].p_type   = PT_LOAD;
        phdrs[ph_idx].p_flags  = rw_flags;
        phdrs[ph_idx].p_offset = rw_file_off;
        phdrs[ph_idx].p_vaddr  = rw_vaddr;
        phdrs[ph_idx].p_paddr  = rw_vaddr;
        phdrs[ph_idx].p_filesz = rw_filesz;
        phdrs[ph_idx].p_memsz  = rw_memsz;
        phdrs[ph_idx].p_align  = 0x1000;
        if (g_verbose) {
            printf("zld segment: RW_SEG, VMA=0x%llx, memsz=0x%llx, filesz=0x%llx, flags=%d\n",
                   (unsigned long long)rw_vaddr, (unsigned long long)rw_memsz,
                   (unsigned long long)rw_filesz, (int)rw_flags);
        }
        ph_idx++;
    }

    if (note_sec) {
        uint64_t note_file_off = rx_file_off + (note_sec->vma - rx_vaddr);
        memset(&phdrs[ph_idx], 0, sizeof(Elf64_Phdr));
        phdrs[ph_idx].p_type   = PT_NOTE;
        phdrs[ph_idx].p_flags  = PF_R;
        phdrs[ph_idx].p_offset = note_file_off;
        phdrs[ph_idx].p_vaddr  = note_sec->vma;
        phdrs[ph_idx].p_paddr  = note_sec->vma;
        phdrs[ph_idx].p_filesz = note_sec->size;
        phdrs[ph_idx].p_memsz  = note_sec->size;
        phdrs[ph_idx].p_align  = 4;
        if (g_verbose) {
            printf("zld segment: PT_NOTE, VMA=0x%llx, memsz=0x%llx, filesz=0x%llx, flags=%d\n",
                   (unsigned long long)note_sec->vma, (unsigned long long)note_sec->size,
                   (unsigned long long)note_sec->size, (int)PF_R);
        }
        ph_idx++;
    }

    memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[0] = ELFMAG0; ehdr.e_ident[1] = ELFMAG1;
    ehdr.e_ident[2] = ELFMAG2; ehdr.e_ident[3] = ELFMAG3;
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_type      = ET_EXEC;
    ehdr.e_machine   = EM_X86_64;
    ehdr.e_version   = EV_CURRENT;
    ehdr.e_entry     = g_entry;
    ehdr.e_phoff     = ehdr_size;
    ehdr.e_shoff     = 0;
    ehdr.e_flags     = 0;
    ehdr.e_ehsize    = (Elf64_Half)ehdr_size;
    ehdr.e_phentsize = (Elf64_Half)phdr_size;
    ehdr.e_phnum     = (Elf64_Half)phnum;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum     = 0;
    ehdr.e_shstrndx  = 0;
    fwrite(&ehdr, 1, sizeof(ehdr), f);

    for (i = 0; i < phnum; i++) {
        fwrite(&phdrs[i], 1, sizeof(Elf64_Phdr), f);
    }

    cur_pos = (uint64_t)ftell(f);
    while (cur_pos < data_offset) { fputc(0, f); cur_pos++; }

    for (i = 0; i < rx_count; i++) {
        OutSection *sec = rx_sections[i];
        uint64_t expected = rx_file_off + (sec->vma - rx_vaddr);
        cur_pos = (uint64_t)ftell(f);
        while (cur_pos < expected) { fputc(0, f); cur_pos++; }
        if (sec->buf_used > 0) {
            fwrite(sec->buf, 1, sec->buf_used, f);
        }
    }

    cur_pos = (uint64_t)ftell(f);
    while (cur_pos < rw_file_off) { fputc(0, f); cur_pos++; }

    for (i = 0; i < rw_count; i++) {
        OutSection *sec = rw_sections[i];
        if (strcmp(sec->name, ".bss") == 0) continue;
        uint64_t expected = rw_file_off + (sec->vma - rw_vaddr);
        cur_pos = (uint64_t)ftell(f);
        while (cur_pos < expected) { fputc(0, f); cur_pos++; }
        if (sec->buf_used > 0) {
            fwrite(sec->buf, 1, sec->buf_used, f);
        }
    }

    cur_pos = (uint64_t)ftell(f);
    next_page = ALIGN_UP(cur_pos, 0x1000);
    while (cur_pos < next_page) { fputc(0, f); cur_pos++; }

    {
        /* Write debug sections and Section Header Table (SHT) */
        uint64_t *debug_file_offsets = xmalloc(g_nout * sizeof(uint64_t));
        char *shstrtab_buf = xmalloc(4096);
        size_t shstrtab_len = 0;
        uint32_t *name_offsets = xmalloc(g_nout * sizeof(uint32_t));
        uint32_t shstrtab_name_offset = 0;
        uint64_t shstrtab_offset = 0;
        uint64_t sht_offset = 0;
        int shnum = g_nout + 2;
        Elf64_Shdr *shdrs = xcalloc(shnum, sizeof(Elf64_Shdr));

        for (i = 0; i < (int)g_nout; i++) {
            debug_file_offsets[i] = 0;
        }

        for (i = 0; i < (int)g_nout; i++) {
            OutSection *sec = &g_out[i];
            if (sec->flags & SHF_ALLOC) continue;

            {
                uint64_t align = sec->align ? sec->align : 8;
                cur_pos = (uint64_t)ftell(f);
                uint64_t expected = ALIGN_UP(cur_pos, align);
                while (cur_pos < expected) { fputc(0, f); cur_pos++; }

                debug_file_offsets[i] = expected;
                if (sec->buf_used > 0) {
                    fwrite(sec->buf, 1, sec->buf_used, f);
                }
            }
        }

        /* Build .shstrtab */
        shstrtab_buf[shstrtab_len++] = '\0';
        for (i = 0; i < (int)g_nout; i++) {
            name_offsets[i] = (uint32_t)shstrtab_len;
            strcpy(shstrtab_buf + shstrtab_len, g_out[i].name);
            shstrtab_len += strlen(g_out[i].name) + 1;
        }
        shstrtab_name_offset = (uint32_t)shstrtab_len;
        strcpy(shstrtab_buf + shstrtab_len, ".shstrtab");
        shstrtab_len += strlen(".shstrtab") + 1;

        /* Write .shstrtab */
        cur_pos = (uint64_t)ftell(f);
        shstrtab_offset = ALIGN_UP(cur_pos, 8);
        while (cur_pos < shstrtab_offset) { fputc(0, f); cur_pos++; }
        fwrite(shstrtab_buf, 1, shstrtab_len, f);

        /* Write section headers */
        cur_pos = (uint64_t)ftell(f);
        sht_offset = ALIGN_UP(cur_pos, 8);
        while (cur_pos < sht_offset) { fputc(0, f); cur_pos++; }

        /* shdrs[0] is NULL */
        for (i = 0; i < (int)g_nout; i++) {
            OutSection *sec = &g_out[i];
            Elf64_Shdr *sh = &shdrs[i + 1];
            sh->sh_name = name_offsets[i];
            if (strcmp(sec->name, ".bss") == 0) {
                sh->sh_type = SHT_NOBITS;
            } else {
                sh->sh_type = SHT_PROGBITS;
            }
            sh->sh_flags = sec->flags;
            sh->sh_addr = sec->vma;
            sh->sh_size = sec->size;
            sh->sh_addralign = sec->align ? sec->align : 8;

            if (sec->flags & SHF_ALLOC) {
                if (strcmp(sec->name, ".note") == 0 || strcmp(sec->name, ".note.zcc.tensor") == 0) {
                    sh->sh_type = SHT_NOTE;
                    sh->sh_offset = rx_file_off + (sec->vma - rx_vaddr);
                } else if (sec->flags & SHF_WRITE) {
                    sh->sh_offset = rw_file_off + (sec->vma - rw_vaddr);
                } else {
                    sh->sh_offset = rx_file_off + (sec->vma - rx_vaddr);
                }
            } else {
                sh->sh_offset = debug_file_offsets[i];
            }
        }

        /* .shstrtab header */
        shdrs[g_nout + 1].sh_name = shstrtab_name_offset;
        shdrs[g_nout + 1].sh_type = SHT_STRTAB;
        shdrs[g_nout + 1].sh_flags = 0;
        shdrs[g_nout + 1].sh_addr = 0;
        shdrs[g_nout + 1].sh_offset = shstrtab_offset;
        shdrs[g_nout + 1].sh_size = shstrtab_len;
        shdrs[g_nout + 1].sh_addralign = 8;

        fwrite(shdrs, sizeof(Elf64_Shdr), shnum, f);

        /* Rewrite ELF header with section table offset */
        ehdr.e_shoff = sht_offset;
        ehdr.e_shnum = (Elf64_Half)shnum;
        ehdr.e_shstrndx = (Elf64_Half)(g_nout + 1);

        fseek(f, 0, SEEK_SET);
        fwrite(&ehdr, 1, sizeof(ehdr), f);

        free(debug_file_offsets);
        free(shstrtab_buf);
        free(name_offsets);
        free(shdrs);
    }
    fclose(f);
    if (g_verbose) {
        printf("zld: wrote %s (entry=0x%llx, %d segments)\n",
               path, (unsigned long long)g_entry, phnum);
    }
}

/* ── zld_link ───────────────────────────────────────────────────────────── */
int zld_link(const char **obj_files, int obj_count, const char *out_path, const char *script_path, const char *tensor_attest_bin_path, const char *tensor_note_json_path) {
    int i;
    char *env_verb = getenv("ZLD_VERBOSE");
    g_verbose = env_verb ? atoi(env_verb) : 0;

    /* Reset and initialize global state */
    cleanup_linker_state();

    if (tensor_attest_bin_path) {
        g_attest_data = read_file(tensor_attest_bin_path, &g_attest_sz);
        if (g_attest_sz < 96) {
            die("tensor attestation file too small");
        }
        uint64_t magic = *(uint64_t *)(g_attest_data);
        uint32_t version = *(uint32_t *)(g_attest_data + 8);
        if (magic != 0x5453415f43435aULL) {
            die("invalid tensor attestation file magic");
        }
        if (version != 1) {
            die("unsupported tensor attestation version");
        }
    }

    if (tensor_note_json_path) {
        if (g_verbose) {
            printf("[zld] Ingested tensor note JSON path (JSON parsing bypassed): %s\n", tensor_note_json_path);
        }
    }

    if (g_verbose) {
        printf("[zld] Initiating static link campaign: output=%s\n", out_path);
    }
    if (script_path) {
        parse_ld_script(script_path);
    } else {
        /* Default hardcoded layout rules */
        LdRule *r;
        strncpy(g_entry_name, "_start", sizeof(g_entry_name)-1);
        r = add_rule();
        strcpy(r->out_name, ".text"); strcpy(r->in_pattern, ".text*");
        r->addr = 0x100000; r->has_addr = 1;
        r = add_rule();
        strcpy(r->out_name, ".text"); strcpy(r->in_pattern, ".multiboot");
        r->addr = 0x100000; r->has_addr = 1;
        r = add_rule();
        strcpy(r->out_name, ".rodata"); strcpy(r->in_pattern, ".rodata*");
        r->addr = 0; r->has_addr = 0;
        r = add_rule();
        strcpy(r->out_name, ".data"); strcpy(r->in_pattern, ".data*");
        r->addr = 0; r->has_addr = 0;
        r = add_rule();
        strcpy(r->out_name, ".bss"); strcpy(r->in_pattern, ".bss*");
        r->addr = 0; r->has_addr = 0;
    }

    g_nobj = obj_count;
    g_objs_cap = g_nobj;
    g_objs = xcalloc(g_objs_cap, sizeof(ObjFile));
    for (i = 0; i < g_nobj; i++) {
        load_obj(obj_files[i], i);
    }

    layout();
    collect_symbols();
    copy_sections();
    apply_relocations();
    write_output(out_path);

    /* Free allocated memory to avoid leaks on repeat calls */
    cleanup_linker_state();

    return 0;
}


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#if defined(__GNUC__) && !defined(__GNUC_MINOR__)
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
typedef long time_t;
time_t time(time_t *tloc);
struct tm *gmtime(const time_t *timep);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
#endif

#include "zcc_elf_parser.h"
#include "zcc_sha256.h"

/* Analysis Limits */
#define MAX_OBJECTS 64
#define MAX_FUNCTIONS 2048
#define MAX_EDGES 8192

/* Input object structures using the common Elf64_Obj substrate */
typedef struct {
    const char *path;
    Elf64_Obj   obj;
} InputObj;

/* Global Function representation */
typedef struct {
    char        name[128];
    int         obj_idx;      /* index into cg->objs, or -1 if unresolved external */
    int         sym_idx;      /* index into obj->symtab */
    Elf64_Addr  value;        /* offset inside its section */
    Elf64_Xword size;         /* size of function */
    int         shndx;        /* section index in obj */
    int         is_exported;
    int         is_imported;  /* placeholder if unresolved import */
    int         referenced;   /* incoming call edges (fan-in) */
    int         visited;      /* DFS cycle state (0=unvisited, 1=visiting, 2=visited) */
} FunctionNode;

/* Adjacency list edge with relocation type */
typedef struct {
    int      src_fn_idx;
    int      dest_fn_idx;
    uint32_t reloc_type;
} CallEdge;

/* CallGraph Struct to support multiple graphs in memory */
typedef struct {
    InputObj     objs[MAX_OBJECTS];
    int          obj_count;
    FunctionNode funcs[MAX_FUNCTIONS];
    int          func_count;
    CallEdge     edges[MAX_EDGES];
    int          edge_count;
} CallGraph;

static CallGraph g_candidate;

static void die(const char *msg) {
    fprintf(stderr, "zcc_zxr_verify: fatal: %s\n", msg);
    exit(1);
}

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "zcc_zxr_verify: error: cannot open %s\n", path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(*sz + 1);
    if (!buf) die("out of memory");
    if (fread(buf, 1, *sz, f) != *sz) die("file read error");
    fclose(f);
    buf[*sz] = 0;
    return buf;
}

static void parse_object(CallGraph *cg, const char *path, int obj_idx) {
    InputObj *o = &cg->objs[obj_idx];
    o->path = path;
    size_t sz = 0;
    uint8_t *data = load_file(path, &sz);

    char err_msg[256];
    err_msg[0] = '\0';
    if (elf64_parse(data, sz, &o->obj, err_msg, sizeof(err_msg)) != 0) {
        fprintf(stderr, "zcc_zxr_verify: error parsing %s: %s\n", path, err_msg);
        free(data);
        exit(1);
    }
}

static int find_or_add_function(CallGraph *cg, const char *name, int obj_idx, int sym_idx, Elf64_Addr val, Elf64_Xword sz, int shndx, int is_def) {
    for (int i = 0; i < cg->func_count; i++) {
        if (strcmp(cg->funcs[i].name, name) == 0) {
            if (is_def && !cg->funcs[i].is_exported) {
                cg->funcs[i].obj_idx = obj_idx;
                cg->funcs[i].sym_idx = sym_idx;
                cg->funcs[i].value = val;
                cg->funcs[i].size = sz;
                cg->funcs[i].shndx = shndx;
                cg->funcs[i].is_exported = 1;
                cg->funcs[i].is_imported = 0;
            }
            return i;
        }
    }
    if (cg->func_count >= MAX_FUNCTIONS) die("exceeded max function nodes");
    int idx = cg->func_count++;
    strncpy(cg->funcs[idx].name, name, 127);
    cg->funcs[idx].name[127] = '\0';
    cg->funcs[idx].obj_idx = obj_idx;
    cg->funcs[idx].sym_idx = sym_idx;
    cg->funcs[idx].value = val;
    cg->funcs[idx].size = sz;
    cg->funcs[idx].shndx = shndx;
    if (is_def) {
        cg->funcs[idx].is_exported = 1;
        cg->funcs[idx].is_imported = 0;
    } else {
        cg->funcs[idx].is_exported = 0;
        cg->funcs[idx].is_imported = 1;
    }
    cg->funcs[idx].referenced = 0;
    cg->funcs[idx].visited = 0;
    return idx;
}

static int get_func_at_offset(CallGraph *cg, int obj_idx, int shndx, Elf64_Addr offset) {
    int best_idx = -1;
    Elf64_Addr best_val = 0;

    for (int i = 0; i < cg->func_count; i++) {
        FunctionNode *fn = &cg->funcs[i];
        if (fn->obj_idx == obj_idx && fn->shndx == shndx && fn->is_exported) {
            if (fn->size > 0) {
                if (offset >= fn->value && offset < fn->value + fn->size) {
                    return i;
                }
            } else {
                if (offset >= fn->value && (best_idx == -1 || fn->value > best_val)) {
                    best_idx = i;
                    best_val = fn->value;
                }
            }
        }
    }
    return best_idx;
}

static void add_call_edge(CallGraph *cg, int src_fn, int dest_fn, uint32_t rtype) {
    for (int i = 0; i < cg->edge_count; i++) {
        if (cg->edges[i].src_fn_idx == src_fn && cg->edges[i].dest_fn_idx == dest_fn) {
            return;
        }
    }
    if (cg->edge_count >= MAX_EDGES) die("exceeded max call graph edges");
    cg->edges[cg->edge_count].src_fn_idx = src_fn;
    cg->edges[cg->edge_count].dest_fn_idx = dest_fn;
    cg->edges[cg->edge_count].reloc_type = rtype;
    cg->edge_count++;
    cg->funcs[dest_fn].referenced++;
}

static int get_fan_out(CallGraph *cg, int fn_idx) {
    int cnt = 0;
    for (int i = 0; i < cg->edge_count; i++) {
        if (cg->edges[i].src_fn_idx == fn_idx) {
            cnt++;
        }
    }
    return cnt;
}

static int get_criticality_score(CallGraph *cg, int fn_idx) {
    int fan_in = cg->funcs[fn_idx].referenced;
    int fan_out = get_fan_out(cg, fn_idx);
    return (fan_in * 2) + fan_out + (cg->funcs[fn_idx].is_exported ? 5 : 0);
}

static const char *get_function_domain(const char *name) {
    if (strcmp(name, "_start") == 0 || strcmp(name, "main") == 0 ||
        strcmp(name, "setup_page_tables") == 0 || strcmp(name, "start64") == 0 ||
        strcmp(name, "kmain") == 0) {
        return "Boot";
    }
    if (strstr(name, "pmm_") != NULL || strstr(name, "heap_") != NULL ||
        strstr(name, "bitmap") != NULL || strstr(name, "blocks") != NULL ||
        strstr(name, "page") != NULL) {
        return "Memory";
    }
    if (strstr(name, "isr") != NULL || strstr(name, "idt_") != NULL ||
        strstr(name, "pic_") != NULL || strstr(name, "handle_interrupt") != NULL ||
        strstr(name, "handle_exception") != NULL || strstr(name, "lidt_") != NULL) {
        return "Interrupts";
    }
    if (strstr(name, "serial") != NULL || strstr(name, "outb") != NULL ||
        strstr(name, "inb") != NULL) {
        return "IO_Serial";
    }
    if (strstr(name, "vga_") != NULL || strstr(name, "print") != NULL ||
        strstr(name, "screen") != NULL || strstr(name, "scroll") != NULL ||
        strstr(name, "cursor") != NULL) {
        return "Console";
    }
    return "General";
}

static void populate_call_graph(CallGraph *cg, const char **files, int count) {
    cg->obj_count = count;
    if (cg->obj_count > MAX_OBJECTS) cg->obj_count = MAX_OBJECTS;

    for (int i = 0; i < cg->obj_count; i++) {
        parse_object(cg, files[i], i);
    }

    for (int oi = 0; oi < cg->obj_count; oi++) {
        InputObj *o = &cg->objs[oi];
        for (int i = 0; i < o->obj.symcnt; i++) {
            Elf64_Sym *sym = &o->obj.symtab[i];
            int type = ELF64_ST_TYPE(sym->st_info);

            if (sym->st_shndx != SHN_UNDEF && sym->st_shndx < o->obj.ehdr->e_shnum) {
                Elf64_Shdr *sec = &o->obj.shdrs[sym->st_shndx];
                int is_code = (sec->sh_flags & 4) != 0;

                if (is_code && (type == STT_FUNC || type == STT_NOTYPE)) {
                    const char *name = o->obj.strtab + sym->st_name;
                    if (name[0] && name[0] != '.') {
                        find_or_add_function(cg, name, oi, i, sym->st_value, sym->st_size, sym->st_shndx, 1);
                    }
                }
            }
        }
    }

    for (int oi = 0; oi < cg->obj_count; oi++) {
        InputObj *o = &cg->objs[oi];
        for (int i = 0; i < o->obj.ehdr->e_shnum; i++) {
            Elf64_Shdr *sh = &o->obj.shdrs[i];
            if (sh->sh_type == SHT_RELA) {
                int target_shndx = sh->sh_info;
                Elf64_Rela *relas = (Elf64_Rela *)(o->obj.data + sh->sh_offset);
                int rela_count = (int)(sh->sh_size / sizeof(Elf64_Rela));

                for (int r = 0; r < rela_count; r++) {
                    Elf64_Rela *rela = &relas[r];
                    uint32_t sym_idx = (uint32_t)ELF64_R_SYM(rela->r_info);
                    Elf64_Sym *sym = &o->obj.symtab[sym_idx];
                    const char *target_name = o->obj.strtab + sym->st_name;

                    if (!target_name[0] || target_name[0] == '.') continue;

                    int sym_type = ELF64_ST_TYPE(sym->st_info);
                    if (sym_type == STT_SECTION || sym_type == STT_FILE) continue;

                    int caller_fn = get_func_at_offset(cg, oi, target_shndx, rela->r_offset);
                    if (caller_fn == -1) continue;

                    int callee_fn = -1;
                    if (sym->st_shndx != SHN_UNDEF) {
                        callee_fn = find_or_add_function(cg, target_name, oi, sym_idx, sym->st_value, sym->st_size, sym->st_shndx, 1);
                    } else {
                        callee_fn = find_or_add_function(cg, target_name, -1, sym_idx, 0, 0, SHN_UNDEF, 0);
                    }

                    if (callee_fn != -1 && caller_fn != callee_fn) {
                        uint32_t rtype = (uint32_t)ELF64_R_TYPE(rela->r_info);
                        add_call_edge(cg, caller_fn, callee_fn, rtype);
                    }
                }
            }
        }
    }
}

static Elf64_Xword get_symbol_size(InputObj *o, Elf64_Sym *sym) {
    if (sym->st_size > 0) {
        return sym->st_size;
    }
    if (sym->st_shndx >= o->obj.ehdr->e_shnum) {
        return 0;
    }
    Elf64_Addr next_val = 0;
    int found_next = 0;
    Elf64_Shdr *sec = &o->obj.shdrs[sym->st_shndx];
    Elf64_Addr sec_limit = sec->sh_size;

    for (int i = 0; i < o->obj.symcnt; i++) {
        Elf64_Sym *s = &o->obj.symtab[i];
        if (s->st_shndx == sym->st_shndx && ELF64_ST_TYPE(s->st_info) == STT_FUNC) {
            if (s->st_value > sym->st_value) {
                if (!found_next || s->st_value < next_val) {
                    next_val = s->st_value;
                    found_next = 1;
                }
            }
        }
    }
    if (found_next) {
        return next_val - sym->st_value;
    }
    return sec_limit - sym->st_value;
}

static void compute_function_hash(InputObj *o, Elf64_Sym *sym, char *output_hex) {
    Elf64_Xword sym_size = get_symbol_size(o, sym);
    if (sym_size > 0 && sym->st_shndx < o->obj.ehdr->e_shnum) {
        Elf64_Shdr *sec = &o->obj.shdrs[sym->st_shndx];
        if (sec->sh_offset + sym->st_value + sym_size <= o->obj.size) {
            const uint8_t *fn_code = o->obj.data + sec->sh_offset + sym->st_value;
            ZccSHA256_CTX ctx;
            zcc_sha256_init(&ctx);
            zcc_sha256_update(&ctx, fn_code, sym_size);
            uint8_t hash[32];
            zcc_sha256_final(&ctx, hash);
            for (int i = 0; i < 32; i++) {
                sprintf(output_hex + i * 2, "%02x", hash[i]);
            }
            output_hex[64] = '\0';
        } else {
            ZccSHA256_CTX ctx;
            zcc_sha256_init(&ctx);
            const char *name = o->obj.strtab + sym->st_name;
            zcc_sha256_update(&ctx, (const uint8_t *)name, strlen(name));
            uint8_t hash[32];
            zcc_sha256_final(&ctx, hash);
            for (int i = 0; i < 32; i++) {
                sprintf(output_hex + i * 2, "%02x", hash[i]);
            }
            output_hex[64] = '\0';
        }
    } else {
        ZccSHA256_CTX ctx;
        zcc_sha256_init(&ctx);
        const char *name = o->obj.strtab + sym->st_name;
        zcc_sha256_update(&ctx, (const uint8_t *)name, strlen(name));
        uint8_t hash[32];
        zcc_sha256_final(&ctx, hash);
        for (int i = 0; i < 32; i++) {
            sprintf(output_hex + i * 2, "%02x", hash[i]);
        }
        output_hex[64] = '\0';
    }
}

static int cmp_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

static void compute_domain_merkle_root(CallGraph *cg, const char *domain_name, char *output_hex) {
    const char *hashes[MAX_FUNCTIONS];
    int count = 0;
    
    for (int i = 0; i < cg->func_count; i++) {
        FunctionNode *fn = &cg->funcs[i];
        if (strcmp(get_function_domain(fn->name), domain_name) == 0) {
            char *fn_hash = malloc(65);
            if (!fn_hash) die("out of memory");
            
            if (fn->obj_idx != -1 && fn->obj_idx < cg->obj_count) {
                InputObj *o = &cg->objs[fn->obj_idx];
                if (fn->sym_idx >= 0 && fn->sym_idx < o->obj.symcnt) {
                    Elf64_Sym *sym = &o->obj.symtab[fn->sym_idx];
                    compute_function_hash(o, sym, fn_hash);
                } else {
                    ZccSHA256_CTX ctx;
                    zcc_sha256_init(&ctx);
                    zcc_sha256_update(&ctx, (const uint8_t *)fn->name, strlen(fn->name));
                    uint8_t hash_bytes[32];
                    zcc_sha256_final(&ctx, hash_bytes);
                    for (int h = 0; h < 32; h++) sprintf(fn_hash + h * 2, "%02x", hash_bytes[h]);
                    fn_hash[64] = '\0';
                }
            } else {
                ZccSHA256_CTX ctx;
                zcc_sha256_init(&ctx);
                zcc_sha256_update(&ctx, (const uint8_t *)fn->name, strlen(fn->name));
                uint8_t hash_bytes[32];
                zcc_sha256_final(&ctx, hash_bytes);
                for (int h = 0; h < 32; h++) sprintf(fn_hash + h * 2, "%02x", hash_bytes[h]);
                fn_hash[64] = '\0';
            }
            
            hashes[count++] = fn_hash;
            if (count >= MAX_FUNCTIONS) break;
        }
    }
    
    if (count == 0) {
        strcpy(output_hex, "0000000000000000000000000000000000000000000000000000000000000000");
        return;
    }
    
    qsort(hashes, count, sizeof(char *), cmp_strings);
    
    ZccSHA256_CTX m_ctx;
    zcc_sha256_init(&m_ctx);
    for (int i = 0; i < count; i++) {
        zcc_sha256_update(&m_ctx, (const uint8_t *)hashes[i], 64);
        free((void *)hashes[i]);
    }
    uint8_t m_hash[32];
    zcc_sha256_final(&m_ctx, m_hash);
    for (int i = 0; i < 32; i++) {
        sprintf(output_hex + i * 2, "%02x", m_hash[i]);
    }
    output_hex[64] = '\0';
}

typedef struct {
    int entrypoint_start; /* 1 if _start is entrypoint, 0 if main, -1 if none */
    int reachable_functions;
    int leaf_functions;
    int branch_nodes;
    int critical_path_depth;
    char controlflow_root[65];

    int mov_cnt;
    int call_cnt;
    int lea_cnt;
    int cmp_cnt;
    int jmp_cnt;
    int ret_cnt;
    char instruction_root[65];

    int reg_counts[16];
    char register_root[65];

    int max_stack_frame;
    int average_stack_frame;
    int recursive_functions;
    char stack_root[65];

    int stability_score;
} ExecutionFingerprint;

static int detect_recursion_from(CallGraph *cg, int fn_idx, int target, uint8_t *visited) {
    visited[fn_idx] = 1;
    for (int i = 0; i < cg->edge_count; i++) {
        if (cg->edges[i].src_fn_idx == fn_idx) {
            int dest = cg->edges[i].dest_fn_idx;
            if (dest == target) {
                return 1;
            }
            if (!visited[dest]) {
                if (detect_recursion_from(cg, dest, target, visited)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int get_max_depth(CallGraph *cg, int fn_idx, uint8_t *visited) {
    visited[fn_idx] = 1;
    int max_sub_depth = 0;
    for (int i = 0; i < cg->edge_count; i++) {
        if (cg->edges[i].src_fn_idx == fn_idx) {
            int dest = cg->edges[i].dest_fn_idx;
            if (!visited[dest]) {
                int d = get_max_depth(cg, dest, visited);
                if (d > max_sub_depth) {
                    max_sub_depth = d;
                }
            }
        }
    }
    visited[fn_idx] = 0;
    return 1 + max_sub_depth;
}

static void mark_reachable(CallGraph *cg, int fn_idx, uint8_t *visited) {
    visited[fn_idx] = 1;
    for (int i = 0; i < cg->edge_count; i++) {
        if (cg->edges[i].src_fn_idx == fn_idx) {
            int dest = cg->edges[i].dest_fn_idx;
            if (!visited[dest]) {
                mark_reachable(cg, dest, visited);
            }
        }
    }
}

static void compute_execution_fingerprint(CallGraph *cg, ExecutionFingerprint *ef) {
    memset(ef, 0, sizeof(ExecutionFingerprint));
    ef->stability_score = 100;

    int entry_idx = -1;
    for (int i = 0; i < cg->func_count; i++) {
        if (strcmp(cg->funcs[i].name, "_start") == 0) {
            entry_idx = i;
            ef->entrypoint_start = 1;
            break;
        }
    }
    if (entry_idx == -1) {
        for (int i = 0; i < cg->func_count; i++) {
            if (strcmp(cg->funcs[i].name, "main") == 0) {
                entry_idx = i;
                ef->entrypoint_start = 0;
                break;
            }
        }
    }
    if (entry_idx == -1) {
        ef->entrypoint_start = -1;
    }

    if (entry_idx != -1) {
        uint8_t visited[MAX_FUNCTIONS];
        memset(visited, 0, sizeof(visited));
        mark_reachable(cg, entry_idx, visited);
        for (int i = 0; i < cg->func_count; i++) {
            if (visited[i]) ef->reachable_functions++;
        }

        memset(visited, 0, sizeof(visited));
        ef->critical_path_depth = get_max_depth(cg, entry_idx, visited);
    }

    for (int i = 0; i < cg->func_count; i++) {
        if (cg->funcs[i].is_imported) continue;
        int out_degree = 0;
        for (int j = 0; j < cg->edge_count; j++) {
            if (cg->edges[j].src_fn_idx == i) {
                out_degree++;
            }
        }
        if (out_degree == 0) ef->leaf_functions++;
        else if (out_degree > 1) ef->branch_nodes++;
    }

    {
        char cf_buf[256];
        sprintf(cf_buf, "entrypoint:%s,reachable:%d,leaf:%d,branch:%d,depth:%d",
                ef->entrypoint_start == 1 ? "_start" : (ef->entrypoint_start == 0 ? "main" : "none"),
                ef->reachable_functions, ef->leaf_functions, ef->branch_nodes, ef->critical_path_depth);
        ZccSHA256_CTX ctx;
        zcc_sha256_init(&ctx);
        zcc_sha256_update(&ctx, (const uint8_t *)cf_buf, strlen(cf_buf));
        uint8_t hash[32];
        zcc_sha256_final(&ctx, hash);
        for (int h = 0; h < 32; h++) sprintf(ef->controlflow_root + h * 2, "%02x", hash[h]);
        ef->controlflow_root[64] = '\0';
    }

    long long total_stack_frame = 0;
    int defined_funcs_with_size = 0;

    for (int i = 0; i < cg->func_count; i++) {
        FunctionNode *fn = &cg->funcs[i];
        if (fn->is_imported || fn->obj_idx == -1) continue;

        InputObj *o = &cg->objs[fn->obj_idx];
        if (fn->sym_idx < 0 || fn->sym_idx >= o->obj.symcnt) continue;
        Elf64_Sym *sym = &o->obj.symtab[fn->sym_idx];

        Elf64_Xword sym_size = get_symbol_size(o, sym);
        if (sym_size > 0 && sym->st_shndx < o->obj.ehdr->e_shnum) {
            Elf64_Shdr *sec = &o->obj.shdrs[sym->st_shndx];
            if (sec->sh_offset + sym->st_value + sym_size <= o->obj.size) {
                const uint8_t *fn_code = o->obj.data + sec->sh_offset + sym->st_value;
                int size = sym_size;

                uint8_t rex = 0;
                for (int ip = 0; ip < size; ip++) {
                    uint8_t b = fn_code[ip];
                    if ((b & 0xf0) == 0x40) {
                        rex = b;
                        continue;
                    }
                    if (b >= 0x50 && b <= 0x57) {
                        int reg = b - 0x50;
                        if (rex & 1) reg += 8;
                        ef->reg_counts[reg]++;
                        rex = 0;
                    } else if (b >= 0x58 && b <= 0x5f) {
                        int reg = b - 0x58;
                        if (rex & 1) reg += 8;
                        ef->reg_counts[reg]++;
                        rex = 0;
                    } else if (b >= 0xb8 && b <= 0xbf) {
                        int reg = b - 0xb8;
                        if (rex & 1) reg += 8;
                        ef->reg_counts[reg]++;
                        ef->mov_cnt++;
                        rex = 0;
                    } else if (b == 0xc3 || b == 0xc2) {
                        ef->ret_cnt++;
                        rex = 0;
                    } else if (b == 0xe8) {
                        ef->call_cnt++;
                        rex = 0;
                    } else if (b == 0x8d) {
                        ef->lea_cnt++;
                        if (ip + 1 < size) {
                            uint8_t modrm = fn_code[ip + 1];
                            int reg1 = (modrm >> 3) & 7;
                            if (rex & 4) reg1 += 8;
                            int reg2 = modrm & 7;
                            if (rex & 1) reg2 += 8;
                            ef->reg_counts[reg1]++;
                            if ((modrm >> 6) == 3) ef->reg_counts[reg2]++;
                        }
                        rex = 0;
                    } else if (b == 0x89 || b == 0x8b || b == 0xc7) {
                        ef->mov_cnt++;
                        if (ip + 1 < size) {
                            uint8_t modrm = fn_code[ip + 1];
                            int reg1 = (modrm >> 3) & 7;
                            if (rex & 4) reg1 += 8;
                            int reg2 = modrm & 7;
                            if (rex & 1) reg2 += 8;
                            ef->reg_counts[reg1]++;
                            if ((modrm >> 6) == 3) ef->reg_counts[reg2]++;
                        }
                        rex = 0;
                    } else if (b == 0x39 || b == 0x3b || b == 0x3d) {
                        ef->cmp_cnt++;
                        if (b != 0x3d && ip + 1 < size) {
                            uint8_t modrm = fn_code[ip + 1];
                            int reg1 = (modrm >> 3) & 7;
                            if (rex & 4) reg1 += 8;
                            int reg2 = modrm & 7;
                            if (rex & 1) reg2 += 8;
                            ef->reg_counts[reg1]++;
                            if ((modrm >> 6) == 3) ef->reg_counts[reg2]++;
                        }
                        rex = 0;
                    } else if (b == 0xe9 || b == 0xeb) {
                        ef->jmp_cnt++;
                        rex = 0;
                    } else if (b == 0xff) {
                        if (ip + 1 < size) {
                            uint8_t modrm = fn_code[ip + 1];
                            int op = (modrm >> 3) & 7;
                            if (op == 2) ef->call_cnt++;
                            else if (op == 4) ef->jmp_cnt++;
                            int reg2 = modrm & 7;
                            if (rex & 1) reg2 += 8;
                            if ((modrm >> 6) == 3) ef->reg_counts[reg2]++;
                        }
                        rex = 0;
                    }
                }

                int frame_size = 0;
                for (int s = 0; s < size - 3 && s < 32; s++) {
                    if (fn_code[s] == 0x48 && fn_code[s+1] == 0x83 && fn_code[s+2] == 0xec) {
                        frame_size = fn_code[s+3];
                        break;
                    }
                    if (s < size - 6 && fn_code[s] == 0x48 && fn_code[s+1] == 0x81 && fn_code[s+2] == 0xec) {
                        frame_size = fn_code[s+3] | (fn_code[s+4] << 8) | (fn_code[s+5] << 16) | (fn_code[s+6] << 24);
                        break;
                    }
                }
                if (frame_size > ef->max_stack_frame) {
                    ef->max_stack_frame = frame_size;
                }
                total_stack_frame += frame_size;
                defined_funcs_with_size++;
            }
        }

        uint8_t cyc_visited[MAX_FUNCTIONS];
        memset(cyc_visited, 0, sizeof(cyc_visited));
        if (detect_recursion_from(cg, i, i, cyc_visited)) {
            ef->recursive_functions++;
        }
    }

    if (defined_funcs_with_size > 0) {
        ef->average_stack_frame = (int)(total_stack_frame / defined_funcs_with_size);
    }

    {
        char ins_buf[256];
        sprintf(ins_buf, "mov:%d,call:%d,lea:%d,cmp:%d,jmp:%d,ret:%d",
                ef->mov_cnt, ef->call_cnt, ef->lea_cnt, ef->cmp_cnt, ef->jmp_cnt, ef->ret_cnt);
        ZccSHA256_CTX ctx;
        zcc_sha256_init(&ctx);
        zcc_sha256_update(&ctx, (const uint8_t *)ins_buf, strlen(ins_buf));
        uint8_t hash[32];
        zcc_sha256_final(&ctx, hash);
        for (int h = 0; h < 32; h++) sprintf(ef->instruction_root + h * 2, "%02x", hash[h]);
        ef->instruction_root[64] = '\0';
    }

    {
        char reg_buf[512];
        sprintf(reg_buf, "rax:%d,rbx:%d,rcx:%d,rdx:%d,rsi:%d,rdi:%d,rbp:%d,rsp:%d,r8:%d,r9:%d,r10:%d,r11:%d,r12:%d,r13:%d,r14:%d,r15:%d",
                ef->reg_counts[0], ef->reg_counts[3], ef->reg_counts[1], ef->reg_counts[2],
                ef->reg_counts[6], ef->reg_counts[7], ef->reg_counts[5], ef->reg_counts[4],
                ef->reg_counts[8], ef->reg_counts[9], ef->reg_counts[10], ef->reg_counts[11],
                ef->reg_counts[12], ef->reg_counts[13], ef->reg_counts[14], ef->reg_counts[15]);
        ZccSHA256_CTX ctx;
        zcc_sha256_init(&ctx);
        zcc_sha256_update(&ctx, (const uint8_t *)reg_buf, strlen(reg_buf));
        uint8_t hash[32];
        zcc_sha256_final(&ctx, hash);
        for (int h = 0; h < 32; h++) sprintf(ef->register_root + h * 2, "%02x", hash[h]);
        ef->register_root[64] = '\0';
    }

    {
        char stk_buf[256];
        sprintf(stk_buf, "max:%d,avg:%d,rec:%d",
                ef->max_stack_frame, ef->average_stack_frame, ef->recursive_functions);
        ZccSHA256_CTX ctx;
        zcc_sha256_init(&ctx);
        zcc_sha256_update(&ctx, (const uint8_t *)stk_buf, strlen(stk_buf));
        uint8_t hash[32];
        zcc_sha256_final(&ctx, hash);
        for (int h = 0; h < 32; h++) sprintf(ef->stack_root + h * 2, "%02x", hash[h]);
        ef->stack_root[64] = '\0';
    }
}

static FILE *open_source_file(const char *obj_path) {
    char src_path[512];
    strncpy(src_path, obj_path, sizeof(src_path) - 1);
    src_path[sizeof(src_path) - 1] = '\0';
    size_t len = strlen(src_path);
    if (len > 2 && src_path[len - 2] == '.' && src_path[len - 1] == 'o') {
        src_path[len - 1] = 'c';
    } else {
        strncat(src_path, ".c", sizeof(src_path) - len - 1);
    }
    FILE *f = fopen(src_path, "rb");
    if (f) return f;

    const char *base = src_path;
    for (const char *p = src_path; *p; p++) {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
    }
    if (base != src_path) {
        f = fopen(base, "rb");
        if (f) return f;
    }
    return NULL;
}

static int cmp_edges(const void *a, const void *b) {
    const CallEdge *ea = (const CallEdge *)a;
    const CallEdge *eb = (const CallEdge *)b;
    if (ea->src_fn_idx != eb->src_fn_idx) {
        return ea->src_fn_idx - eb->src_fn_idx;
    }
    return ea->dest_fn_idx - eb->dest_fn_idx;
}

/* Scoped JSON extraction helper */
static int find_json_string_scoped(const char *scope, const char *key, char *out_val, int max_len) {
    if (!scope) return 0;
    const char *p = strstr(scope, key);
    if (!p) return 0;
    p += strlen(key);
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    p = strchr(p, '"');
    if (!p) return 0;
    p++;
    int len = 0;
    while (*p && *p != '"' && len < max_len - 1) {
        out_val[len++] = *p++;
    }
    out_val[len] = '\0';
    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: zcc_zxr_verify <record.zxr> <obj1.o> <obj2.o> ...\n");
        return 1;
    }

    const char *zxr_path = argv[1];
    const char *objects[MAX_OBJECTS];
    int obj_count = 0;
    for (int i = 2; i < argc; i++) {
        if (obj_count < MAX_OBJECTS) {
            objects[obj_count++] = argv[i];
        }
    }

    /* 1. Load and parse the record.zxr JSON file */
    size_t zxr_size = 0;
    uint8_t *zxr_data = load_file(zxr_path, &zxr_size);
    const char *json = (const char *)zxr_data;

    const char *metadata_block = strstr(json, "\"metadata\"");
    const char *merkle_block = strstr(json, "\"merkle_topology\"");

    char exp_compiler[128] = {0};
    char exp_stage[128] = {0};
    char exp_version[128] = {0};
    char exp_build_id[128] = {0};
    char exp_source_status[128] = {0};
    char exp_source_sha256[128] = {0};
    char exp_object_sha256[128] = {0};
    char exp_topology_hash[128] = {0};
    char exp_timestamp[128] = {0};
    char exp_topology_root[128] = {0};

    int schema_pass = 1;
    if (!find_json_string_scoped(metadata_block, "\"compiler\"", exp_compiler, sizeof(exp_compiler))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"compiler_stage\"", exp_stage, sizeof(exp_stage))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"version\"", exp_version, sizeof(exp_version))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"build_id\"", exp_build_id, sizeof(exp_build_id))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"source_status\"", exp_source_status, sizeof(exp_source_status))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"source_sha256\"", exp_source_sha256, sizeof(exp_source_sha256))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"object_sha256\"", exp_object_sha256, sizeof(exp_object_sha256))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"topology_hash\"", exp_topology_hash, sizeof(exp_topology_hash))) schema_pass = 0;
    if (!find_json_string_scoped(metadata_block, "\"timestamp\"", exp_timestamp, sizeof(exp_timestamp))) schema_pass = 0;
    if (!find_json_string_scoped(merkle_block, "\"topology_root\"", exp_topology_root, sizeof(exp_topology_root))) schema_pass = 0;

    const char *fingerprint_block = strstr(json, "\"execution_fingerprint\"");
    char exp_controlflow_root[128] = {0};
    char exp_instruction_root[128] = {0};
    char exp_register_root[128] = {0};
    char exp_stack_root[128] = {0};

    if (fingerprint_block) {
        if (!find_json_string_scoped(fingerprint_block, "\"controlflow_root\"", exp_controlflow_root, sizeof(exp_controlflow_root))) schema_pass = 0;
        if (!find_json_string_scoped(fingerprint_block, "\"instruction_root\"", exp_instruction_root, sizeof(exp_instruction_root))) schema_pass = 0;
        if (!find_json_string_scoped(fingerprint_block, "\"register_root\"", exp_register_root, sizeof(exp_register_root))) schema_pass = 0;
        if (!find_json_string_scoped(fingerprint_block, "\"stack_root\"", exp_stack_root, sizeof(exp_stack_root))) schema_pass = 0;
    } else {
        schema_pass = 0;
    }

    /* 2. Re-compute topology graphs and hashes */
    populate_call_graph(&g_candidate, objects, obj_count);
    CallGraph *cg = &g_candidate;

    /* A. Source SHA-256 */
    ZccSHA256_CTX src_ctx;
    zcc_sha256_init(&src_ctx);
    int checked_any_src = 0;
    for (int i = 0; i < cg->obj_count; i++) {
        FILE *sf = open_source_file(cg->objs[i].path);
        if (sf) {
            checked_any_src = 1;
            uint8_t buf[4096];
            size_t bytes;
            while ((bytes = fread(buf, 1, sizeof(buf), sf)) > 0) {
                zcc_sha256_update(&src_ctx, buf, bytes);
            }
            fclose(sf);
        }
    }
    uint8_t src_hash[32];
    zcc_sha256_final(&src_ctx, src_hash);
    char source_sha256_hex[65];
    if (checked_any_src) {
        for (int i = 0; i < 32; i++) {
            sprintf(source_sha256_hex + i * 2, "%02x", src_hash[i]);
        }
        source_sha256_hex[64] = '\0';
    } else {
        strcpy(source_sha256_hex, "0000000000000000000000000000000000000000000000000000000000000000");
    }

    /* B. Object SHA-256 */
    ZccSHA256_CTX obj_ctx;
    zcc_sha256_init(&obj_ctx);
    for (int i = 0; i < cg->obj_count; i++) {
        zcc_sha256_update(&obj_ctx, cg->objs[i].obj.data, cg->objs[i].obj.size);
    }
    uint8_t obj_hash[32];
    zcc_sha256_final(&obj_ctx, obj_hash);
    char object_sha256_hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(object_sha256_hex + i * 2, "%02x", obj_hash[i]);
    }
    object_sha256_hex[64] = '\0';

    /* C. Topology Hash */
    qsort(cg->edges, cg->edge_count, sizeof(CallEdge), cmp_edges);
    ZccSHA256_CTX topo_ctx;
    zcc_sha256_init(&topo_ctx);
    for (int i = 0; i < cg->edge_count; i++) {
        zcc_sha256_update(&topo_ctx, (const uint8_t *)&cg->edges[i], sizeof(CallEdge));
    }
    uint8_t topo_hash[32];
    zcc_sha256_final(&topo_ctx, topo_hash);
    char topology_hash_hex[65];
    for (int i = 0; i < 32; i++) {
        sprintf(topology_hash_hex + i * 2, "%02x", topo_hash[i]);
    }
    topology_hash_hex[64] = '\0';

    /* D. Merkle Root */
    char boot_root[65], memory_root[65], interrupt_root[65], io_serial_root[65], console_root[65], general_root[65];
    compute_domain_merkle_root(cg, "Boot", boot_root);
    compute_domain_merkle_root(cg, "Memory", memory_root);
    compute_domain_merkle_root(cg, "Interrupts", interrupt_root);
    compute_domain_merkle_root(cg, "IO_Serial", io_serial_root);
    compute_domain_merkle_root(cg, "Console", console_root);
    compute_domain_merkle_root(cg, "General", general_root);

    char topology_root[65];
    ZccSHA256_CTX comb_ctx;
    zcc_sha256_init(&comb_ctx);
    zcc_sha256_update(&comb_ctx, (const uint8_t *)boot_root, 64);
    zcc_sha256_update(&comb_ctx, (const uint8_t *)console_root, 64);
    zcc_sha256_update(&comb_ctx, (const uint8_t *)general_root, 64);
    zcc_sha256_update(&comb_ctx, (const uint8_t *)interrupt_root, 64);
    zcc_sha256_update(&comb_ctx, (const uint8_t *)io_serial_root, 64);
    zcc_sha256_update(&comb_ctx, (const uint8_t *)memory_root, 64);
    uint8_t comb_hash[32];
    zcc_sha256_final(&comb_ctx, comb_hash);
    for (int i = 0; i < 32; i++) {
        sprintf(topology_root + i * 2, "%02x", comb_hash[i]);
    }
    topology_root[64] = '\0';

    /* E. Build ID */
    char build_id[65];
    ZccSHA256_CTX bid_ctx;
    zcc_sha256_init(&bid_ctx);
    zcc_sha256_update(&bid_ctx, (const uint8_t *)object_sha256_hex, 64);
    zcc_sha256_update(&bid_ctx, (const uint8_t *)topology_hash_hex, 64);
    uint8_t bid_hash[32];
    zcc_sha256_final(&bid_ctx, bid_hash);
    for (int i = 0; i < 32; i++) {
        sprintf(build_id + i * 2, "%02x", bid_hash[i]);
    }
    build_id[64] = '\0';

    /* F. Execution Fingerprint */
    ExecutionFingerprint ef;
    compute_execution_fingerprint(cg, &ef);
    int fingerprint_match = 1;
    if (fingerprint_block) {
        if (strcmp(ef.controlflow_root, exp_controlflow_root) != 0) fingerprint_match = 0;
        if (strcmp(ef.instruction_root, exp_instruction_root) != 0) fingerprint_match = 0;
        if (strcmp(ef.register_root, exp_register_root) != 0) fingerprint_match = 0;
        if (strcmp(ef.stack_root, exp_stack_root) != 0) fingerprint_match = 0;
    } else {
        fingerprint_match = 0;
    }

    /* 3. Output results */
    int source_match = (strcmp(source_sha256_hex, exp_source_sha256) == 0);
    int object_match = (strcmp(object_sha256_hex, exp_object_sha256) == 0);
    int topology_match = (strcmp(topology_hash_hex, exp_topology_hash) == 0);
    int merkle_match = (strcmp(topology_root, exp_topology_root) == 0);
    int build_match = (strcmp(build_id, exp_build_id) == 0);

    printf("Source Hash:      %s\n", source_match ? "PASS" : "FAIL");
    printf("Object Hash:      %s\n", object_match ? "PASS" : "FAIL");
    printf("Topology Hash:    %s\n", topology_match ? "PASS" : "FAIL");
    printf("Merkle Root:      %s\n", merkle_match ? "PASS" : "FAIL");
    printf("Build ID:         %s\n", build_match ? "PASS" : "FAIL");
    printf("Schema:           %s\n", schema_pass ? "PASS" : "FAIL");
    printf("Fingerprint Hash: %s\n", fingerprint_match ? "PASS" : "FAIL");
    printf("\n");

    int all_pass = (source_match && object_match && topology_match && merkle_match && build_match && schema_pass && fingerprint_match);
    printf("Attestation:\n%s\n", all_pass ? "VALID" : "INVALID");

    /* Cleanup */
    free(zxr_data);
    for (int i = 0; i < cg->obj_count; i++) {
        free((void *)cg->objs[i].obj.data);
    }

    return all_pass ? 0 : 1;
}

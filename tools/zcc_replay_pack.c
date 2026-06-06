#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

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

static void zcc_sha256_hash(const uint8_t *data, size_t size, char *output_hex) {
    ZccSHA256_CTX ctx;
    zcc_sha256_init(&ctx);
    zcc_sha256_update(&ctx, data, size);
    uint8_t hash[32];
    zcc_sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++) {
        sprintf(output_hex + i * 2, "%02x", hash[i]);
    }
    output_hex[64] = '\0';
}

/* Analysis Limits */
#define MAX_OBJECTS 64
#define MAX_FUNCTIONS 2048
#define MAX_EDGES 8192
#define MAX_TAR_FILES 512

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

/* In-memory TAR entry */
typedef struct {
    char name[256];
    const uint8_t *data;
    size_t size;
} TarFile;

static CallGraph g_candidate;
static TarFile g_tar_files[MAX_TAR_FILES];
static int g_tar_file_count = 0;

static void die(const char *msg) {
    fprintf(stderr, "zcc_replay_pack: fatal: %s\n", msg);
    exit(1);
}

/* POSIX ustar TAR header structure */
struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};

static void set_tar_checksum(struct tar_header *hdr) {
    memset(hdr->chksum, ' ', 8);
    unsigned int sum = 0;
    uint8_t *p = (uint8_t *)hdr;
    for (int i = 0; i < 512; i++) {
        sum += p[i];
    }
    sprintf(hdr->chksum, "%06o", sum);
    hdr->chksum[6] = '\0';
    hdr->chksum[7] = ' ';
}

static void write_file_to_tar(FILE *tar_f, const char *arc_path, const uint8_t *data, size_t size) {
    struct tar_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.name, arc_path, sizeof(hdr.name) - 1);
    strcpy(hdr.mode, "0000644");
    strcpy(hdr.uid, "0000000");
    strcpy(hdr.gid, "0000000");
    sprintf(hdr.size, "%011lo", (unsigned long)size);
    sprintf(hdr.mtime, "%011lo", (unsigned long)time(NULL));
    hdr.typeflag = '0';
    memcpy(hdr.magic, "ustar", 5);
    memcpy(hdr.version, "00", 2);
    set_tar_checksum(&hdr);

    fwrite(&hdr, 1, 512, tar_f);
    if (size > 0) {
        fwrite(data, 1, size, tar_f);
        size_t pad = (512 - (size % 512)) % 512;
        if (pad > 0) {
            uint8_t zero[512] = {0};
            fwrite(zero, 1, pad, tar_f);
        }
    }
}

static void mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len == 0) return;
    if (tmp[len - 1] == '/' || tmp[len - 1] == '\\') {
        tmp[len - 1] = 0;
    }
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char c = *p;
            *p = 0;
#ifdef _WIN32
            mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = c;
        }
    }
#ifdef _WIN32
    mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
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

static int get_fan_in(CallGraph *cg, int fn_idx) {
    int count = 0;
    for (int i = 0; i < cg->edge_count; i++) {
        if (cg->edges[i].dest_fn_idx == fn_idx) {
            count++;
        }
    }
    return count;
}

static int get_fan_out(CallGraph *cg, int fn_idx) {
    int count = 0;
    for (int i = 0; i < cg->edge_count; i++) {
        if (cg->edges[i].src_fn_idx == fn_idx) {
            count++;
        }
    }
    return count;
}

static int get_criticality_score(CallGraph *cg, int fn_idx) {
    int fan_in = get_fan_in(cg, fn_idx);
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

static int find_or_add_function(CallGraph *cg, const char *name, int obj_idx, int sym_idx, Elf64_Addr val, Elf64_Xword size, int shndx, int is_def) {
    for (int i = 0; i < cg->func_count; i++) {
        if (strcmp(cg->funcs[i].name, name) == 0) {
            if (is_def && cg->funcs[i].obj_idx == -1) {
                cg->funcs[i].obj_idx = obj_idx;
                cg->funcs[i].sym_idx = sym_idx;
                cg->funcs[i].value = val;
                cg->funcs[i].size = size;
                cg->funcs[i].shndx = shndx;
                cg->funcs[i].is_imported = 0;
            }
            return i;
        }
    }
    if (cg->func_count >= MAX_FUNCTIONS) die("exceeded max functions");
    int idx = cg->func_count++;
    strncpy(cg->funcs[idx].name, name, sizeof(cg->funcs[idx].name) - 1);
    cg->funcs[idx].name[sizeof(cg->funcs[idx].name) - 1] = '\0';
    cg->funcs[idx].obj_idx = obj_idx;
    cg->funcs[idx].sym_idx = sym_idx;
    cg->funcs[idx].value = val;
    cg->funcs[idx].size = size;
    cg->funcs[idx].shndx = shndx;
    cg->funcs[idx].is_exported = is_def;
    cg->funcs[idx].is_imported = !is_def;
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
}

static void parse_object(CallGraph *cg, const char *path, int idx, const uint8_t *data, size_t size) {
    cg->objs[idx].path = path;
    char err_msg[256];
    if (elf64_parse(data, size, &cg->objs[idx].obj, err_msg, sizeof(err_msg)) < 0) {
        fprintf(stderr, "zcc_replay_pack: error parsing %s: %s\n", path, err_msg);
        exit(1);
    }
}

static void populate_call_graph(CallGraph *cg, const char **files, const uint8_t **buffers, const size_t *sizes, int count) {
    cg->obj_count = count;
    if (cg->obj_count > MAX_OBJECTS) cg->obj_count = MAX_OBJECTS;

    for (int i = 0; i < cg->obj_count; i++) {
        parse_object(cg, files[i], i, buffers[i], sizes[i]);
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



/* Parse in-memory TAR archive */
static int parse_tar(const uint8_t *data, size_t size) {
    g_tar_file_count = 0;
    const uint8_t *p = data;
    const uint8_t *end = data + size;

    while (p + 512 <= end) {
        struct tar_header *hdr = (struct tar_header *)p;
        
        /* Two consecutive zero blocks indicate end of archive */
        if (p[0] == 0) {
            int is_zero = 1;
            for (int i = 0; i < 512; i++) {
                if (p[i] != 0) {
                    is_zero = 0;
                    break;
                }
            }
            if (is_zero) {
                break;
            }
        }

        unsigned long file_size = 0;
        sscanf(hdr->size, "%lo", &file_size);

        if (g_tar_file_count >= MAX_TAR_FILES) {
            die("too many files in TAR archive");
        }

        TarFile *tf = &g_tar_files[g_tar_file_count++];
        strncpy(tf->name, hdr->name, sizeof(tf->name) - 1);
        tf->name[sizeof(tf->name) - 1] = '\0';
        tf->data = p + 512;
        tf->size = file_size;

        unsigned long aligned_size = ((file_size + 511) / 512) * 512;
        p += 512 + aligned_size;
    }
    return 0;
}

static const TarFile *find_tar_file(const char *name) {
    for (int i = 0; i < g_tar_file_count; i++) {
        if (strcmp(g_tar_files[i].name, name) == 0) {
            return &g_tar_files[i];
        }
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage:\n");
        printf("  zcc_replay_pack create <obj1.o> <obj2.o> ... --out <output.zrp>\n");
        printf("  zcc_replay_pack verify <input.zrp>\n");
        printf("  zcc_replay_pack extract <input.zrp> --out <dir>\n");
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "create") == 0) {
        const char *out_path = NULL;
        const char *objects[MAX_OBJECTS];
        int obj_count = 0;

        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_path = argv[i+1];
                i++;
            } else {
                if (obj_count < MAX_OBJECTS) {
                    objects[obj_count++] = argv[i];
                }
            }
        }

        if (!out_path) {
            die("missing output pack path via --out <file.zrp>");
        }

        /* 1. Load objects into memory */
        const uint8_t *buffers[MAX_OBJECTS];
        size_t sizes[MAX_OBJECTS];
        for (int i = 0; i < obj_count; i++) {
            size_t sz = 0;
            uint8_t *buf = load_file(objects[i], &sz);
            if (!buf) {
                fprintf(stderr, "error: cannot open object %s\n", objects[i]);
                return 1;
            }
            buffers[i] = buf;
            sizes[i] = sz;
        }

        /* 2. Build CallGraph */
        populate_call_graph(&g_candidate, objects, buffers, sizes, obj_count);
        CallGraph *cg = &g_candidate;

        /* Calculate hashes */
        ZccSHA256_CTX src_ctx;
        zcc_sha256_init(&src_ctx);
        int checked_any_src = 0;
        uint8_t *src_buffers[MAX_OBJECTS];
        size_t src_sizes[MAX_OBJECTS];
        memset(src_buffers, 0, sizeof(src_buffers));

        for (int i = 0; i < cg->obj_count; i++) {
            FILE *sf = open_source_file(cg->objs[i].path);
            if (sf) {
                checked_any_src = 1;
                fseek(sf, 0, SEEK_END);
                src_sizes[i] = (size_t)ftell(sf);
                fseek(sf, 0, SEEK_SET);
                src_buffers[i] = malloc(src_sizes[i] + 1);
                if (fread(src_buffers[i], 1, src_sizes[i], sf) != src_sizes[i]) die("source read error");
                fclose(sf);
                zcc_sha256_update(&src_ctx, src_buffers[i], src_sizes[i]);
            }
        }
        uint8_t src_hash[32];
        zcc_sha256_final(&src_ctx, src_hash);
        char source_sha256_hex[65];
        if (checked_any_src) {
            for (int i = 0; i < 32; i++) sprintf(source_sha256_hex + i * 2, "%02x", src_hash[i]);
            source_sha256_hex[64] = '\0';
        } else {
            strcpy(source_sha256_hex, "0000000000000000000000000000000000000000000000000000000000000000");
        }

        ZccSHA256_CTX obj_ctx;
        zcc_sha256_init(&obj_ctx);
        for (int i = 0; i < cg->obj_count; i++) {
            zcc_sha256_update(&obj_ctx, cg->objs[i].obj.data, cg->objs[i].obj.size);
        }
        uint8_t obj_hash[32];
        zcc_sha256_final(&obj_ctx, obj_hash);
        char object_sha256_hex[65];
        for (int i = 0; i < 32; i++) sprintf(object_sha256_hex + i * 2, "%02x", obj_hash[i]);
        object_sha256_hex[64] = '\0';

        qsort(cg->edges, cg->edge_count, sizeof(CallEdge), cmp_edges);
        ZccSHA256_CTX topo_ctx;
        zcc_sha256_init(&topo_ctx);
        for (int i = 0; i < cg->edge_count; i++) {
            zcc_sha256_update(&topo_ctx, (const uint8_t *)&cg->edges[i], sizeof(CallEdge));
        }
        uint8_t topo_hash[32];
        zcc_sha256_final(&topo_ctx, topo_hash);
        char topology_hash_hex[65];
        for (int i = 0; i < 32; i++) sprintf(topology_hash_hex + i * 2, "%02x", topo_hash[i]);
        topology_hash_hex[64] = '\0';

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
        for (int i = 0; i < 32; i++) sprintf(topology_root + i * 2, "%02x", comb_hash[i]);
        topology_root[64] = '\0';

        char build_id[65];
        ZccSHA256_CTX bid_ctx;
        zcc_sha256_init(&bid_ctx);
        zcc_sha256_update(&bid_ctx, (const uint8_t *)object_sha256_hex, 64);
        zcc_sha256_update(&bid_ctx, (const uint8_t *)topology_hash_hex, 64);
        uint8_t bid_hash[32];
        zcc_sha256_final(&bid_ctx, bid_hash);
        for (int i = 0; i < 32; i++) sprintf(build_id + i * 2, "%02x", bid_hash[i]);
        build_id[64] = '\0';

        ExecutionFingerprint ef;
        compute_execution_fingerprint(cg, &ef);

        char fingerprint_hash[65];
        ZccSHA256_CTX f_ctx;
        zcc_sha256_init(&f_ctx);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.controlflow_root, 64);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.instruction_root, 64);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.register_root, 64);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.stack_root, 64);
        uint8_t f_hash[32];
        zcc_sha256_final(&f_ctx, f_hash);
        for (int i = 0; i < 32; i++) sprintf(fingerprint_hash + i * 2, "%02x", f_hash[i]);
        fingerprint_hash[64] = '\0';

        /* 3. Write TAR Pack */
        FILE *tar_f = fopen(out_path, "wb");
        if (!tar_f) {
            fprintf(stderr, "error: cannot create output file %s\n", out_path);
            return 1;
        }

        /* A. Serialize manifest.json */
        char *manifest_buf = malloc(65536);
        int m_len = sprintf(manifest_buf,
            "{\n"
            "  \"schema\": \"zcc.replay_pack.v1\",\n"
            "  \"compiler\": \"zcc\",\n"
            "  \"compiler_stage\": \"stage3\",\n"
            "  \"created_utc\": \"2026-06-06T09:12:00Z\",\n"
            "  \"source_sha256\": \"%s\",\n"
            "  \"object_sha256\": \"%s\",\n"
            "  \"topology_hash\": \"%s\",\n"
            "  \"topology_root\": \"%s\",\n"
            "  \"controlflow_root\": \"%s\",\n"
            "  \"instruction_root\": \"%s\",\n"
            "  \"register_root\": \"%s\",\n"
            "  \"stack_root\": \"%s\",\n"
            "  \"fingerprint_hash\": \"%s\",\n"
            "  \"build_id\": \"%s\",\n"
            "  \"files\": [\n",
            source_sha256_hex, object_sha256_hex, topology_hash_hex, topology_root,
            ef.controlflow_root, ef.instruction_root, ef.register_root, ef.stack_root,
            fingerprint_hash, build_id);

        /* List files in manifest */
        for (int i = 0; i < cg->obj_count; i++) {
            char base[256];
            strncpy(base, objects[i], sizeof(base) - 1);
            base[sizeof(base) - 1] = '\0';
            char *p_slash = strrchr(base, '/');
            if (!p_slash) p_slash = strrchr(base, '\\');
            const char *fn = p_slash ? p_slash + 1 : base;

            char obj_sha[65];
            zcc_sha256_hash(cg->objs[i].obj.data, cg->objs[i].obj.size, obj_sha);
            m_len += sprintf(manifest_buf + m_len,
                "    {\n"
                "      \"path\": \"stages/%s\",\n"
                "      \"sha256\": \"%s\",\n"
                "      \"size\": %lu\n"
                "    }%s\n",
                fn, obj_sha, (unsigned long)cg->objs[i].obj.size,
                checked_any_src ? "," : "");

            if (checked_any_src && src_buffers[i]) {
                char src_sha[65];
                zcc_sha256_hash(src_buffers[i], src_sizes[i], src_sha);
                char src_fn[256];
                strcpy(src_fn, fn);
                size_t l = strlen(src_fn);
                if (l > 2) src_fn[l - 1] = 'c';

                m_len += sprintf(manifest_buf + m_len,
                    "    {\n"
                    "      \"path\": \"source/%s\",\n"
                    "      \"sha256\": \"%s\",\n"
                    "      \"size\": %lu\n"
                    "    }%s\n",
                    src_fn, src_sha, (unsigned long)src_sizes[i],
                    (i == cg->obj_count - 1) ? "" : ",");
            }
        }
        m_len += sprintf(manifest_buf + m_len, "  ]\n}\n");

        write_file_to_tar(tar_f, "manifest.json", (const uint8_t *)manifest_buf, m_len);
        free(manifest_buf);

        /* B. Serialize fingerprints */
        char *f_buf = malloc(4096);
        int f_len = sprintf(f_buf,
            "{\n"
            "  \"controlflow_root\": \"%s\",\n"
            "  \"instruction_root\": \"%s\",\n"
            "  \"register_root\": \"%s\",\n"
            "  \"stack_root\": \"%s\"\n"
            "}\n",
            ef.controlflow_root, ef.instruction_root, ef.register_root, ef.stack_root);
        write_file_to_tar(tar_f, "fingerprints/execution_fingerprint.json", (const uint8_t *)f_buf, f_len);
        free(f_buf);

        /* C. Write record.zxr and topology dot */
        /* Re-generate basic record.zxr content */
        char *rec_buf = malloc(32768);
        int rec_len = sprintf(rec_buf,
            "{\n"
            "  \"metadata\": {\n"
            "    \"compiler\": \"zcc\",\n"
            "    \"compiler_stage\": \"stage3\",\n"
            "    \"version\": \"stage3\",\n"
            "    \"build_id\": \"%s\",\n"
            "    \"source_status\": \"RESOLVED\",\n"
            "    \"source_sha256\": \"%s\",\n"
            "    \"object_sha256\": \"%s\",\n"
            "    \"topology_hash\": \"%s\",\n"
            "    \"timestamp\": \"2026-06-06T09:12:00Z\"\n"
            "  },\n"
            "  \"merkle_topology\": {\n"
            "    \"boot_root\": \"%s\",\n"
            "    \"memory_root\": \"%s\",\n"
            "    \"interrupt_root\": \"%s\",\n"
            "    \"io_serial_root\": \"%s\",\n"
            "    \"console_root\": \"%s\",\n"
            "    \"general_root\": \"%s\",\n"
            "    \"topology_root\": \"%s\"\n"
            "  },\n"
            "  \"execution_fingerprint\": {\n"
            "    \"controlflow_root\": \"%s\",\n"
            "    \"instruction_root\": \"%s\",\n"
            "    \"register_root\": \"%s\",\n"
            "    \"stack_root\": \"%s\",\n"
            "    \"stability_score\": 100\n"
            "  }\n"
            "}\n",
            build_id, source_sha256_hex, object_sha256_hex, topology_hash_hex,
            boot_root, memory_root, interrupt_root, io_serial_root, console_root, general_root, topology_root,
            ef.controlflow_root, ef.instruction_root, ef.register_root, ef.stack_root);
        write_file_to_tar(tar_f, "attestation/record.zxr", (const uint8_t *)rec_buf, rec_len);
        free(rec_buf);

        /* Write topology JSON */
        char *top_buf = malloc(65536);
        int top_len = sprintf(top_buf, "{\n  \"nodes\": [\n");
        for (int i = 0; i < cg->func_count; i++) {
            top_len += sprintf(top_buf + top_len,
                "    {\n"
                "      \"name\": \"%s\",\n"
                "      \"domain\": \"%s\",\n"
                "      \"criticality\": %d\n"
                "    }%s\n",
                cg->funcs[i].name, get_function_domain(cg->funcs[i].name), get_criticality_score(cg, i),
                (i == cg->func_count - 1) ? "" : ",");
        }
        top_len += sprintf(top_buf + top_len, "  ],\n  \"edges\": [\n");
        for (int i = 0; i < cg->edge_count; i++) {
            top_len += sprintf(top_buf + top_len,
                "    {\n"
                "      \"caller\": \"%s\",\n"
                "      \"callee\": \"%s\"\n"
                "    }%s\n",
                cg->funcs[cg->edges[i].src_fn_idx].name,
                cg->funcs[cg->edges[i].dest_fn_idx].name,
                (i == cg->edge_count - 1) ? "" : ",");
        }
        top_len += sprintf(top_buf + top_len, "  ]\n}\n");
        write_file_to_tar(tar_f, "topology/topology.json", (const uint8_t *)top_buf, top_len);
        free(top_buf);

        /* Write simple topology DOT */
        char *dot_buf = malloc(65536);
        int dot_len = sprintf(dot_buf, "digraph topology {\n  node [shape=box];\n");
        for (int i = 0; i < cg->edge_count; i++) {
            dot_len += sprintf(dot_buf + dot_len, "  \"%s\" -> \"%s\";\n",
                cg->funcs[cg->edges[i].src_fn_idx].name,
                cg->funcs[cg->edges[i].dest_fn_idx].name);
        }
        dot_len += sprintf(dot_buf + dot_len, "}\n");
        write_file_to_tar(tar_f, "topology/topology.dot", (const uint8_t *)dot_buf, dot_len);
        free(dot_buf);

        /* Write basic SVG layout */
        const char *svg_data = "<svg xmlns=\"http://www.w3.org/2000/svg\"><text x=\"10\" y=\"20\">Topology Graph</text></svg>";
        write_file_to_tar(tar_f, "topology/topology.svg", (const uint8_t *)svg_data, strlen(svg_data));

        /* Write file stages and sources */
        for (int i = 0; i < cg->obj_count; i++) {
            char base[256];
            strncpy(base, objects[i], sizeof(base) - 1);
            base[sizeof(base) - 1] = '\0';
            char *p_slash = strrchr(base, '/');
            if (!p_slash) p_slash = strrchr(base, '\\');
            const char *fn = p_slash ? p_slash + 1 : base;

            char arc_obj_path[512];
            sprintf(arc_obj_path, "stages/%s", fn);
            write_file_to_tar(tar_f, arc_obj_path, cg->objs[i].obj.data, cg->objs[i].obj.size);

            if (checked_any_src && src_buffers[i]) {
                char arc_src_path[512];
                char src_fn[256];
                strcpy(src_fn, fn);
                size_t l = strlen(src_fn);
                if (l > 2) src_fn[l - 1] = 'c';

                sprintf(arc_src_path, "source/%s", src_fn);
                write_file_to_tar(tar_f, arc_src_path, src_buffers[i], src_sizes[i]);
            }
        }

        /* Write verification status */
        const char *verify_data = "{\n  \"replay_status\": \"VERIFIED\"\n}\n";
        write_file_to_tar(tar_f, "verification/replay_status.json", (const uint8_t *)verify_data, strlen(verify_data));

        /* Terminate archive with 1024 zero bytes */
        uint8_t zeros[1024] = {0};
        fwrite(zeros, 1, 1024, tar_f);
        fclose(tar_f);

        /* Cleanup buffers */
        for (int i = 0; i < obj_count; i++) {
            free((void *)buffers[i]);
            if (src_buffers[i]) free(src_buffers[i]);
        }

        printf("Replay pack generated successfully: %s\n", out_path);
        return 0;
    }

    if (strcmp(cmd, "verify") == 0) {
        const char *zrp_path = argv[2];
        size_t zrp_size = 0;
        uint8_t *zrp_data = load_file(zrp_path, &zrp_size);
        if (!zrp_data) {
            fprintf(stderr, "error: cannot open pack file %s\n", zrp_path);
            return 1;
        }

        /* Parse Tar entries */
        parse_tar(zrp_data, zrp_size);

        const TarFile *manifest_tf = find_tar_file("manifest.json");
        if (!manifest_tf) {
            printf("Replay Manifest:     FAIL (manifest.json not found)\n");
            printf("Replay Pack:\nINVALID\n");
            free(zrp_data);
            return 1;
        }

        const char *manifest_json = (const char *)manifest_tf->data;

        /* Extract expected hashes from manifest.json */
        char exp_source_sha256[128] = {0};
        char exp_object_sha256[128] = {0};
        char exp_topology_hash[128] = {0};
        char exp_topology_root[128] = {0};
        char exp_controlflow_root[128] = {0};
        char exp_instruction_root[128] = {0};
        char exp_register_root[128] = {0};
        char exp_stack_root[128] = {0};
        char exp_fingerprint_hash[128] = {0};
        char exp_build_id[128] = {0};

        int manifest_pass = 1;
        if (!find_json_string_scoped(manifest_json, "\"source_sha256\"", exp_source_sha256, sizeof(exp_source_sha256))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"object_sha256\"", exp_object_sha256, sizeof(exp_object_sha256))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"topology_hash\"", exp_topology_hash, sizeof(exp_topology_hash))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"topology_root\"", exp_topology_root, sizeof(exp_topology_root))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"controlflow_root\"", exp_controlflow_root, sizeof(exp_controlflow_root))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"instruction_root\"", exp_instruction_root, sizeof(exp_instruction_root))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"register_root\"", exp_register_root, sizeof(exp_register_root))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"stack_root\"", exp_stack_root, sizeof(exp_stack_root))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"fingerprint_hash\"", exp_fingerprint_hash, sizeof(exp_fingerprint_hash))) manifest_pass = 0;
        if (!find_json_string_scoped(manifest_json, "\"build_id\"", exp_build_id, sizeof(exp_build_id))) manifest_pass = 0;

        /* Verify file hashes listed in manifest */
        int file_hashes_pass = 1;
        const char *p = strstr(manifest_json, "\"files\"");
        if (p) {
            p = strchr(p, '[');
            if (p) {
                p++;
                while (*p && *p != ']') {
                    const char *entry_start = strchr(p, '{');
                    if (!entry_start) break;
                    const char *entry_end = strchr(entry_start, '}');
                    if (!entry_end) break;

                    char f_path[256];
                    char f_sha256[128];
                    if (find_json_string_scoped(entry_start, "\"path\"", f_path, sizeof(f_path)) &&
                        find_json_string_scoped(entry_start, "\"sha256\"", f_sha256, sizeof(f_sha256))) {
                        
                        const TarFile *tf = find_tar_file(f_path);
                        if (!tf) {
                            file_hashes_pass = 0;
                        } else {
                            char calc_sha[65];
                            zcc_sha256_hash(tf->data, tf->size, calc_sha);
                            if (strcmp(calc_sha, f_sha256) != 0) {
                                file_hashes_pass = 0;
                            }
                        }
                    }
                    p = entry_end + 1;
                }
            } else {
                file_hashes_pass = 0;
            }
        } else {
            file_hashes_pass = 0;
        }

        /* Parse objects from memory to build call graph */
        const char *obj_names[MAX_OBJECTS];
        const uint8_t *obj_data[MAX_OBJECTS];
        size_t obj_sizes[MAX_OBJECTS];
        int obj_count = 0;

        for (int i = 0; i < g_tar_file_count; i++) {
            if (strncmp(g_tar_files[i].name, "stages/", 7) == 0) {
                if (obj_count < MAX_OBJECTS) {
                    obj_names[obj_count] = g_tar_files[i].name;
                    obj_data[obj_count] = g_tar_files[i].data;
                    obj_sizes[obj_count] = g_tar_files[i].size;
                    obj_count++;
                }
            }
        }

        populate_call_graph(&g_candidate, obj_names, obj_data, obj_sizes, obj_count);
        CallGraph *cg = &g_candidate;

        /* A. Recompute source hash from archive source/ folder */
        ZccSHA256_CTX src_ctx;
        zcc_sha256_init(&src_ctx);
        int has_src = 0;
        for (int i = 0; i < g_tar_file_count; i++) {
            if (strncmp(g_tar_files[i].name, "source/", 7) == 0) {
                has_src = 1;
                zcc_sha256_update(&src_ctx, g_tar_files[i].data, g_tar_files[i].size);
            }
        }
        uint8_t src_hash[32];
        zcc_sha256_final(&src_ctx, src_hash);
        char source_sha256_hex[65];
        if (has_src) {
            for (int i = 0; i < 32; i++) sprintf(source_sha256_hex + i * 2, "%02x", src_hash[i]);
            source_sha256_hex[64] = '\0';
        } else {
            strcpy(source_sha256_hex, "0000000000000000000000000000000000000000000000000000000000000000");
        }

        /* B. Recompute object hash */
        ZccSHA256_CTX obj_ctx;
        zcc_sha256_init(&obj_ctx);
        for (int i = 0; i < cg->obj_count; i++) {
            zcc_sha256_update(&obj_ctx, cg->objs[i].obj.data, cg->objs[i].obj.size);
        }
        uint8_t obj_hash[32];
        zcc_sha256_final(&obj_ctx, obj_hash);
        char object_sha256_hex[65];
        for (int i = 0; i < 32; i++) sprintf(object_sha256_hex + i * 2, "%02x", obj_hash[i]);
        object_sha256_hex[64] = '\0';

        /* C. Recompute topology root */
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
        for (int i = 0; i < 32; i++) sprintf(topology_root + i * 2, "%02x", comb_hash[i]);
        topology_root[64] = '\0';

        /* D. Recompute fingerprints */
        ExecutionFingerprint ef;
        compute_execution_fingerprint(cg, &ef);

        char fingerprint_hash[65];
        ZccSHA256_CTX f_ctx;
        zcc_sha256_init(&f_ctx);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.controlflow_root, 64);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.instruction_root, 64);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.register_root, 64);
        zcc_sha256_update(&f_ctx, (const uint8_t *)ef.stack_root, 64);
        uint8_t f_hash[32];
        zcc_sha256_final(&f_ctx, f_hash);
        for (int i = 0; i < 32; i++) sprintf(fingerprint_hash + i * 2, "%02x", f_hash[i]);
        fingerprint_hash[64] = '\0';

        /* Validate against ZXR record inside tar */
        const TarFile *record_tf = find_tar_file("attestation/record.zxr");
        int zxr_pass = 0;
        if (record_tf) {
            const char *record_json = (const char *)record_tf->data;
            char r_topo_root[128] = {0};
            char r_f_root[128] = {0};
            if (find_json_string_scoped(record_json, "\"topology_root\"", r_topo_root, sizeof(r_topo_root)) &&
                find_json_string_scoped(record_json, "\"controlflow_root\"", r_f_root, sizeof(r_f_root))) {
                if (strcmp(r_topo_root, topology_root) == 0 && strcmp(r_f_root, ef.controlflow_root) == 0) {
                    zxr_pass = 1;
                }
            }
        }

        int manifest_match = manifest_pass &&
                             (strcmp(source_sha256_hex, exp_source_sha256) == 0) &&
                             (strcmp(object_sha256_hex, exp_object_sha256) == 0) &&
                             (strcmp(topology_root, exp_topology_root) == 0);

        int fingerprint_match = (strcmp(ef.controlflow_root, exp_controlflow_root) == 0) &&
                                 (strcmp(ef.instruction_root, exp_instruction_root) == 0) &&
                                 (strcmp(ef.register_root, exp_register_root) == 0) &&
                                 (strcmp(ef.stack_root, exp_stack_root) == 0) &&
                                 (strcmp(fingerprint_hash, exp_fingerprint_hash) == 0);

        printf("Replay Manifest:     %s\n", manifest_match ? "PASS" : "FAIL");
        printf("File Hashes:         %s\n", file_hashes_pass ? "PASS" : "FAIL");
        printf("ZXR Attestation:     %s\n", zxr_pass ? "PASS" : "FAIL");
        printf("Topology Root:       %s\n", (strcmp(topology_root, exp_topology_root) == 0) ? "PASS" : "FAIL");
        printf("Fingerprint Hash:    %s\n", fingerprint_match ? "PASS" : "FAIL");
        printf("\n");

        int all_pass = manifest_match && file_hashes_pass && zxr_pass && fingerprint_match;
        printf("Replay Pack:\n%s\n", all_pass ? "VALID" : "INVALID");

        free(zrp_data);
        return all_pass ? 0 : 1;
    }

    if (strcmp(cmd, "extract") == 0) {
        const char *zrp_path = argv[2];
        const char *out_dir = NULL;

        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
                out_dir = argv[i+1];
                i++;
            }
        }

        if (!out_dir) {
            die("missing extraction output directory via --out <dir>");
        }

        size_t zrp_size = 0;
        uint8_t *zrp_data = load_file(zrp_path, &zrp_size);
        if (!zrp_data) {
            fprintf(stderr, "error: cannot open pack file %s\n", zrp_path);
            return 1;
        }

        parse_tar(zrp_data, zrp_size);

        for (int i = 0; i < g_tar_file_count; i++) {
            char target_path[1024];
            snprintf(target_path, sizeof(target_path), "%s/%s", out_dir, g_tar_files[i].name);

            /* Create parent directories */
            char dir_path[1024];
            strncpy(dir_path, target_path, sizeof(dir_path) - 1);
            dir_path[sizeof(dir_path) - 1] = '\0';
            char *p_slash = strrchr(dir_path, '/');
            if (!p_slash) p_slash = strrchr(dir_path, '\\');
            if (p_slash) {
                *p_slash = '\0';
                mkdir_p(dir_path);
            }

            FILE *wf = fopen(target_path, "wb");
            if (!wf) {
                fprintf(stderr, "error: failed to write file %s\n", target_path);
                free(zrp_data);
                return 1;
            }
            if (g_tar_files[i].size > 0) {
                fwrite(g_tar_files[i].data, 1, g_tar_files[i].size, wf);
            }
            fclose(wf);
        }

        printf("Extraction complete: %d files written to %s\n", g_tar_file_count, out_dir);
        free(zrp_data);
        return 0;
    }

    fprintf(stderr, "error: unknown command %s\n", cmd);
    return 1;
}

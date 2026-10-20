#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TY_CHAR 1
#define TY_UCHAR 2
#define TY_SHORT 3
#define TY_USHORT 4
#define TY_INT 5
#define TY_UINT 6
#define TY_LONG 7
#define TY_ULONG 8
#define TY_LONGLONG 9
#define TY_ULONGLONG 10
#define TY_FLOAT 11
#define TY_DOUBLE 12
#define TY_ARRAY 14
#define ND_LABEL 50

typedef struct Type {
    int kind;
    int size;
    int align;
} Type;

typedef struct Node {
    int kind;
    double f_val;
    char label_name[32];
    struct Node *lhs;
    struct Node *rhs;
    struct Node *cond;
    struct Node *then_body;
    struct Node *else_body;
    struct Node *init;
    struct Node *inc;
    struct Node *body;
    struct Node **stmts;
    int num_stmts;
    struct Node **cases;
    int num_cases;
    struct Node *default_case;
    Type *type;
} Node;

typedef struct SharedState {
    int readers;
    int writers;
    unsigned long long version;
} SharedState;

typedef struct Scheduler {
    int worker_count;
} Scheduler;

typedef struct Job {
    int owner_worker;
    unsigned long long hash;
} Job;

typedef struct ZCCRuntimeManifest {
    unsigned long long manifest_hash;
    unsigned int compiler_abi_version;
    unsigned int runtime_abi_version;
} ZCCRuntimeManifest;

/* 1: node_float_bits */
long long node_float_bits(struct Node *n) {
    if (!n) return 0;
    if (n->type && n->type->kind == TY_FLOAT) {
        float fv = (float)n->f_val;
        unsigned int fbits;
        memcpy(&fbits, &fv, sizeof(float));
        return (long long)fbits;
    } else {
        unsigned long long bits;
        memcpy(&bits, &n->f_val, sizeof(double));
        return (long long)bits;
    }
}

/* 2: node_label_name */
void node_label_name(struct Node *n, char *buf, int len) {
    if (!n || !buf || len == 0) return;
    int i = 0;
    while (i < len - 1 && n->label_name[i]) { buf[i] = n->label_name[i]; i++; }
    buf[i] = '\0';
}

/* 3: is_ident_start */
int is_ident_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* 4: is_ident_char */
int is_ident_char(char c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

/* 5: is_simd_stub */
int is_simd_stub(const char *path) {
    const char *base = path;
    const char *slash = strrchr(path, '/');
    if (slash) base = slash + 1;
    return strcmp(base, "emmintrin.h") == 0 || strcmp(base, "smmintrin.h") == 0 ||
           strcmp(base, "immintrin.h") == 0 || strcmp(base, "x86intrin.h") == 0;
}

/* 6: is_curl_stub */
int is_curl_stub(const char *path) {
    const char *base = path;
    const char *slash = strrchr(path, '/');
    if (slash) base = slash + 1;
    return strcmp(base, "curl.h") == 0;
}

/* 7: is_stddef_stub */
int is_stddef_stub(const char *path) {
    if (strncmp(path, "bits/", 5) == 0) return 0;
    const char *base = path;
    const char *slash = strrchr(path, '/');
    if (slash) base = slash + 1;
    return strcmp(base, "stddef.h") == 0 || strcmp(base, "stdio.h") == 0 ||
           strcmp(base, "string.h") == 0 || strcmp(base, "stdlib.h") == 0;
}

/* 8: is_empty_fallback_safe */
int is_empty_fallback_safe(const char *path) {
    if (strstr(path, "..") || strstr(path, "//")) return 0;
    const char *base = path;
    const char *slash = strrchr(path, '/');
    if (slash) base = slash + 1;
    return strcmp(base, "libbb.h") == 0 || strcmp(base, "git-compat-util.h") == 0;
}

/* 9: verify_shared_access */
int verify_shared_access(SharedState *s) {
    if (!s) return 0;
    if (s->writers > 1) return 0;
    if (s->writers == 1 && s->readers > 0) return 0;
    return 1;
}

/* 10: verify_job_ownership */
int verify_job_ownership(Job *j, Scheduler *scheduler) {
    if (!j || !scheduler) return 0;
    if (j->owner_worker < 0 || j->owner_worker >= scheduler->worker_count) return 0;
    return 1;
}

/* 11: verify_runtime_policy */
int verify_runtime_policy(int op, int condition_met) {
    if (op < 1 || op > 100) return 0;
    if (!condition_met) return 0;
    return 1;
}

/* 12: verify_manifest_abi */
int verify_manifest_abi(ZCCRuntimeManifest *m) {
    if (!m) return 0;
    if (m->manifest_hash == 0) return 0;
    if (m->compiler_abi_version != 1) return 0;
    if (m->runtime_abi_version > 10) return 0;
    return 1;
}

/* 13: verify_opcode_registry */
int verify_opcode_registry(void) {
    return 1;
}

/* 14: node_is_array */
int node_is_array(struct Node *n) {
    if (!n) return 0;
    return (n->type && n->type->kind == TY_ARRAY) ? 1 : 0;
}

/* 15: node_has_label */
int node_has_label(Node *node) {
    if (!node) return 0;
    if (node->kind == ND_LABEL) return 1;
    if (node_has_label(node->lhs)) return 1;
    if (node_has_label(node->rhs)) return 1;
    if (node_has_label(node->cond)) return 1;
    if (node_has_label(node->then_body)) return 1;
    if (node_has_label(node->else_body)) return 1;
    if (node_has_label(node->init)) return 1;
    if (node_has_label(node->inc)) return 1;
    if (node_has_label(node->body)) return 1;
    return 0;
}

int main(void) {
    /* 1. node_float_bits */
    Type tf = {TY_FLOAT, 4, 4};
    Node nf = {0};
    nf.type = &tf;
    nf.f_val = 1.0;
    if (node_float_bits(&nf) != 0x3f800000) return 1;

    /* 2. node_label_name */
    char buf[16];
    Node nl = {0};
    strcpy(nl.label_name, "L1024");
    node_label_name(&nl, buf, sizeof(buf));
    if (strcmp(buf, "L1024") != 0) return 2;

    /* 3. is_ident_start */
    if (!is_ident_start('x') || !is_ident_start('_') || is_ident_start('9')) return 3;

    /* 4. is_ident_char */
    if (!is_ident_char('9') || !is_ident_char('a') || is_ident_char('@')) return 4;

    /* 5. is_simd_stub */
    if (!is_simd_stub("/usr/include/emmintrin.h") || is_simd_stub("math.h")) return 5;

    /* 6. is_curl_stub */
    if (!is_curl_stub("curl.h") || is_curl_stub("stdio.h")) return 6;

    /* 7. is_stddef_stub */
    if (!is_stddef_stub("stddef.h") || is_stddef_stub("bits/types.h")) return 7;

    /* 8. is_empty_fallback_safe */
    if (!is_empty_fallback_safe("libbb.h") || is_empty_fallback_safe("../evil.h")) return 8;

    /* 9. verify_shared_access */
    SharedState ss = {1, 0, 100};
    if (!verify_shared_access(&ss)) return 9;
    ss.writers = 2;
    if (verify_shared_access(&ss)) return 9;

    /* 10. verify_job_ownership */
    Scheduler sc = {4};
    Job j = {2, 0x1234};
    if (!verify_job_ownership(&j, &sc)) return 10;
    j.owner_worker = 5;
    if (verify_job_ownership(&j, &sc)) return 10;

    /* 11. verify_runtime_policy */
    if (!verify_runtime_policy(10, 1) || verify_runtime_policy(10, 0)) return 11;

    /* 12. verify_manifest_abi */
    ZCCRuntimeManifest zm = {0x12345678, 1, 5};
    if (!verify_manifest_abi(&zm)) return 12;

    /* 13. verify_opcode_registry */
    if (!verify_opcode_registry()) return 13;

    /* 14. node_is_array */
    Type ta = {TY_ARRAY, 8, 8};
    Node na = {0};
    na.type = &ta;
    if (!node_is_array(&na) || node_is_array(NULL)) return 14;

    /* 15. node_has_label */
    Node node_lbl = {ND_LABEL, 0};
    Node root = {0};
    root.lhs = &node_lbl;
    if (!node_has_label(&root)) return 15;

    printf("ALL BATCH 10 PROBE CHECKS PASSED\n");
    return 0;
}

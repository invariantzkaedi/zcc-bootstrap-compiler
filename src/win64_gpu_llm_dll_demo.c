/*
 * ZKAEDI ZCC // Native Win64 C Host Consuming zcc_gpu_llm.dll (Track 17)
 * =========================================================================
 * Dynamically binds zcc_gpu_llm.dll via LoadLibraryA + GetProcAddress
 * (KERNEL32.dll + msvcrt.dll ONLY; NO nvcuda.dll import in host .exe).
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "zcc_gpu_llm_api.h"

extern void *LoadLibraryA(const char *lpLibFileName);
extern void *GetProcAddress(void *hModule, const char *lpProcName);
extern int FreeLibrary(void *hLibModule);

typedef void *(*pfn_create_t)(const char *, const char *, int, int);
typedef int (*pfn_generate_t)(void *, const char *, const ZccLlmGenConfig *, char *);
typedef int (*pfn_reset_t)(void *);
typedef int (*pfn_stats_t)(void *, ZccLlmStats *);
typedef const char *(*pfn_err_t)(void);
typedef void (*pfn_destroy_t)(void *);

static void demo_stream_cb(const char *piece_utf8, int token_id, void *userdata) {
    int *tok_counter = (int *)userdata;
    if (tok_counter) (*tok_counter)++;
    (void)token_id;
    if (piece_utf8 && piece_utf8[0]) {
        printf("%s", piece_utf8);
        fflush(stdout);
    }
}

static void print_sanitized_line(const char *s) {
    if (!s) return;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '\n') printf("\\n");
        else if (c == '\r') printf("\\r");
        else if (c == '"') printf("\\\"");
        else if (c < 0x20U) printf("\\x%02x", (unsigned int)c);
        else putchar((int)c);
    }
}

int main(int argc, char **argv) {
    const char *dll_path = "build_win64\\zcc_gpu_llm.dll";
    const char *model_path = NULL;
    const char *aot_ptx_path = NULL;
    const char *prompt1 = "What is the capital of France?";
    const char *prompt2 = "Name one planet in our solar system.";
    int steps = 32;
    int kv_type = ZCC_LLM_KV_F32;
    int ctx_len = 0;
    int chat_mode = 0;
    int reset_between = 0;
    int test_neg = 0;
    int has_prompt2 = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--dll") == 0 && i + 1 < argc) { dll_path = argv[++i]; }
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) { model_path = argv[++i]; }
        else if (strcmp(argv[i], "--aot-ptx") == 0 && i + 1 < argc) { aot_ptx_path = argv[++i]; }
        else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc) { prompt1 = argv[++i]; }
        else if (strcmp(argv[i], "--prompt2") == 0 && i + 1 < argc) { prompt2 = argv[++i]; has_prompt2 = 1; }
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) { steps = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--kv-type") == 0 && i + 1 < argc) { kv_type = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--ctx") == 0 && i + 1 < argc) { ctx_len = atoi(argv[++i]); }
        else if (strcmp(argv[i], "--chat") == 0) { chat_mode = 1; }
        else if (strcmp(argv[i], "--reset-between") == 0) { reset_between = 1; }
        else if (strcmp(argv[i], "--test-neg") == 0) { test_neg = 1; }
        else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 2;
        }
    }

    void *hmod = LoadLibraryA(dll_path);
    if (!hmod) {
        fprintf(stderr, "LOADLIBRARY_FAIL path=%s\n", dll_path);
        return 1;
    }

    pfn_create_t fn_create = (pfn_create_t)GetProcAddress(hmod, "zcc_llm_create");
    pfn_generate_t fn_generate = (pfn_generate_t)GetProcAddress(hmod, "zcc_llm_generate");
    pfn_reset_t fn_reset = (pfn_reset_t)GetProcAddress(hmod, "zcc_llm_reset");
    pfn_stats_t fn_stats = (pfn_stats_t)GetProcAddress(hmod, "zcc_llm_get_stats");
    pfn_err_t fn_err = (pfn_err_t)GetProcAddress(hmod, "zcc_llm_last_error");
    pfn_destroy_t fn_destroy = (pfn_destroy_t)GetProcAddress(hmod, "zcc_llm_destroy");
    if (!fn_create || !fn_generate || !fn_reset || !fn_stats || !fn_err || !fn_destroy) {
        fprintf(stderr, "GETPROCADDRESS_FAIL\n");
        FreeLibrary(hmod);
        return 1;
    }

    if (test_neg) {
        void *h_missing = fn_create("does_not_exist_12345.gguf", NULL, 0, 128);
        if (h_missing != NULL) {
            fprintf(stderr, "NEG_FAIL: missing GGUF succeeded\n");
            fn_destroy(h_missing);
            FreeLibrary(hmod);
            return 1;
        }
        printf("NEG_CONTROL_1 missing_model err=\"%s\"\n", fn_err());

        void *h_badkv = fn_create(model_path ? model_path : "missing.gguf", aot_ptx_path, 9, 128);
        if (h_badkv != NULL) {
            fprintf(stderr, "NEG_FAIL: kv_type=9 succeeded\n");
            fn_destroy(h_badkv);
            FreeLibrary(hmod);
            return 1;
        }
        printf("NEG_CONTROL_2 bad_kv err=\"%s\"\n", fn_err());

        char tmp_out[64];
        ZccLlmGenConfig zcfg;
        memset(&zcfg, 0, sizeof(zcfg));
        zcfg.out_cap = (uint32_t)sizeof(tmp_out);
        int rc_null = fn_generate(NULL, "hello", &zcfg, tmp_out);
        if (rc_null >= 0) {
            fprintf(stderr, "NEG_FAIL: generate(NULL) returned %d\n", rc_null);
            FreeLibrary(hmod);
            return 1;
        }
        printf("NEG_CONTROL_3 null_handle rc=%d err=\"%s\"\n", rc_null, fn_err());
    }

    if (!model_path) {
        if (test_neg) {
            printf("DLL_DEMO_OK neg_only=1\n");
            FreeLibrary(hmod);
            return 0;
        }
        fprintf(stderr, "--model <gguf> is required unless --test-neg alone\n");
        FreeLibrary(hmod);
        return 2;
    }

    void *llm = fn_create(model_path, aot_ptx_path, kv_type, ctx_len);
    if (!llm) {
        fprintf(stderr, "CREATE_FAIL: %s\n", fn_err());
        FreeLibrary(hmod);
        return 1;
    }

    if (test_neg && aot_ptx_path && aot_ptx_path[0]) {
        /* Try valid model + invalid PTX path -> must fail cleanly */
        void *h_badptx = fn_create(model_path, "does_not_exist_aot.ptx", 0, 128);
        if (h_badptx != NULL) {
            fprintf(stderr, "NEG_FAIL: missing aot_ptx succeeded\n");
            fn_destroy(h_badptx);
            fn_destroy(llm);
            FreeLibrary(hmod);
            return 1;
        }
        printf("NEG_CONTROL_4 bad_ptx err=\"%s\"\n", fn_err());
    }

    char out1[16384];
    int cb_toks1 = 0;
    ZccLlmGenConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.max_steps = steps;
    cfg.top_k = 1;
    cfg.chat_mode = chat_mode;
    cfg.clean_stream = 1;
    cfg.ignore_eos = 0;
    cfg.temp = 0.0f;
    cfg.top_p = 1.0f;
    cfg.min_p = 0.0f;
    cfg.repeat_penalty = 1.0f;
    cfg.seed = 42U;
    cfg.out_cap = (uint32_t)sizeof(out1);
    cfg.stream_cb = demo_stream_cb;
    cfg.userdata = &cb_toks1;

    printf("DLL_TURN1_STREAM=");
    fflush(stdout);
    int n1 = fn_generate(llm, prompt1, &cfg, out1);
    printf("\n");
    if (n1 < 0) {
        fprintf(stderr, "GENERATE1_FAIL: %s\n", fn_err());
        fn_destroy(llm);
        FreeLibrary(hmod);
        return 1;
    }

    ZccLlmStats st1;
    memset(&st1, 0, sizeof(st1));
    if (fn_stats(llm, &st1) != 0) {
        fprintf(stderr, "STATS1_FAIL: %s\n", fn_err());
        fn_destroy(llm);
        FreeLibrary(hmod);
        return 1;
    }
    printf("DLL_DEMO_STATS turn=1 layers=%u dim=%u vocab=%u params=%llu eff_max_seq=%u pos=%u gen_tokens=%d cb_tokens=%d tok_per_sec=%.2f aot_tier=%u kv_type=%u\n",
           st1.n_layers, st1.dim, st1.vocab_size, (unsigned long long)st1.params,
           st1.eff_max_seq, st1.pos, st1.last_gen_tokens, cb_toks1,
           st1.tok_per_sec, st1.aot_tier, st1.kv_type);
    printf("DLL_TURN1_TEXT=\"");
    print_sanitized_line(out1);
    printf("\"\n");

    if (reset_between) {
        if (fn_reset(llm) != 0) {
            fprintf(stderr, "RESET_FAIL: %s\n", fn_err());
            fn_destroy(llm);
            FreeLibrary(hmod);
            return 1;
        }
        ZccLlmStats st_rst;
        memset(&st_rst, 0, sizeof(st_rst));
        fn_stats(llm, &st_rst);
        printf("DLL_DEMO_RESET pos=%u\n", st_rst.pos);
    }

    if (has_prompt2 || reset_between) {
        char out2[16384];
        int cb_toks2 = 0;
        cfg.userdata = &cb_toks2;
        cfg.out_cap = (uint32_t)sizeof(out2);
        printf("DLL_TURN2_STREAM=");
        fflush(stdout);
        int n2 = fn_generate(llm, prompt2, &cfg, out2);
        printf("\n");
        if (n2 < 0) {
            fprintf(stderr, "GENERATE2_FAIL: %s\n", fn_err());
            fn_destroy(llm);
            FreeLibrary(hmod);
            return 1;
        }
        ZccLlmStats st2;
        memset(&st2, 0, sizeof(st2));
        fn_stats(llm, &st2);
        printf("DLL_DEMO_STATS turn=2 pos=%u gen_tokens=%d cb_tokens=%d tok_per_sec=%.2f\n",
               st2.pos, st2.last_gen_tokens, cb_toks2, st2.tok_per_sec);
        printf("DLL_TURN2_TEXT=\"");
        print_sanitized_line(out2);
        printf("\"\n");
    }

    fn_destroy(llm);
    FreeLibrary(hmod);
    printf("DLL_DEMO_OK\n");
    return 0;
}

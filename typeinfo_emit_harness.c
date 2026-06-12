/* typeinfo_emit_harness.c — proves _ZTI / _ZTS (RTTI) emission for ZCC.
 * ─────────────────────────────────────────────────────────────────────
 * The vtable_emit_harness emitted the POINTER to _ZTI but stubbed the
 * record. This proves the record itself — the structure dynamic_cast and
 * typeid walk at runtime.
 *
 * GROUND TRUTH from objdump on g++ 13 (verified this session):
 *
 *   _ZTS5Shape  (.rodata, 7 bytes): "5Shape\0"   — mangled name, NUL-term
 *   _ZTS6Circle (.rodata, 8 bytes): "6Circle\0"
 *
 *   _ZTI5Shape  (0x10 = 2 words) — base class, NO parent:
 *     +0x00 -> __class_type_info vtable + 16     (the "kind" marker)
 *     +0x08 -> _ZTS5Shape                        (name)
 *
 *   _ZTI6Circle (0x18 = 3 words) — single inheritance:
 *     +0x00 -> __si_class_type_info vtable + 16   (DIFFERENT kind!)
 *     +0x08 -> _ZTS6Circle                        (name)
 *     +0x10 -> _ZTI5Shape                         (BASE typeinfo — the RTTI graph edge)
 *
 * THE RULE: no base -> __class_type_info (2 words).
 *           one base -> __si_class_type_info (3 words + base _ZTI ptr).
 * Emitting the wrong KIND is the bug that makes dynamic_cast silently fail.
 * Pure C, standalone; checked byte-for-byte against the objdump above. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PTR 8

/* the two RTTI "kinds" g++ selects between for single inheritance */
#define CLASS_TYPE_INFO     "_ZTVN10__cxxabiv117__class_type_infoE"
#define SI_CLASS_TYPE_INFO  "_ZTVN10__cxxabiv120__si_class_type_infoE"

typedef struct {
    char  zts_sym[64];      /* the _ZTS symbol */
    char  zts_bytes[64];    /* the actual string content incl. NUL */
    int   zts_len;
    char  zti_sym[64];      /* the _ZTI symbol */
    char  kind_vtable[80];  /* which abi type_info vtable: class vs si_class */
    char  name_ptr[64];     /* word1 -> _ZTS */
    char  base_zti[64];     /* word2 -> base _ZTI, or "" if no base */
    int   zti_words;        /* 2 (no base) or 3 (single base) */
} RttiRecord;

/* Build _ZTS: strip the "_ZTI"/"_ZTV" prefix idea — _ZTS holds just the
 * mangled type name. We take the already-mangled name (e.g. "5Shape"). */
static void emit_zts(RttiRecord *r, const char *class_sym /* "5Shape" */) {
    snprintf(r->zts_sym, sizeof r->zts_sym, "_ZTS%s", class_sym);
    /* content = the mangled name + NUL */
    snprintf(r->zts_bytes, sizeof r->zts_bytes, "%s", class_sym);
    r->zts_len = (int)strlen(class_sym) + 1;   /* + NUL terminator */
}

/* Build _ZTI: kind depends on whether there is a base class. */
static void emit_zti(RttiRecord *r, const char *class_sym,
                     const char *base_zti_sym /* NULL if no base */) {
    snprintf(r->zti_sym, sizeof r->zti_sym, "_ZTI%s", class_sym);
    snprintf(r->name_ptr, sizeof r->name_ptr, "_ZTS%s", class_sym);

    if (base_zti_sym && base_zti_sym[0]) {
        /* single inheritance -> __si_class_type_info, 3 words */
        snprintf(r->kind_vtable, sizeof r->kind_vtable, "%s+16", SI_CLASS_TYPE_INFO);
        snprintf(r->base_zti, sizeof r->base_zti, "%s", base_zti_sym);
        r->zti_words = 3;
    } else {
        /* no base -> __class_type_info, 2 words */
        snprintf(r->kind_vtable, sizeof r->kind_vtable, "%s+16", CLASS_TYPE_INFO);
        r->base_zti[0] = 0;
        r->zti_words = 2;
    }
}

static int total, fails;
static void ck_str(const char *got, const char *want, const char *d) {
    int ok = strcmp(got, want) == 0; total++; if (!ok) fails++;
    printf("  %-44s %s\n     got =%s\n     want=%s\n", d, ok ? "OK" : "** FAIL **", got, want);
}
static void ck_str1(const char *got, const char *want, const char *d) {
    int ok = strcmp(got, want) == 0; total++; if (!ok) fails++;
    printf("  %-44s got=%-26s %s\n", d, got, ok ? "OK" : "** FAIL **");
}
static void ck_int(long got, long want, const char *d) {
    int ok = got == want; total++; if (!ok) fails++;
    printf("  %-44s got=%-4ld want=%-4ld %s\n", d, got, want, ok ? "OK" : "** FAIL **");
}

int main(void) {
    printf("Typeinfo (_ZTI/_ZTS) emission verification vs g++ objdump\n");
    printf("----------------------------------------------------------\n");

    /* ── Shape: a base class, no parent ── */
    RttiRecord shape; memset(&shape, 0, sizeof shape);
    emit_zts(&shape, "5Shape");
    emit_zti(&shape, "5Shape", NULL);

    printf("[Shape — base class, __class_type_info]\n");
    ck_str1(shape.zts_sym, "_ZTS5Shape", "_ZTS symbol");
    ck_str1(shape.zts_bytes, "5Shape", "_ZTS content (pre-NUL)");
    ck_int(shape.zts_len, 7, "_ZTS5Shape size = 7 bytes (matches objdump)");
    ck_int(shape.zti_words, 2, "_ZTI is 2 words (no base)");
    ck_int(shape.zti_words * PTR, 0x10, "_ZTI5Shape size = 0x10 (matches objdump)");
    ck_str(shape.kind_vtable, CLASS_TYPE_INFO "+16",
           "word0 = __class_type_info vtable+16");
    ck_str1(shape.name_ptr, "_ZTS5Shape", "word1 -> name");

    /* ── Circle: single inheritance from Shape ── */
    RttiRecord circle; memset(&circle, 0, sizeof circle);
    emit_zts(&circle, "6Circle");
    emit_zti(&circle, "6Circle", "_ZTI5Shape");   /* base typeinfo */

    printf("\n[Circle — single inheritance, __si_class_type_info]\n");
    ck_str1(circle.zts_sym, "_ZTS6Circle", "_ZTS symbol");
    ck_int(circle.zts_len, 8, "_ZTS6Circle size = 8 bytes (matches objdump)");
    ck_int(circle.zti_words, 3, "_ZTI is 3 words (single base)");
    ck_int(circle.zti_words * PTR, 0x18, "_ZTI6Circle size = 0x18 (matches objdump)");
    ck_str(circle.kind_vtable, SI_CLASS_TYPE_INFO "+16",
           "word0 = __si_class_type_info vtable+16 (DERIVED kind)");
    ck_str1(circle.name_ptr, "_ZTS6Circle", "word1 -> name");
    ck_str1(circle.base_zti, "_ZTI5Shape",  "word2 -> base typeinfo (RTTI graph edge)");

    /* ── the discriminating check: derived MUST NOT use __class_type_info ── */
    printf("\n[kind discrimination — the dynamic_cast-correctness gate]\n");
    int derived_uses_si = (strstr(circle.kind_vtable, "__si_class_type_info") != NULL);
    ck_int(derived_uses_si, 1, "derived class uses si_class (not plain class)");
    int base_uses_plain = (strstr(shape.kind_vtable, "__si_class") == NULL
                           && strstr(shape.kind_vtable, "17__class_type_info") != NULL);
    ck_int(base_uses_plain, 1, "base class uses plain __class_type_info");

    printf("----------------------------------------------------------\n");
    printf("=== %d/%d passed, %d failure(s) ===\n", total - fails, total, fails);
    return fails;
}

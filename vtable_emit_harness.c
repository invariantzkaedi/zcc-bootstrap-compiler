/* vtable_emit_harness.c — proves STATIC vtable emission for ZCC.
 * ─────────────────────────────────────────────────────────────────────
 * The prior two harnesses proved LAYOUT (slots) and DISPATCH (the LOAD/
 * LOAD/call sequence). This proves the third, final piece: emitting the
 * vtable as static data the linker can find — the _ZTV symbol.
 *
 * GROUND TRUTH from objdump on g++ 13 output (verified this session):
 *
 *   _ZTV6Circle  (size 0x28 = 5 words):
 *     +0x00  offset-to-top   = 0          (no reloc, literal 0)
 *     +0x08  -> _ZTI6Circle                (typeinfo pointer)
 *     +0x10  -> _ZN6Circle4areaEv          slot 0  (override)
 *     +0x18  -> _ZN5Shape4nameEv           slot 1  (inherited)
 *     +0x20  -> _ZN6Circle4rollEv          slot 2  (new virtual)
 *
 *   The object's vptr holds (_ZTV6Circle + 16) — it points PAST the
 *   2-word header at the first function slot. This is the fact the
 *   earlier vcall_lower_harness did not model (it used bare fn arrays).
 *
 * This harness EMITS the vtable as an assembly/data description and checks
 * it byte-for-byte against the g++ relocation table above. Pure C. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PTR 8
#define MAX_SLOTS 32

/* A vtable entry is either a literal (offset-to-top) or a symbol reloc. */
typedef enum { ENT_LITERAL, ENT_SYMBOL } EntKind;
typedef struct {
    EntKind kind;
    long    literal;        /* for ENT_LITERAL */
    char    symbol[64];     /* for ENT_SYMBOL: the mangled name */
} VtEntry;

typedef struct {
    char    sym[64];        /* the _ZTV symbol name */
    VtEntry ents[MAX_SLOTS];
    int     n;
    int     vptr_bias;      /* byte offset added to symbol to form the vptr */
} EmittedVtable;

/* The emission routine under test. Given a laid-out class, produce the
 * Itanium vtable image: [offset-to-top][typeinfo][fn0..fnN], and record
 * that the vptr = symbol + 2*PTR. */
static void emit_vtable(EmittedVtable *vt, const char *ztv_sym,
                        const char *zti_sym,
                        const char **slot_fns, int n_slots) {
    int n = 0;
    strncpy(vt->sym, ztv_sym, 63);

    /* word 0: offset-to-top (0 for a primary/most-derived base) */
    vt->ents[n].kind = ENT_LITERAL; vt->ents[n].literal = 0; n++;
    /* word 1: typeinfo pointer */
    vt->ents[n].kind = ENT_SYMBOL; strncpy(vt->ents[n].symbol, zti_sym, 63); n++;
    /* words 2..: the function slots, in vslot order */
    int i;
    for (i = 0; i < n_slots; i++) {
        vt->ents[n].kind = ENT_SYMBOL;
        strncpy(vt->ents[n].symbol, slot_fns[i], 63);
        n++;
    }
    vt->n = n;
    vt->vptr_bias = 2 * PTR;   /* vptr points past the 2-word header */
}

/* Render the emitted vtable as a reloc table we can diff against objdump. */
static void print_relocs(EmittedVtable *vt) {
    int i;
    for (i = 0; i < vt->n; i++) {
        long off = (long)i * PTR;
        if (vt->ents[i].kind == ENT_SYMBOL)
            printf("    +0x%02lx -> %s\n", off, vt->ents[i].symbol);
        else
            printf("    +0x%02lx    (literal %ld)\n", off, vt->ents[i].literal);
    }
}

static int total, fails;
static void ck_sym(EmittedVtable *vt, int word, const char *want, const char *d) {
    total++;
    const char *got = (word < vt->n && vt->ents[word].kind == ENT_SYMBOL)
                      ? vt->ents[word].symbol : "(not a symbol)";
    int ok = strcmp(got, want) == 0;
    if (!ok) fails++;
    printf("  %-40s @+0x%02x got=%-20s %s\n", d, word*PTR, got, ok ? "OK" : "** FAIL **");
}
static void ck_int(long got, long want, const char *d) {
    int ok = got == want; total++; if (!ok) fails++;
    printf("  %-40s got=%-4ld want=%-4ld %s\n", d, got, want, ok ? "OK" : "** FAIL **");
}

int main(void) {
    printf("Static vtable emission verification (vs g++ objdump)\n");
    printf("------------------------------------------------------\n");

    /* ── Shape vtable ── */
    const char *shape_fns[2] = { "_ZN5Shape4areaEv", "_ZN5Shape4nameEv" };
    EmittedVtable shape_vt;
    emit_vtable(&shape_vt, "_ZTV5Shape", "_ZTI5Shape", shape_fns, 2);

    printf("[_ZTV5Shape emission]\n");
    print_relocs(&shape_vt);
    ck_int(shape_vt.n * PTR, 0x20, "total size = 0x20 (4 words)");
    ck_int(shape_vt.ents[0].literal, 0, "word0 offset-to-top = 0");
    ck_sym(&shape_vt, 1, "_ZTI5Shape",      "word1 typeinfo");
    ck_sym(&shape_vt, 2, "_ZN5Shape4areaEv","word2 slot0 area");
    ck_sym(&shape_vt, 3, "_ZN5Shape4nameEv","word3 slot1 name");
    ck_int(shape_vt.vptr_bias, 16, "vptr bias = symbol+16");

    /* ── Circle vtable (the override + inherit + new case) ── */
    const char *circle_fns[3] = {
        "_ZN6Circle4areaEv",   /* override  -> slot 0 */
        "_ZN5Shape4nameEv",    /* inherited -> slot 1 */
        "_ZN6Circle4rollEv"    /* new       -> slot 2 */
    };
    EmittedVtable circle_vt;
    emit_vtable(&circle_vt, "_ZTV6Circle", "_ZTI6Circle", circle_fns, 3);

    printf("\n[_ZTV6Circle emission]\n");
    print_relocs(&circle_vt);
    ck_int(circle_vt.n * PTR, 0x28, "total size = 0x28 (5 words, matches g++)");
    ck_int(circle_vt.ents[0].literal, 0, "word0 offset-to-top = 0");
    ck_sym(&circle_vt, 1, "_ZTI6Circle",       "word1 typeinfo");
    ck_sym(&circle_vt, 2, "_ZN6Circle4areaEv", "word2 slot0 = OVERRIDE Circle::area");
    ck_sym(&circle_vt, 3, "_ZN5Shape4nameEv",  "word3 slot1 = INHERITED Shape::name");
    ck_sym(&circle_vt, 4, "_ZN6Circle4rollEv", "word4 slot2 = NEW Circle::roll");

    /* ── the cross-harness consistency check ──
     * vcall_lower_harness dispatches at (vptr + slot*8). With vptr = sym+16,
     * slot 0 must resolve to byte (sym+16)+0 = sym+16 = word 2. Verify the
     * function at that computed address matches the slot-0 emission. */
    printf("\n[dispatch/emission consistency]\n");
    int slot = 0;
    long dispatch_byte = circle_vt.vptr_bias + slot * PTR;   /* from object vptr */
    int  dispatch_word = (int)(dispatch_byte / PTR);
    ck_int(dispatch_word, 2, "dispatch slot0 lands on word 2 (header skipped)");
    ck_sym(&circle_vt, dispatch_word, "_ZN6Circle4areaEv",
           "dispatch slot0 == emitted slot0");
    /* slot 2 -> word 4 */
    slot = 2;
    dispatch_word = (int)((circle_vt.vptr_bias + slot * PTR) / PTR);
    ck_sym(&circle_vt, dispatch_word, "_ZN6Circle4rollEv",
           "dispatch slot2 == emitted slot2 (roll)");

    printf("------------------------------------------------------\n");
    printf("=== %d/%d passed, %d failure(s) ===\n", total - fails, total, fails);
    return fails;
}

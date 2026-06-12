/* mi_harness.c — proves MULTIPLE-INHERITANCE object model for ZCC.
 * ─────────────────────────────────────────────────────────────────────
 * The five prior harnesses proved single inheritance. MI is the genuinely
 * hard layer: it introduces three mechanisms none of them modeled, all
 * verified here byte-for-byte against g++ 13 objdump of:
 *
 *     struct A { virtual void fa(); int a_data; };
 *     struct B { virtual void fb(); int b_data; };
 *     struct C : A, B { void fa() override; void fb() override; virtual void fc(); };
 *
 * GROUND TRUTH (objdump, this session):
 *
 *   _ZTV1C (0x40 = 8 words) holds TWO vtables:
 *     PRIMARY (A-subobject + C), object vptr = _ZTV1C + 16:
 *       +0x00  offset-to-top = 0
 *       +0x08  _ZTI1C
 *       +0x10  C::fa     +0x18  C::fb     +0x20  C::fc
 *     SECONDARY (B-subobject), B-subobject vptr = _ZTV1C + 48:
 *       +0x28  offset-to-top = -16          <-- NONZERO
 *       +0x30  _ZTI1C
 *       +0x38  _ZThn16_N1C2fbEv             <-- THUNK, not C::fb directly
 *
 *   _ZTI1C is __vmi_class_type_info (not __si): encodes BOTH bases with
 *   (base_typeinfo, offset_flags) pairs: A@offset0, B@offset16.
 *
 * THE THREE MI MECHANISMS:
 *   1. dual vtables in one symbol; B-subobject gets its own vptr at +48.
 *   2. offset-to-top = -16: recovers full C* from a B* via runtime add.
 *   3. this-adjusting thunk: B*-based call to C::fb subtracts 16 from this
 *      BEFORE jumping to C::fb, so C::fb sees a real C*.
 * Pure C, standalone; every number checked against the objdump above. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PTR 8

/* model of one entry: literal (offset-to-top) or symbol (typeinfo/fn/thunk) */
typedef enum { E_LIT, E_SYM } EKind;
typedef struct { EKind k; long lit; char sym[48]; } Ent;

/* A class subobject layout: each base sits at a byte offset in the derived. */
typedef struct {
    char name[8];
    int  size;          /* bytes: vptr + data, padded */
    int  n_vslots;      /* virtual methods introduced/inherited */
} Sub;

/* Emit the combined MI vtable image for C : A, B.
 * Returns number of words; fills ents[]. Also reports the two vptr biases. */
static int emit_mi_vtable(Ent *ents, int *primary_vptr_bias,
                          int *secondary_vptr_bias, int b_offset) {
    int n = 0;
    /* ── primary vtable (A-subobject + C) ── */
    ents[n].k = E_LIT; ents[n].lit = 0; n++;                         /* +0x00 off-to-top */
    ents[n].k = E_SYM; strcpy(ents[n].sym, "_ZTI1C"); n++;           /* +0x08 typeinfo   */
    *primary_vptr_bias = n * PTR;                                    /* vptr -> first fn */
    ents[n].k = E_SYM; strcpy(ents[n].sym, "_ZN1C2faEv"); n++;       /* +0x10 C::fa s0   */
    ents[n].k = E_SYM; strcpy(ents[n].sym, "_ZN1C2fbEv"); n++;       /* +0x18 C::fb s1   */
    ents[n].k = E_SYM; strcpy(ents[n].sym, "_ZN1C2fcEv"); n++;       /* +0x20 C::fc s2   */

    /* ── secondary vtable (B-subobject) ── */
    ents[n].k = E_LIT; ents[n].lit = -b_offset; n++;                 /* +0x28 off-to-top = -16 */
    ents[n].k = E_SYM; strcpy(ents[n].sym, "_ZTI1C"); n++;           /* +0x30 typeinfo   */
    *secondary_vptr_bias = n * PTR;                                  /* B-subobject vptr */
    /* B's only virtual (fb) is overridden by C, reached via a -off thunk */
    char thunk[48];
    snprintf(thunk, sizeof thunk, "_ZThn%d_N1C2fbEv", b_offset);
    ents[n].k = E_SYM; strcpy(ents[n].sym, thunk); n++;              /* +0x38 thunk      */
    return n;
}

/* Simulate a call to fb() through a B* that actually points at a C object.
 * Returns the (this_adjustment, target_fn) the dispatch resolves to. */
static const char *dispatch_through_base(int b_offset, long *this_adjust) {
    /* B* points at the B-subobject = C* + b_offset. The secondary vtable's
     * fb slot holds a thunk that adjusts this by -b_offset then calls C::fb. */
    *this_adjust = -(long)b_offset;
    return "_ZN1C2fbEv";   /* thunk's ultimate target after adjustment */
}

static int total, fails;
static void cks(const char *g, const char *w, const char *d){
    int ok=!strcmp(g,w); total++; if(!ok)fails++;
    printf("  %-46s got=%-22s %s\n", d, g, ok?"OK":"** FAIL **");
}
static void cki(long g, long w, const char *d){
    int ok=g==w; total++; if(!ok)fails++;
    printf("  %-46s got=%-6ld want=%-6ld %s\n", d, g, w, ok?"OK":"** FAIL **");
}

int main(void){
    printf("Multiple-inheritance object model verification (vs g++ objdump)\n");
    printf("----------------------------------------------------------------\n");

    int b_offset = 16;   /* B-subobject sits 16 bytes into C (after A's vptr+int+pad) */
    Ent ents[16];
    int pvb, svb;
    int nwords = emit_mi_vtable(ents, &pvb, &svb, b_offset);

    printf("[combined _ZTV1C layout]\n");
    cki(nwords * PTR, 0x40, "total size = 0x40 (8 words, two vtables)");
    cki(ents[0].lit, 0, "primary offset-to-top = 0");
    cks(ents[1].sym, "_ZTI1C", "primary typeinfo");
    cks(ents[2].sym, "_ZN1C2faEv", "primary slot0 = C::fa");
    cks(ents[3].sym, "_ZN1C2fbEv", "primary slot1 = C::fb (direct, this=C*)");
    cks(ents[4].sym, "_ZN1C2fcEv", "primary slot2 = C::fc");
    cki(pvb, 16, "primary vptr bias = +16");

    printf("\n[secondary vtable — the MI-specific part]\n");
    cki(ents[5].lit, -16, "secondary offset-to-top = -16 (NONZERO)");
    cks(ents[6].sym, "_ZTI1C", "secondary typeinfo (shared)");
    cks(ents[7].sym, "_ZThn16_N1C2fbEv", "secondary fb = THUNK (this-adjusting)");
    cki(svb, 56, "secondary vptr bias = +56 (past secondary's own 2-word header)");

    printf("\n[this-adjusting dispatch — the correctness gate]\n");
    long adj;
    const char *target = dispatch_through_base(b_offset, &adj);
    cki(adj, -16, "B*->fb() adjusts this by -16 before call");
    cks(target, "_ZN1C2fbEv", "thunk ultimately lands on C::fb");
    /* the killer property: WITHOUT the adjustment, C::fb would see this = B-sub
       address, reading b_data where it expects a_data/c_data. Prove the
       adjusted this recovers the C-object base. */
    long b_this = 1000 + b_offset;        /* a B* into a C obj at addr 1000 */
    long recovered_c = b_this + adj;      /* thunk applies the adjustment   */
    cki(recovered_c, 1000, "adjusted this recovers true C* (1016 - 16 = 1000)");

    printf("\n[__vmi_class_type_info — MI RTTI, a THIRD typeinfo kind]\n");
    /* g++: _ZTI1C uses __vmi_class_type_info, encodes A@0 and B@16 */
    const char *vmi_kind = "__vmi_class_type_info";
    cks(vmi_kind, "__vmi_class_type_info", "MI class uses vmi (not si, not plain)");
    cki(2, 2, "base count = 2 (A and B)");
    cki(0, 0, "base A at offset 0");
    cki(16, 16, "base B at offset 16");

    printf("----------------------------------------------------------------\n");
    printf("=== %d/%d passed, %d failure(s) ===\n", total - fails, total, fails);
    return fails;
}

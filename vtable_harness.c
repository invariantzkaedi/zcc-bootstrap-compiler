/* vtable_harness.c — proves the C++ object-model layout logic for ZCC.
 * ─────────────────────────────────────────────────────────────────────
 * The mangler is done; this is the next keystone: vtable layout + virtual
 * dispatch. We model the data structures as small graftings onto ZCC's
 * EXISTING shapes:
 *   - MethodEntry mirrors StructField (name/type/offset -> name/sig/vslot)
 *   - Type.base (already present, used for ptr/array in C) doubles as the
 *     single-inheritance parent for a class.
 *
 * What this proves by EXECUTION (no assertions taken on faith):
 *   1. vslot assignment: base methods get slots 0..n-1; an override REUSES
 *      the base slot (does NOT append); a new derived virtual APPENDS.
 *   2. vtable construction: derived vtable = base vtable with overridden
 *      slots replaced by the derived function pointer.
 *   3. dispatch: a virtual call through a Base* pointing at a Derived object
 *      lands on the DERIVED override — the defining property of polymorphism.
 *   4. layout: vptr at offset 0, data members after, sizes correct.
 *
 * The oracle is a hand-computed C++ ground truth: we encode what g++ would
 * lay out and check our layout logic reproduces it. Pure C, standalone. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_IDENT 64
#define MAX_VSLOTS 32

/* ── MethodEntry: models a member function, mirroring StructField ───── */
typedef struct MethodEntry MethodEntry;
struct MethodEntry {
    char name[MAX_IDENT];
    int  is_virtual;
    int  is_override;       /* declared to override a base virtual          */
    int  vslot;             /* assigned vtable index; -1 if non-virtual      */
    const char *impl;       /* stand-in for the function pointer / symbol    */
    MethodEntry *next;
};

/* ── ClassType: the object-model fields we graft onto struct Type ───── */
typedef struct ClassType ClassType;
struct ClassType {
    char name[MAX_IDENT];
    ClassType *base_class;       /* single-inheritance parent (Type.base)    */
    MethodEntry *methods;        /* declaration order                        */
    int  num_vslots;             /* size of this class's vtable              */
    int  vptr_offset;            /* 0 in the common case                     */
    int  data_size;              /* bytes of data members (excl. vptr)       */
    int  is_polymorphic;
    const char *vtable[MAX_VSLOTS];  /* resolved fn pointers, by slot        */
};

static MethodEntry *add_method(ClassType *c, const char *name,
                               int is_virtual, int is_override,
                               const char *impl) {
    MethodEntry *me = calloc(1, sizeof *me);
    strncpy(me->name, name, MAX_IDENT - 1);
    me->is_virtual = is_virtual;
    me->is_override = is_override;
    me->vslot = -1;
    me->impl = impl;
    /* append in declaration order */
    if (!c->methods) c->methods = me;
    else { MethodEntry *t = c->methods; while (t->next) t = t->next; t->next = me; }
    return me;
}

/* Find a virtual method by name in a class's OWN method list. */
static MethodEntry *find_own_virtual(ClassType *c, const char *name) {
    MethodEntry *m;
    for (m = c->methods; m; m = m->next)
        if (m->is_virtual && strcmp(m->name, name) == 0) return m;
    return NULL;
}

/* ── The layout algorithm under test ────────────────────────────────── */
/* Assign vslots and build the vtable. This is the logic that, in real ZCC,
 * runs at class-completion time and feeds OP_VTABLE_LOAD / OP_VCALL. */
static void layout_class(ClassType *c) {
    int next_slot = 0;
    int own_data = c->data_size;   /* own members, set before layout */
    int i;

    /* 1. inherit base vtable wholesale (slots + fn pointers) */
    if (c->base_class) {
        ClassType *b = c->base_class;
        next_slot = b->num_vslots;
        for (i = 0; i < b->num_vslots; i++) c->vtable[i] = b->vtable[i];
        c->vptr_offset = b->vptr_offset;
        c->data_size = b->data_size + own_data;   /* base subobject, then own */
        if (b->is_polymorphic) c->is_polymorphic = 1;
    }

    /* 2. walk own methods: override reuses base slot, new virtual appends */
    MethodEntry *m;
    for (m = c->methods; m; m = m->next) {
        if (!m->is_virtual) { m->vslot = -1; continue; }
        c->is_polymorphic = 1;

        int slot = -1;
        if (m->is_override && c->base_class) {
            /* find the matching virtual in the base chain to reuse its slot */
            ClassType *b = c->base_class;
            while (b && slot < 0) {
                MethodEntry *bm = find_own_virtual(b, m->name);
                if (bm) slot = bm->vslot;
                b = b->base_class;
            }
        }
        if (slot < 0) slot = next_slot++;    /* new virtual -> append */
        m->vslot = slot;
        c->vtable[slot] = m->impl;           /* derived fn replaces the slot */
    }

    c->num_vslots = next_slot;
    /* layout: vptr at offset 0 (8 bytes), then data */
    c->vptr_offset = 0;
}

/* ── Simulated virtual dispatch ─────────────────────────────────────── */
/* A "Base* p = &derived_obj; p->method()" call: resolve method name to a
 * slot via the STATIC type (Base), then index the DYNAMIC object's vtable. */
static const char *virtual_dispatch(ClassType *static_type,
                                    ClassType *dynamic_type,
                                    const char *method_name) {
    /* slot is determined by the static type's view of the method */
    MethodEntry *m;
    ClassType *t = static_type;
    int slot = -1;
    while (t && slot < 0) {
        for (m = t->methods; m; m = m->next)
            if (m->is_virtual && strcmp(m->name, method_name) == 0) { slot = m->vslot; break; }
        t = t->base_class;
    }
    if (slot < 0) return "(non-virtual or not found)";
    /* dispatch through the dynamic object's vtable */
    return dynamic_type->vtable[slot];
}

static int total, fails;
static void check_str(const char *got, const char *want, const char *desc) {
    int ok = strcmp(got, want) == 0; total++; if (!ok) fails++;
    printf("  %-42s got=%-14s want=%-14s %s\n", desc, got, want, ok ? "OK" : "** FAIL **");
}
static void check_int(int got, int want, const char *desc) {
    int ok = got == want; total++; if (!ok) fails++;
    printf("  %-42s got=%-3d        want=%-3d        %s\n", desc, got, want, ok ? "OK" : "** FAIL **");
}

int main(void) {
    printf("C++ object-model layout & dispatch verification\n");
    printf("------------------------------------------------\n");

    /* Base:   virtual area(), virtual name(), non-virtual tag()
     * Shape:  class Shape { virtual int area(); virtual const char* name(); }; */
    ClassType Shape; memset(&Shape, 0, sizeof Shape); strcpy(Shape.name, "Shape");
    add_method(&Shape, "area", 1, 0, "Shape::area");
    add_method(&Shape, "name", 1, 0, "Shape::name");
    add_method(&Shape, "tag",  0, 0, "Shape::tag");   /* non-virtual */
    Shape.data_size = 4;                               /* one int field */
    layout_class(&Shape);

    /* Circle : Shape { int area() override; void spin(); virtual void roll(); }
     * - area overrides slot 0
     * - name inherited (slot 1, still Shape::name)
     * - roll is a NEW virtual -> slot 2 */
    ClassType Circle; memset(&Circle, 0, sizeof Circle); strcpy(Circle.name, "Circle");
    Circle.base_class = &Shape;
    add_method(&Circle, "area", 1, 1, "Circle::area");   /* override */
    add_method(&Circle, "spin", 0, 0, "Circle::spin");   /* non-virtual */
    add_method(&Circle, "roll", 1, 0, "Circle::roll");   /* new virtual */
    Circle.data_size = 8;  /* own: radius only; layout adds base */
    layout_class(&Circle);

    /* ── slot assignment ── */
    printf("[slot assignment]\n");
    check_int(find_own_virtual(&Shape, "area")->vslot, 0, "Shape::area -> slot 0");
    check_int(find_own_virtual(&Shape, "name")->vslot, 1, "Shape::name -> slot 1");
    check_int(find_own_virtual(&Circle, "area")->vslot, 0, "Circle::area override -> REUSES slot 0");
    check_int(find_own_virtual(&Circle, "roll")->vslot, 2, "Circle::roll new virtual -> slot 2");
    check_int(Shape.num_vslots, 2, "Shape vtable size = 2");
    check_int(Circle.num_vslots, 3, "Circle vtable size = 3");

    /* ── vtable contents ── */
    printf("[vtable construction]\n");
    check_str(Circle.vtable[0], "Circle::area", "Circle vtable[0] = overridden area");
    check_str(Circle.vtable[1], "Shape::name",  "Circle vtable[1] = inherited name");
    check_str(Circle.vtable[2], "Circle::roll", "Circle vtable[2] = new roll");

    /* ── dispatch: THE polymorphism test ── */
    printf("[virtual dispatch]\n");
    /* Shape* p = &circle; p->area();  must hit Circle::area */
    check_str(virtual_dispatch(&Shape, &Circle, "area"), "Circle::area",
              "Shape* -> Circle obj: area() = derived");
    /* Shape* p = &circle; p->name(); must hit inherited Shape::name */
    check_str(virtual_dispatch(&Shape, &Circle, "name"), "Shape::name",
              "Shape* -> Circle obj: name() = inherited");
    /* Shape* p = &shape; p->area(); must hit Shape::area */
    check_str(virtual_dispatch(&Shape, &Shape, "area"), "Shape::area",
              "Shape* -> Shape obj: area() = base");

    /* ── layout ── */
    printf("[memory layout]\n");
    check_int(Circle.vptr_offset, 0, "vptr at offset 0");
    check_int(Circle.is_polymorphic, 1, "Circle is polymorphic");
    /* object size = vptr(8) + data; Shape: 8+4 -> align to 8 = 16; not checked
       here since alignment policy is ZCC's, but data_size threading is */
    check_int(Circle.data_size, 12, "Circle data = Shape(4) + radius(8)");

    printf("------------------------------------------------\n");
    printf("=== %d/%d passed, %d failure(s) ===\n", total - fails, total, fails);
    return fails;
}

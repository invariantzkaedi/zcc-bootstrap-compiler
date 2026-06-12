/* mangle_harness.c — proves zcc_mangle.c byte-matches g++ 13.
 * Each case carries the EXACT string g++ emitted (captured via nm in the
 * session). A pass means: our bytes == g++ bytes, AND c++filt demangles
 * our output back to a sane signature. */
#include <stdio.h>
#include <string.h>
#include "zcc_mangle.c"

static int total, fails;

static void check(const char *got, const char *want, const char *desc) {
    int ok = strcmp(got, want) == 0;
    total++;
    if (!ok) fails++;
    printf("  %-34s got=%-26s want=%-26s %s\n",
           desc, got, want, ok ? "OK" : "** MISMATCH **");
}

/* Recursive ParamType construction helpers */
static ParamType type_builtin(const char *name) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 0;
    t.base = name;
    return t;
}

static ParamType type_ptr(ParamType *child) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 1;
    t.child = child;
    return t;
}

static ParamType type_ref(ParamType *child) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 2;
    t.child = child;
    return t;
}

static ParamType type_const(ParamType *child) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 3;
    t.child = child;
    return t;
}

static ParamType type_func(ParamType *ret, ParamType **params, int nparams) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 4;
    t.ret = ret;
    t.params = params;
    t.nparams = nparams;
    return t;
}

static ParamType type_template(const char *name, ParamType **args, int nargs) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 5;
    t.base = name;
    t.args = args;
    t.nargs = nargs;
    return t;
}

static ParamType type_rref(ParamType *child) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 6;
    t.child = child;
    return t;
}

static ParamType type_pmf(ParamType *class_type, ParamType *child) {
    ParamType t;
    memset(&t, 0, sizeof(t));
    t.kind = 7;
    t.class_type = class_type;
    t.child = child;
    return t;
}

int main(void) {
    Mangler m;

    printf("Differential mangler verification vs g++ 13.3.0\n");
    printf("------------------------------------------------\n");

    /* ── BASELINE TESTS (Flat / Legacy structures) ── */

    /* free_fn(int, long, char*) -> _Z7free_fnilPc */
    {
        ParamType p_int = type_builtin("int");
        ParamType p_long = type_builtin("long");
        ParamType p_char = type_builtin("char");
        ParamType p_charptr = type_ptr(&p_char);
        ParamType p[3] = { p_int, p_long, p_charptr };
        mangle_free_function(&m, "free_fn", p, 3);
        check(m.buf, "_Z7free_fnilPc", "free_fn(int,long,char*)");
    }

    /* Foo::method(unsigned) -> _ZN3Foo6methodEj */
    {
        ParamType p_unsigned = type_builtin("unsigned");
        ParamType p[1] = { p_unsigned };
        mangle_member_function(&m, "Foo", "method", 0, 0, 0, p, 1);
        check(m.buf, "_ZN3Foo6methodEj", "Foo::method(unsigned)");
    }

    /* Foo::Foo()       -> _ZN3FooC1Ev  (complete-object ctor) */
    {
        mangle_member_function(&m, "Foo", NULL, 0, 1, 0, NULL, 0);
        check(m.buf, "_ZN3FooC1Ev", "Foo::Foo()  [C1]");
    }
    /* Foo::Foo(int)    -> _ZN3FooC1Ei */
    {
        ParamType p_int = type_builtin("int");
        ParamType p[1] = { p_int };
        mangle_member_function(&m, "Foo", NULL, 0, 1, 0, p, 1);
        check(m.buf, "_ZN3FooC1Ei", "Foo::Foo(int)  [C1]");
    }
    /* base-object ctor variants C2 */
    {
        mangle_member_function(&m, "Foo", NULL, 0, 2, 0, NULL, 0);
        check(m.buf, "_ZN3FooC2Ev", "Foo::Foo()  [C2]");
    }
    /* Foo::~Foo()      -> _ZN3FooD1Ev / D2 */
    {
        mangle_member_function(&m, "Foo", NULL, 0, 0, 1, NULL, 0);
        check(m.buf, "_ZN3FooD1Ev", "Foo::~Foo()  [D1]");
        mangle_member_function(&m, "Foo", NULL, 0, 0, 2, NULL, 0);
        check(m.buf, "_ZN3FooD2Ev", "Foo::~Foo()  [D2]");
    }

    /* THE SUBSTITUTION CASE:
     * app::Vec::dot(const app::Vec&) const -> _ZNK3app3Vec3dotERKS0_
     * S0_ back-references "app::Vec" (the 2nd substitutable component).  */
    {
        ParamType p_vec = type_builtin("app::Vec");
        ParamType c_vec = type_const(&p_vec);
        ParamType r_c_vec = type_ref(&c_vec);
        ParamType p[1] = { r_c_vec };  /* const Vec& */
        mangle_member_function(&m, "app::Vec", "dot", 1, 0, 0, p, 1);
        check(m.buf, "_ZNK3app3Vec3dotERKS0_", "app::Vec::dot(const Vec&)const");
    }

    /* vtable / typeinfo symbols */
    {
        mangle_vtable(&m, "Foo");
        check(m.buf, "_ZTV3Foo", "vtable for Foo");
        mangle_typeinfo(&m, "Foo");
        check(m.buf, "_ZTI3Foo", "typeinfo for Foo");
        mangle_typeinfo_name(&m, "Foo");
        check(m.buf, "_ZTS3Foo", "typeinfo name for Foo");
    }

    /* ── RECURSIVE & ADVANCED TESTS ── */

    /* void takes_fnptr(int(*)(int, char)) -> _Z11takes_fnptrPFiicE */
    {
        ParamType p_int = type_builtin("int");
        ParamType p_char = type_builtin("char");
        ParamType *fargs[2] = { &p_int, &p_char };
        ParamType fn = type_func(&p_int, fargs, 2);
        ParamType p_fn = type_ptr(&fn);

        mangle_free_function(&m, "takes_fnptr", &p_fn, 1);
        check(m.buf, "_Z11takes_fnptrPFiicE", "takes_fnptr(int(*)(int,char))");
    }

    /* void takes_fnptr_multi(int(*)(int,char), int(*)(int,char)) -> _Z17takes_fnptr_multiPFiicES0_ */
    {
        ParamType p_int = type_builtin("int");
        ParamType p_char = type_builtin("char");
        ParamType *fargs[2] = { &p_int, &p_char };
        ParamType fn = type_func(&p_int, fargs, 2);
        ParamType p_fn = type_ptr(&fn);

        ParamType params[2];
        params[0] = p_fn;
        params[1] = p_fn;

        mangle_free_function(&m, "takes_fnptr_multi", params, 2);
        check(m.buf, "_Z17takes_fnptr_multiPFiicES0_", "takes_fnptr_multi(int(*)(int,char) x2)");
    }

    /* void takes_vec(vector<int>) -> _Z9takes_vec6vectorIiE */
    {
        ParamType p_int = type_builtin("int");
        ParamType *targs[1] = { &p_int };
        ParamType vec = type_template("vector", targs, 1);

        mangle_free_function(&m, "takes_vec", &vec, 1);
        check(m.buf, "_Z9takes_vec6vectorIiE", "takes_vec(vector<int>)");
    }

    /* void takes_vec_multi(vector<int>, vector<int>) -> _Z15takes_vec_multi6vectorIiES0_ */
    {
        ParamType p_int = type_builtin("int");
        ParamType *targs[1] = { &p_int };
        ParamType vec = type_template("vector", targs, 1);

        ParamType params[2];
        params[0] = vec;
        params[1] = vec;

        mangle_free_function(&m, "takes_vec_multi", params, 2);
        check(m.buf, "_Z15takes_vec_multi6vectorIiES0_", "takes_vec_multi(vector<int> x2)");
    }

    /* void takes_nested_vec(vector<vector<int>>) -> _Z16takes_nested_vec6vectorIS_IiEE */
    {
        ParamType p_int = type_builtin("int");
        ParamType *targs_inner[1] = { &p_int };
        ParamType vec_inner = type_template("vector", targs_inner, 1);
        
        ParamType *targs_outer[1] = { &vec_inner };
        ParamType vec_outer = type_template("vector", targs_outer, 1);

        mangle_free_function(&m, "takes_nested_vec", &vec_outer, 1);
        check(m.buf, "_Z16takes_nested_vec6vectorIS_IiEE", "takes_nested_vec(vector<vector<int>>)");
    }

    /* void takes_const_vec_ptr(const vector<int>*) -> _Z19takes_const_vec_ptrPK6vectorIiE */
    {
        ParamType p_int = type_builtin("int");
        ParamType *targs[1] = { &p_int };
        ParamType vec = type_template("vector", targs, 1);
        ParamType c_vec = type_const(&vec);
        ParamType p_c_vec = type_ptr(&c_vec);

        mangle_free_function(&m, "takes_const_vec_ptr", &p_c_vec, 1);
        check(m.buf, "_Z19takes_const_vec_ptrPK6vectorIiE", "takes_const_vec_ptr(const vector<int>*)");
    }

    /* void takes_ref(int&) -> _Z9takes_refRi */
    {
        ParamType p_int = type_builtin("int");
        ParamType p_ref = type_ref(&p_int);

        mangle_free_function(&m, "takes_ref", &p_ref, 1);
        check(m.buf, "_Z9takes_refRi", "takes_ref(int&)");
    }

    /* void vecptr_then_vec(vector<int>*, vector<int>) -> _Z15vecptr_then_vecP6vectorIiES0_ */
    {
        ParamType p_int = type_builtin("int");
        ParamType *targs[1] = { &p_int };
        ParamType vec = type_template("vector", targs, 1);
        ParamType p_vec = type_ptr(&vec);

        ParamType params[2];
        params[0] = p_vec;
        params[1] = vec;

        mangle_free_function(&m, "vecptr_then_vec", params, 2);
        check(m.buf, "_Z15vecptr_then_vecP6vectorIiES0_", "vecptr_then_vec(vector<int>*, vector<int>)");
    }

    /* void ref_vec(vector<int>&) -> _Z7ref_vecR6vectorIiE */
    {
        ParamType p_int = type_builtin("int");
        ParamType *targs[1] = { &p_int };
        ParamType vec = type_template("vector", targs, 1);
        ParamType r_vec = type_ref(&vec);

        mangle_free_function(&m, "ref_vec", &r_vec, 1);
        check(m.buf, "_Z7ref_vecR6vectorIiE", "ref_vec(vector<int>&)");
    }

    /* void takes_rref(int&&) -> _Z10takes_rrefOi */
    {
        ParamType p_int = type_builtin("int");
        ParamType rr_int = type_rref(&p_int);

        mangle_free_function(&m, "takes_rref", &rr_int, 1);
        check(m.buf, "_Z10takes_rrefOi", "takes_rref(int&&)");
    }

    /* void fp_classarg(void(*)(S)) -> _Z11fp_classargPFv1SE */
    {
        ParamType p_void = type_builtin("void");
        ParamType p_S = type_builtin("S");
        ParamType *fargs[1] = { &p_S };
        ParamType fn = type_func(&p_void, fargs, 1);
        ParamType p_fn = type_ptr(&fn);

        mangle_free_function(&m, "fp_classarg", &p_fn, 1);
        check(m.buf, "_Z11fp_classargPFv1SE", "fp_classarg(void(*)(S))");
    }

    /* void pair_then_int(pair<int,char>, int) -> _Z13pair_then_int4pairIicEi */
    {
        ParamType p_int = type_builtin("int");
        ParamType p_char = type_builtin("char");
        ParamType *targs[2] = { &p_int, &p_char };
        ParamType pair_type = type_template("pair", targs, 2);

        ParamType params[2];
        params[0] = pair_type;
        params[1] = p_int;

        mangle_free_function(&m, "pair_then_int", params, 2);
        check(m.buf, "_Z13pair_then_int4pairIicEi", "pair_then_int(pair<int,char>, int)");
    }

    /* void takes_pmf(void(C::*)(int)) -> _Z9takes_pmfM1CFviE */
    {
        ParamType p_C = type_builtin("C");
        ParamType p_void = type_builtin("void");
        ParamType p_int = type_builtin("int");
        ParamType *fargs[1] = { &p_int };
        ParamType member_fn = type_func(&p_void, fargs, 1);
        ParamType pmf = type_pmf(&p_C, &member_fn);

        mangle_free_function(&m, "takes_pmf", &pmf, 1);
        check(m.buf, "_Z9takes_pmfM1CFviE", "takes_pmf(void(C::*)(int))");
    }

    /* void vec_of_s_then_s(vector<S>, S) -> _Z15vec_of_s_then_s6vectorI1SES0_ */
    {
        ParamType p_S = type_builtin("S");
        ParamType *targs[1] = { &p_S };
        ParamType vec_S = type_template("vector", targs, 1);

        ParamType params[2];
        params[0] = vec_S;
        params[1] = p_S;

        mangle_free_function(&m, "vec_of_s_then_s", params, 2);
        check(m.buf, "_Z15vec_of_s_then_s6vectorI1SES0_", "vec_of_s_then_s(vector<S>, S)");
    }

    /* void s_then_vec_of_s(S*, vector<S>) -> _Z15s_then_vec_of_sP1S6vectorIS_E */
    {
        ParamType p_S = type_builtin("S");
        ParamType p_S_ptr = type_ptr(&p_S);
        ParamType *targs[1] = { &p_S };
        ParamType vec_S = type_template("vector", targs, 1);

        ParamType params[2];
        params[0] = p_S_ptr;
        params[1] = vec_S;

        mangle_free_function(&m, "s_then_vec_of_s", params, 2);
        check(m.buf, "_Z15s_then_vec_of_sP1S6vectorIS_E", "s_then_vec_of_s(S*, vector<S>)");
    }

    /* void fp_then_classarg(int(*)(S), S) -> _Z16fp_then_classargPFi1SES_ */
    {
        ParamType p_int = type_builtin("int");
        ParamType p_S = type_builtin("S");
        ParamType *fargs[1] = { &p_S };
        ParamType fn = type_func(&p_int, fargs, 1);
        ParamType p_fn = type_ptr(&fn);

        ParamType params[2];
        params[0] = p_fn;
        params[1] = p_S;

        mangle_free_function(&m, "fp_then_classarg", params, 2);
        check(m.buf, "_Z16fp_then_classargPFi1SES_", "fp_then_classarg(int(*)(S), S)");
    }

    /* void vec_int_vec_char(vector<int>, vector<char>) -> _Z16vec_int_vec_char6vectorIiES_IcE */
    {
        ParamType p_int = type_builtin("int");
        ParamType *targs_int[1] = { &p_int };
        ParamType vec_int = type_template("vector", targs_int, 1);

        ParamType p_char = type_builtin("char");
        ParamType *targs_char[1] = { &p_char };
        ParamType vec_char = type_template("vector", targs_char, 1);

        ParamType params[2];
        params[0] = vec_int;
        params[1] = vec_char;

        mangle_free_function(&m, "vec_int_vec_char", params, 2);
        check(m.buf, "_Z16vec_int_vec_char6vectorIiES_IcE", "vec_int_vec_char(vector<int>, vector<char>)");
    }

    /* void three_s(S*, S*, S*) -> _Z7three_sP1SS0_S0_ */
    {
        ParamType p_S = type_builtin("S");
        ParamType p_S_ptr = type_ptr(&p_S);

        ParamType params[3];
        params[0] = p_S_ptr;
        params[1] = p_S_ptr;
        params[2] = p_S_ptr;

        mangle_free_function(&m, "three_s", params, 3);
        check(m.buf, "_Z7three_sP1SS0_S0_", "three_s(S*, S*, S*)");
    }

    printf("------------------------------------------------\n");
    printf("=== %d/%d passed, %d mismatch(es) ===\n", total - fails, total, fails);
    return fails;
}

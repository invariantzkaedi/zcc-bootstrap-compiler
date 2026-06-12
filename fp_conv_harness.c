/* Proves ZCC's unconditional signed FP conversion miscompiles unsigned values.
 * Oracle = the CPU. We emit BOTH sequences as inline asm and compare to the
 * C-correct answer that gcc produces. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* what ZCC emits for (double)u : cvtsi2sdq (SIGNED) */
static double zcc_u64_to_double(uint64_t u){
    double r;
    __asm__("cvtsi2sdq %1, %0" : "=x"(r) : "r"(u));   /* signed conversion */
    return r;
}
/* what's CORRECT for (double)u : unsigned conversion (gcc picks this) */
static double correct_u64_to_double(uint64_t u){
    return (double)u;   /* let gcc emit the right thing */
}

/* what ZCC emits for (unsigned)d : cvttsd2si (SIGNED truncation) */
static uint64_t zcc_double_to_u64(double d){
    uint64_t r;
    __asm__("cvttsd2si %1, %0" : "=r"(r) : "x"(d));   /* signed truncation */
    return r;
}
static uint64_t correct_double_to_u64(double d){
    return (uint64_t)d;
}

int main(void){
    int fails = 0;

    printf("=== (double)unsigned : signed cvtsi2sd vs correct ===\n");
    uint64_t cases1[] = { 0, 100, 0x7FFFFFFFFFFFFFFFULL,
                          0x8000000000000000ULL,            /* MSB set */
                          0xFFFFFFFFFFFFFFFFULL };           /* UINT64_MAX */
    for (size_t i=0;i<sizeof(cases1)/sizeof(*cases1);i++){
        double z = zcc_u64_to_double(cases1[i]);
        double c = correct_u64_to_double(cases1[i]);
        int ok = (z==c);
        if(!ok) fails++;
        printf("  u=%-20llu  zcc=%-24g correct=%-24g %s\n",
               (unsigned long long)cases1[i], z, c, ok?"OK":"** MISCOMPILE **");
    }

    printf("\n=== (unsigned)double : signed cvttsd2si vs correct ===\n");
    double cases2[] = { 0.0, 100.5, 2147483647.0,
                        9223372036854775808.0,    /* 2^63, doesn't fit signed */
                        18446744073709551615.0 };  /* near UINT64_MAX */
    for (size_t i=0;i<sizeof(cases2)/sizeof(*cases2);i++){
        uint64_t z = zcc_double_to_u64(cases2[i]);
        uint64_t c = correct_double_to_u64(cases2[i]);
        int ok = (z==c);
        if(!ok) fails++;
        printf("  d=%-22g  zcc=%-22llu correct=%-22llu %s\n",
               cases2[i], (unsigned long long)z, (unsigned long long)c,
               ok?"OK":"** MISCOMPILE **");
    }

    printf("\n=== %d miscompile(s) demonstrated on real hardware ===\n", fails);
    return fails;
}

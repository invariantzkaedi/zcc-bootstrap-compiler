/* tests/abi/width_promotion.c
 * HYGIENE-ABI-002 probe: integer-width promotions across call boundaries.
 * Callee declares narrow parameter types; caller passes wider or narrower values.
 * Each function is noinline to force real ABI crossing.
 */
#include <stdio.h>

__attribute__((noinline))
long take_char(char c) { return (long)c; }

__attribute__((noinline))
long take_short(short s) { return (long)s; }

__attribute__((noinline))
long take_int(int i) { return (long)i; }

__attribute__((noinline))
unsigned long take_uchar(unsigned char c) { return (unsigned long)c; }

__attribute__((noinline))
unsigned long take_ushort(unsigned short s) { return (unsigned long)s; }

__attribute__((noinline))
unsigned long take_uint(unsigned int i) { return (unsigned long)i; }

/* Callee expects long, caller passes int literal */
__attribute__((noinline))
long take_long(long l) { return l; }

int main(void) {
    /* Signed narrowing: pass int-width values to narrow params */
    printf("char(-1):   %ld\n", take_char(-1));
    printf("char(127):  %ld\n", take_char(127));
    printf("char(-128): %ld\n", take_char(-128));
    printf("short(-1):  %ld\n", take_short(-1));
    printf("short(32767): %ld\n", take_short(32767));
    printf("int(-1):    %ld\n", take_int(-1));

    /* Unsigned narrowing */
    printf("uchar(255): %lu\n", take_uchar(255));
    printf("uchar(0):   %lu\n", take_uchar(0));
    printf("ushort(65535): %lu\n", take_ushort(65535));
    printf("uint(0xFFFFFFFF): %lu\n", take_uint(0xFFFFFFFFU));

    /* Widening: pass int literal to long param */
    printf("long(42):   %ld\n", take_long(42));
    printf("long(-1):   %ld\n", take_long(-1));

    return 0;
}

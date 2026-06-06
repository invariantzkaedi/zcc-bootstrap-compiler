/*
 * mem_layout.c — D-30: Memory Layout Workload
 *
 * Exercises packed struct alignments, normal structures, and stack VLAs.
 */
#include <stdio.h>
#include <stdlib.h>

#pragma pack(push, 1)
struct PackedData {
    char a;
    int b;
    char c;
    long d;
};
#pragma pack(pop)

struct NormalData {
    char a;
    int b;
    char c;
    long d;
};

int run_memory_layout(int size) {
    struct PackedData p;
    struct NormalData n;
    p.a = 1; p.b = 2; p.c = 3; p.d = 4;
    n.a = 5; n.b = 6; n.c = 7; n.d = 8;
    
    /* Variable Length Array on stack */
    volatile char vla[size];
    for (int i = 0; i < size; i++) {
        vla[i] = (char)(i * 3 + 1);
    }
    
    int val = (int)(p.a + p.b + p.c + p.d + n.a + n.b + n.c + n.d + vla[size / 2]);
    return val;
}

int main(int argc, char **argv) {
    int size = 64;
    if (argc > 1) {
        size = atoi(argv[1]);
    }
    if (size <= 0 || size > 4096) size = 64;
    
    int result = run_memory_layout(size);
    printf("result=%d\n", result);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>

int backtrace(void **buffer, int size);
char **backtrace_symbols(void *const *buffer, int size);

static int frame_count = 0;

void trace_inner(int depth) {
    void *buffer[16];
    int n = backtrace(buffer, 16);
    frame_count = n;
    printf("PROBE CFI: backtrace captured %d frames at depth %d\n", n, depth);
}

void trace_mid(int depth) {
    trace_inner(depth + 1);
}

void trace_outer(int depth) {
    trace_mid(depth + 1);
}

int main(void) {
    trace_outer(1);
    if (frame_count >= 4) {
        printf("PROBE CFI RESULT: PASS (stack unwind depth %d >= 4)\n", frame_count);
        return 0;
    } else {
        printf("PROBE CFI RESULT: FAIL (stack unwind depth %d < 4)\n", frame_count);
        return 1;
    }
}

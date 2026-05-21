#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

int main() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Pre-allocate 60MB raw memory block to bypass OS paging overhead during loop
    int cap = 1024 * 1024 * 60; 
    char* out = (char*)malloc(cap);
    int len = 0;
    
    const char* header = "<svg width='1000' height='1000'>\n";
    strcpy(out, header);
    len += strlen(header);
    
    char buf[128];
    // 1 Million iterations of mathematical string generation
    for(int i = 0; i < 1000000; i++) {
        int slen = sprintf(buf, "<circle cx=\"%d\" cy=\"%d\" r=\"5\" fill=\"cyan\"/>\n", (i * 13) % 1000, (i * 7) % 1000);
        memcpy(out + len, buf, slen); // Direct pointer math, zero V8 garbage collection
        len += slen;
    }
    
    strcpy(out + len, "</svg>");
    len += 6;

    clock_gettime(CLOCK_MONOTONIC, &end);
    double time_taken = (end.tv_sec - start.tv_sec) * 1e3 + (end.tv_nsec - start.tv_nsec) / 1e6;
    
    printf("C Generated %d bytes.\n", len);
    printf("Raw Metal Execution Time: %.2f ms\n", time_taken);
    
    free(out);
    return 0;
}

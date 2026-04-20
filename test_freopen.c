#include <stdio.h>
int main() {
    FILE *f1 = fopen("test1.txt", "w");
    fprintf(f1, "Hello 1\n");
    fclose(f1);

    freopen("/dev/null", "w", stderr);

    FILE *f2 = fopen("test2.txt", "w");
    if (!f2) { printf("F2 failed\n"); return 1; }
    fprintf(f2, "Hello 2\n");
    fclose(f2);
    printf("Done!\n");
    return 0;
}

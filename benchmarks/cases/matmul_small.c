int printf(const char *fmt, ...);
#define N 32
int A[1024];
int B[1024];
int C[1024];
int main() {
    int i, j, k, step;
    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            A[i * N + j] = i + j;
            B[i * N + j] = i - j;
            C[i * N + j] = 0;
        }
    }
    for (step = 0; step < 1000; step++) {
        for (i = 0; i < N; i++) {
            for (j = 0; j < N; j++) {
                int sum = 0;
                for (k = 0; k < N; k++) {
                    sum = sum + A[i * N + k] * B[k * N + j];
                }
                C[i * N + j] = sum;
            }
        }
    }
    printf("C[10][10] = %d\n", C[10 * N + 10]);
    return 0;
}

/* ZCC Dream Benchmark Workload — canonical fitness oracle */
int printf(const char *fmt, ...);

long fibonacci(int n) {
    long a = 0, b = 1, t; int i;
    if (n <= 1) return n;
    for (i = 2; i <= n; i++) { t = a + b; a = b; b = t; }
    return b;
}

int bubble_sort(int *arr, int n) {
    int i, j, tmp, sw = 0;
    for (i = 0; i < n-1; i++)
        for (j = 0; j < n-i-1; j++)
            if (arr[j] > arr[j+1]) { tmp=arr[j]; arr[j]=arr[j+1]; arr[j+1]=tmp; sw++; }
    return sw;
}

long hash_str(const char *s) {
    long h = 5381;
    while (*s) { h = ((h << 5) + h) + *s; s++; }
    return h;
}

int matrix_mul(int n) {
    int A[8][8], B[8][8], C[8][8], i, j, k, sum = 0;
    for (i=0; i<n&&i<8; i++) for (j=0; j<n&&j<8; j++) { A[i][j]=i*n+j; B[i][j]=j*n+i; }
    for (i=0; i<n&&i<8; i++) for (j=0; j<n&&j<8; j++) {
        C[i][j]=0;
        for (k=0; k<n&&k<8; k++) C[i][j]+=A[i][k]*B[k][j];
        sum+=C[i][j];
    }
    return sum;
}

int main(void) {
    int arr[16]; int i;
    for (i=0; i<16; i++) arr[i]=16-i;
    long f = fibonacci(40);
    int sw = bubble_sort(arr, 16);
    long h = hash_str("ZCC Oneirogenesis v2");
    int m = matrix_mul(8);
    printf("DREAM_BENCH: fib=%ld sw=%d hash=%ld mat=%d\n", f, sw, h, m);
    return 0;
}

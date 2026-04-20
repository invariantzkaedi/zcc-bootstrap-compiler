unsigned long long ull_max = 18446744073709551615ULL;
long long ll_min = (-9223372036854775807LL-1LL);
long long ll_max = 9223372036854775807LL;
int test(void) { return (int)(ull_max + ll_max + ll_min); }

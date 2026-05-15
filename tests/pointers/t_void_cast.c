/* Regression: void* cast roundtrip must preserve the address.
 * int -> void* -> int* must read back the original value. */
int main() {
    int x = 42;
    void *vp = &x;
    int *ip = (int *)vp;
    return *ip;  /* 42 */
}

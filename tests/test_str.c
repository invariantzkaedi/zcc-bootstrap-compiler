long long W1[3];
int main() {
    char* src = "\x0A\x00\x00\x00\x00\x00\x00\x00" "\x14\x00\x00\x00\x00\x00\x00\x00" "\x1E\x00\x00\x00\x00\x00\x00\x00";
    long long* w_ptr = (long long*)src;
    int i;
    for (i = 0; i < 3; i++) {
        W1[i] = w_ptr[i];
    }
    if (W1[2] == 30 && W1[0] == 10) return 0;
    return 1;
}

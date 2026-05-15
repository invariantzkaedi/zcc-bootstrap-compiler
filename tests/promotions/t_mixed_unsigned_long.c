int main() {
    signed char a = -56;
    unsigned long b = 754;
    // a promotes to int, then to unsigned long. -56 becomes a huge unsigned long.
    // However, when we do bitwise AND with 754 (0x2F2), we get 704.
    unsigned long res = a & b;
    if (res == 704) {
        return 0; // Success
    }
    return 1;
}

int main() {
    signed char a = -5;
    unsigned int b = 10;
    // a promotes to int (-5), then int and unsigned int means both promote to unsigned int.
    // -5 as unsigned int is huge. So it should be greater than 10.
    if (a > b) {
        return 1;
    }
    return 0;
}

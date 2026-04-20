int printf();
int new_label() {
    static int label_count = 0;
    return ++label_count;
}
int main() {
    printf("1: %d\n", new_label());
    printf("2: %d\n", new_label());
    return 0;
}

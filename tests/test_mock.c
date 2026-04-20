void foo() {
    int has_tag;
    char tag[128];
    has_tag = 0;
    tag[0] = 0;
    while (has_tag == 0) {
        char fname[128];
        fname[0] = 'x';
        has_tag = 1;
    }
}

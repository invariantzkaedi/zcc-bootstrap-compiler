unsigned int pal[256];
unsigned int test(unsigned char *src) {
    return pal[src[0]];
}

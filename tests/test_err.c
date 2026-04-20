int stbi__err(char*, char*);
void f() {
    return ((unsigned char *)(size_t) (stbi__err("outofmem", "Out of memory")?NULL:NULL));
}

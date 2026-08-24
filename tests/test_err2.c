int stbi__err(const char *str, const char *msg);
void *f() {
    return ((unsigned char *)(size_t) (stbi__err("outofmem", "Out of memory")?0:0));
}

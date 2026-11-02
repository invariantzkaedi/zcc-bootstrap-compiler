struct BF {
    unsigned int a : 3;
    int b : 5;
};
int main(void) {
    static struct BF bf = { 7U, -1 };
    int v_pre = ++bf.a;
    int v_post = bf.b--;
    if (v_pre != 0 || bf.a != 0U || v_post != -1 || bf.b != -2) return 1;
    return 0;
}

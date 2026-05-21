struct Point {
    int x;
    int y;
};

int test_amf(struct Point *p) {
    p->y = 42;
    return p->y;
}

int main() {
    struct Point p = {10, 20};
    test_amf(&p);
    return p.y;
}

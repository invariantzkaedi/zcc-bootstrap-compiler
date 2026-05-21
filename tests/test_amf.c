struct Point {
    int x;
    int y;
};

int test_amf(struct Point *p) {
    return p->y;
}

int main() {
    struct Point p = {10, 20};
    return test_amf(&p);
}

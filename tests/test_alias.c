struct Point {
    int x;
    int y;
};

int test_alias(struct Point *external_p) {
    struct Point local_p;
    local_p.x = 10;
    local_p.y = 20;

    external_p->x = 30;
    external_p->y = 40;

    return local_p.x + external_p->x;
}

int test_local_array(int index) {
    int arr[5];
    arr[index] = 100;
    return arr[index];
}

int main() {
    struct Point p = {0, 0};
    return test_alias(&p) + test_local_array(2);
}

int test_dom(int cond, int x) {
    int y;
    if (cond) {
        y = x + 10;
    } else {
        y = x + 20;
    }
    return y;
}

int main() {
    return test_dom(1, 5);
}

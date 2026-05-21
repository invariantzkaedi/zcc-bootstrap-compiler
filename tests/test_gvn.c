int gvn_test(int x, int y) {
    int a;
    if (x > 0) {
        a = x + y;   /* computed in BB_then */
    } else {
        a = x + y;   /* same expr in BB_else — should NOT be eliminated cross-branch */
    }
    return a + (x + y);  /* this x+y at join SHOULD be eliminated (dominated by entry) */
}

int main() {
    return gvn_test(5, 10);
}

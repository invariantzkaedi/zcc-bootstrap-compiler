int loop_sum(int n) {
    int i, s = 0;
    for (i = 0; i < n; i++) s += i;
    return s;
}
int main() { return loop_sum(10) - 45; }

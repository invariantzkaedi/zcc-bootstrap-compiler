int main() {
    int a = 0, b = 1, c = 2;
    if (a && (b = 3)) c = 4;
    if (b != 1) return 1; // b should not be evaluated
    
    if (a || (b = 4)) c = 5;
    if (b != 4) return 1; // b should be evaluated
    
    return c == 5 ? 0 : 1;
}

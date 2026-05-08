int printf(const char *fmt, ...);

int main() {
    double a = 3.14;
    double b = 2.71;
    double c = a + b;
    double d = a * b;
    
    // Convert to int to print since printf float might be complex
    int ic = (int)c;
    int id = (int)d;
    printf("c = %d, d = %d\n", ic, id);
    return 0;
}

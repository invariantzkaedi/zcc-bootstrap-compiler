int g;
int inc() { g = g + 1; return g; }
int main() {
    g = 40;
    inc();
    inc();
    return g;
}

int classify(int x) {
    switch (x) {
    case 1: return 10;
    case 2: return 20;
    case 3: return 30;
    default: return 0;
    }
}
int main() { return classify(2); }

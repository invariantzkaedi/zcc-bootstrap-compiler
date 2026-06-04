int main() {
    int i; int j; int count;
    count = 0;
    for (i = 0; i < 5; i = i + 1) {
        for (j = 0; j < 3; j = j + 1) {
            count = count + 1;
        }
    }
    return count;
}

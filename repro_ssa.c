int main() {
    int result = 0;
    {
        int a = 9;
        int b = 11;
        if (a > 5) {
            a = a + 2;
        } else {
            a = b - 1;
        }
        result = result + (a & 0xFF);
    }
    {
        int x = 1;
        int y = 2;
        if (x < y) {
            if (y > 0) {
                x = 10;
            } else {
                x = 20;
            }
        } else {
            y = 5;
        }
        result = result + ((x + y) & 0xFF);
    }
    {
        int a = 5;
        int b = 20;
        if (a > 5) {
            a = a + 2;
        } else {
            a = b - 1;
        }
        result = result + (a & 0xFF);
    }
    {
        int sum = 0;
        for (int i = 0; i < 7; i = i + 1) {
            sum = sum + i;
        }
        result = result + (sum & 0xFF);
    }
    return result & 255;
}

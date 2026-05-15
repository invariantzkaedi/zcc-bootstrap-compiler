int main() {
    int result = 0;
    {
        int x = 8;
        int *p = &x;
        int y = *p;
        *p = 14;
        result = result + ((x + y) & 0xFF);
    }
    {
        int x = 2;
        int y = 11;
        int *p = &x;
        int *q = &x;
        *p = 30;
        *q = 34;
        result = result + (x & 0xFF);
    }
    {
        volatile int x;
        x = 9;
        x = 17;
        result = result + (x & 0xFF);
    }
    {
        volatile int x;
        x = 1;
        x = 20;
        result = result + (x & 0xFF);
    }
    {
        volatile int x;
        x = 3;
        x = 11;
        result = result + (x & 0xFF);
    }
    return result & 255;
}

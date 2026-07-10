struct tm { int sec; };
typedef struct tm tm_t;

int main() {
    struct tm s;
    struct tm *tm = &s;
    tm = &s;
    int x = ((tm)->sec);
    
    // Typedefed struct tag name resolves as type
    tm_t y;
    y.sec = 0;
    
    return x + y.sec;
}

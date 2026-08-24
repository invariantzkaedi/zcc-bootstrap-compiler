unsigned char uc = 255u;
signed char sc = -128;
unsigned short us = 65535u;
short ss = -32768;
unsigned int ui = 4294967295u;
int si = -2147483648;
unsigned long ul = 18446744073709551615ul;
long sl = -9223372036854775807L - 1;
unsigned long long ull = 12345678901234567890ULL;
long long sll = -1234567890123456789LL;

float f1 = 3.14159f;
float f2 = 1.0e-5f;
float f3 = 2.5E+3f;
double d1 = 2.718281828459045;
double d2 = 1.0e-100;
double d3 = 1.0e+100;

char str1[] = "escape\n\t\r\v\f\a\b\\\"\'\0\x41\101";
char char1 = 'A';
char char_esc = '\n';
char char_hex = '\x7f';
char char_oct = '\077';

int test_literals() {
    int octal = 0755;
    int hex = 0xDEADBEEF;
    int binary = 0b101010;
    return (int)uc + (int)sc + (int)us + ss + ui + si + (int)f1 + (int)d1 + octal + hex + binary;
}
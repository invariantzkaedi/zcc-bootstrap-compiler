#include <stdio.h>
int main(void) { volatile unsigned long long i = 0; while(i < 1000000000000ULL) { i++; } return 0; }
/* test_generic_negative.c — Negative compilation cases for ZCC _Generic */

#ifdef TEST_DUPLICATE_TYPE
int x = _Generic(1, int: 10, int: 20);
#endif

#ifdef TEST_DUPLICATE_DEFAULT
int y = _Generic(1, default: 10, default: 20);
#endif

#ifdef TEST_NO_MATCH
float f = 1.0f;
int z = _Generic(f, int: 10, double: 20);
#endif

int main(void) {
    return 0;
}

/* tests/typedef_compatible.c */
typedef unsigned int size_t;
typedef unsigned int size_t;
int main(void) { return sizeof(size_t) == 4 ? 0 : 1; }


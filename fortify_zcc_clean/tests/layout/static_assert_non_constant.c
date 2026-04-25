int runtime_value(void);

_Static_assert(runtime_value(), "must reject non-constant expression");

int main(void) {
    return 0;
}
#include <stdio.h>
#include <stdarg.h>

struct FStruct {
    float a;
    double b;
    float c;
};

float add_floats(float x, float y) {
    return x + y;
}

double add_doubles(double x, double y) {
    return x + y;
}

double mixed_args(float x, double y, float z) {
    return x + y + z;
}

void print_varargs(int count, ...) {
    va_list ap;
    va_start(ap, count);
    for (int i = 0; i < count; i++) {
        /* Varargs automatically promotes float to double */
        double val = va_arg(ap, double);
        printf("vararg[%d]: %f\n", i, val);
    }
    va_end(ap);
}

struct FStruct pass_struct_by_value(struct FStruct s) {
    printf("inside_struct: a=%f, b=%f, c=%f\n", s.a, s.b, s.c);
    s.a += 1.0f;
    s.b += 2.0;
    s.c += 3.0f;
    return s;
}

int main(void) {
    float f1 = 1.5f;
    float f2 = 2.5f;
    double d1 = 10.0;
    double d2 = 20.0;

    printf("add_floats: %f\n", add_floats(f1, f2));
    printf("add_doubles: %f\n", add_doubles(d1, d2));
    printf("mixed_args: %f\n", mixed_args(f1, d1, f2));

    /* Float parameters promoted to double */
    print_varargs(3, (double)f1, 3.5, (double)f2);

    struct FStruct s1 = { 1.1f, 2.2, 3.3f };
    struct FStruct s2 = pass_struct_by_value(s1);
    printf("returned_struct: a=%f, b=%f, c=%f\n", s2.a, s2.b, s2.c);

    return 0;
}

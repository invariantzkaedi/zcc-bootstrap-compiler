/* Positive test: C23 [[...]] attributes across valid placements */
#include <stdio.h>

[[maybe_unused]] static int g_unused = 10;
[[maybe_unused, deprecated]] static int g_multi = 20;
[[deprecated("reason string")]] static int g_dep = 30;

[[nodiscard]] int get_answer(void) {
    return 42;
}

int check_fallthrough(int v) {
    int res = 0;
    switch (v) {
        case 1:
            res += 10;
            [[fallthrough]];
        case 2:
            res += 20;
            break;
        default:
            res = -1;
            break;
    }
    return res;
}

int main(void) {
    [[maybe_unused]] int local_unused = 1;
    [[deprecated("in func")]] int local_dep = 2;
    [[maybe_unused]] [[deprecated]] int consecutive_attrs = 3;

    if (get_answer() != 42) return 1;
    if (check_fallthrough(1) != 30) return 2;
    if (check_fallthrough(2) != 20) return 3;

    return 0;
}

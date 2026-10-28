#include <stdio.h>
#include <assert.h>

static int g_side_effect = 0;

int main(void) {
    /* 1. Basic value-producing statement expression */
    int x = ({ int y = 1; y + 1; });
    assert(x == 2);

    /* 2. Void statement expression with side effects */
    ({ g_side_effect = 42; });
    assert(g_side_effect == 42);

    /* 3. Statement expression with conditional branch */
    int r = ({
        int val;
        if (x == 2) {
            val = 100;
        } else {
            val = 200;
        }
        val;
    });
    assert(r == 100);

    /* 4. Nested statement expression */
    int nested = ({
        int a = ({ int b = 3; b * 4; });
        a + 5;
    });
    assert(nested == 17);

    /* 5. Glibc assert expansion check */
    assert(nested > 0);

    printf("SUCCESS: GNU statement expressions verified bit-exact\n");
    return 0;
}

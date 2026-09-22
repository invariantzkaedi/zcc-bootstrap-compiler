/* Negative control: C23 unclosed attribute-specifier */
/* This file MUST fail compilation deterministically with exit != 0 */

[[maybe_unused] int broken;

int main(void) {
    return 0;
}

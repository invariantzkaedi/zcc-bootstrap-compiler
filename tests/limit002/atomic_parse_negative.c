/* Negative control: C11 §6.7.2.4p3 violation - identifier in type-name */
/* This file MUST fail compilation with exit != 0 */

_Atomic(int forbidden_identifier);

int main(void) {
    return 0;
}

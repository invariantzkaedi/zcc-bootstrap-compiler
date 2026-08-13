/* test_alignas_negative.c — Negative compilation cases for ZCC _Alignas */

#ifdef TEST_NEGATIVE_VAL
_Alignas(-1) int negative_alignment;
#endif

#ifdef TEST_NON_POWER_OF_TWO
_Alignas(3) int non_power_of_two;
#endif

#ifdef TEST_NON_CONSTANT
int runtime_value = 16;
_Alignas(runtime_value) int non_constant;
#endif

#ifdef TEST_WEAKENED_ALIGN
_Alignas(1) long long weakened_alignment;
#endif

int main(void) {
    return 0;
}

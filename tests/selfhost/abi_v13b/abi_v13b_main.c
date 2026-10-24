#include <stdio.h>
#include "abi_v13b_types.h"

int main(void) {
    long caller_misalign = check_stack_align();
    printf("CALLER_ALIGN_MISALIGN:%ld\n", caller_misalign);

    zcc_callee_align_test();

    long callee_saved_mask = run_callee_saved_probe();
    printf("CALLEE_SAVED_MASK:%02lx\n", (unsigned long)callee_saved_mask);

    return 0;
}

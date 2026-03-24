#include "ax_vec.h"

#include <stdio.h>
#include <assert.h>
#include <stdint.h>

int main() {
    printf("Running Vector Test...\n");

    int* nums = ax_vecNew(sizeof(int));
    assert(nums != NULL);

    if ((uintptr_t)nums % 16 != 0) {
        printf("Alignment Error: Pointer %p\n", nums);
        return 1;
    }

    for (int i = 0; i < 5; i++) {
        ax_vecPush(nums, i);
    }

    assert(ax_vecSize(nums) == 5);

    for (int i = 0; i < 5; i++) {
        assert(nums[i] == i);
    }

    printf("Vector passed!\n");
    ax_vecFree(nums);

    return 0;
}

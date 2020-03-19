#include "internal.h"

u64 next_power_of_2(u64 x) {
    if (x == 0) {
        return 2;
    }

    x--;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    x++;
    return x;
}

u64 gettime_ns(void) {
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);

    return 1000000000ULL * (u64)t.tv_sec + (u64)t.tv_nsec;
}

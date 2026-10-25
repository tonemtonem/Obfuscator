#include <stdio.h>
#include <stdint.h>
static uint32_t mix(uint32_t a, uint32_t b) {
    uint32_t r = 0;
    r += a + b;
    r += a - b;
    r += a * 2; r += b * 3; r += a * 4; r += b * 5;
    r ^= (a ^ b);
    r |= (a | b) & 0x5A5A5A5Au;
    r += (a & b);
    r -= (a ^ (b + 1));
    return r;
}
int main(void) {
    uint64_t acc = 1469598103u;
    for (uint32_t a = 0; a < 500; a++)
        for (uint32_t b = 0; b < 500; b++)
            acc = (acc * 16777619u) ^ mix(a, b);
    printf("checksum=%llu\n", (unsigned long long)acc);
    return 0;
}
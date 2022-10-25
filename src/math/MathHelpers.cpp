#include "MathHelpers.hpp"

uint32_t RoundToPowerOf2(uint32_t x)
{
    // already a power of 2
    if (IsPowerOf2(x))
        return x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x + 1;
}

uint64_t RoundToPowerOf2(uint64_t x)
{
    // already a power of 2
    if (IsPowerOf2(x))
        return x;

    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    x |= x >> 32;
    return x + 1;
}

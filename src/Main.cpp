#include <iostream>
#include <cstdint>
#include <random>

#include <math/MathHelpers.hpp>
#include <allocators/BitmapAllocator.hpp>

typedef BitmapAllocator TheAllocator;

int main()
{
    const size_t memSize = 1024 * 1024 * 32; // 32mb
    uint8_t* basePtr = new uint8_t[memSize];
    Region regions[] = 
    {
        { basePtr + 0x00000000, 0x00000500, RegionType::Reserved },
        { basePtr + 0x00000500, 0x0007FB00, RegionType::Free },
        { basePtr + 0x00080000, 0x00070000, RegionType::Reserved },
        { basePtr + 0x000F0000, 0x00010000, RegionType::Reserved },
        { basePtr + 0x00100000, memSize - 0x00100000, RegionType::Free },
    };

    TheAllocator allocator;
    if (!allocator.Initialize(4096, regions, sizeof(regions) / sizeof(regions[0])))
    {
        std::cerr << "Error initializing allocator!";
        return -1;
    }

    allocator.Dump();

    delete[] basePtr;
    return 0;
}
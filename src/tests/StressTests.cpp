#include <thirdparty/catch2/catch_amalgamated.hpp>
#include <phallocators/allocators/Allocator.hpp>
#include <Config.hpp>
#include <Utils.hpp>
#include <cstdint>
#include <random>
#include <iostream>

#define TEST_ITERATIONS 1000000
#define MAX_USED_REGIONS 5000
#define BLOCK_SIZE_1_RATIO 75

TEMPLATE_TEST_CASE("Stress test", "[stress]", ALL_ALLOCATORS)
{
    TestType allocator;
    uint8_t* basePtr = new uint8_t[MEM_SIZE];
    Region regions[] = 
    {
        { basePtr + 0x00000000, 0x00000500, RegionType::Reserved },
        { basePtr + 0x00000500, 0x0007FB00, RegionType::Free     },
        { basePtr + 0x00080000, 0x00070000, RegionType::Reserved },
        { basePtr + 0x000F0000, 0x00010000, RegionType::Reserved },
        { basePtr + 0x00100000, MEM_SIZE - 0x00100000, RegionType::Free },
    };

    REQUIRE(allocator.Initialize(BLOCK_SIZE, regions, ArraySize(regions)));
    
    Region usedRegions[MAX_USED_REGIONS];
    size_t usedRegionCount = 0;

    std::mt19937 generator(SRAND);
    std::uniform_int_distribution<int> randUniform(0, 100);
    std::geometric_distribution<size_t> randSize(0.05);

    size_t freeBlocks = (MEM_SIZE * 4 / 5) / BLOCK_SIZE;
    int failedAllocations = 0;

    for (int i = 0; i < TEST_ITERATIONS; i++)
    {
        bool alloc;

        if (usedRegionCount == 0)
            alloc = true;
        else if (usedRegionCount >= MAX_USED_REGIONS || freeBlocks == 0)
            alloc = false;
        else
            alloc = (randUniform(generator) < 50) == 0;

        if (alloc)
        {
            size_t size;

            if (randUniform(generator) < BLOCK_SIZE_1_RATIO)
                size = 1;
            else
            {
                do {
                    size = randSize(generator);
                } while (size == 0 || size > freeBlocks);
            }

            // try to allocate
            ptr_t base = allocator.Allocate(size);
            if (base != nullptr)
            {
                usedRegions[usedRegionCount].Base = base;
                usedRegions[usedRegionCount].Size = size;
                ++usedRegionCount;
                freeBlocks -= size;
            }
            else
            {
                ++failedAllocations;
                // std::cerr << "failed to allocate " << size << " blocks";
                // allocator.Dump();
            }
        }
        else
        {
            std::uniform_int_distribution<size_t> randRegion(0, usedRegionCount - 1);
            size_t reg = randRegion(generator);

            allocator.Free(usedRegions[reg].Base, usedRegions[reg].Size);
            freeBlocks -= usedRegions[reg].Size;
            --usedRegionCount;
            usedRegions[reg] = usedRegions[usedRegionCount];
        }
    }

    for (size_t i = 0; i < usedRegionCount; i++)
        allocator.Free(usedRegions[i].Base, usedRegions[i].Size);

    std::cerr << "failed allocations " << failedAllocations << std::endl;

    // ensure entire memory is free
    for (uint64_t i = 0; i < MEM_SIZE; i += BLOCK_SIZE)
        if ((i >= 0x1000 && i < 0x80000) || i >= 0x00100000)
            REQUIRE(is_one_of(allocator.GetState(basePtr + i), RegionType::Free, RegionType::Allocator));

    delete[] basePtr;
}
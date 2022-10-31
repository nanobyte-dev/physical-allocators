#include <iostream>
#include <cstdint>
#include <random>

#include <phallocators/math/MathHelpers.hpp>
#include <phallocators/allocators/BitmapAllocator.hpp>
#include <phallocators/allocators/BuddyAllocator.hpp>
#include <phallocators/allocators/LinkedListAllocator.hpp>
#include <phallocators/Debug.hpp>
#include <phallocators/Config.hpp>
#include <Config.hpp>

#define ArraySize(array) (sizeof(array) / sizeof(array[0]))

ALLOCATOR g_Allocator;
uint8_t* g_BasePtr;
size_t g_FreeSpace;

void InitializeAllocator()
{
    g_BasePtr = new uint8_t[MEM_SIZE];
    Region regions[] = 
    {
        { g_BasePtr + 0x00000000, 0x00000500, RegionType::Reserved },
        { g_BasePtr + 0x00000500, 0x0007FB00, RegionType::Free     },
        { g_BasePtr + 0x00080000, 0x00070000, RegionType::Reserved },
        { g_BasePtr + 0x000F0000, 0x00010000, RegionType::Reserved },
        { g_BasePtr + 0x00100000, MEM_SIZE - 0x00100000, RegionType::Free },
    };

    if (!g_Allocator.Initialize(BLOCK_SIZE, regions, ArraySize(regions)))
    {
        Debug::Error("Main", "Error initializing allocator!");
        exit(-1);
    }

    // compute free space
    // std::vector<Region> allocatorRegions;
    // g_Allocator.GetRegions(allocatorRegions);
    // for (auto& region : allocatorRegions)
    // {
    //     if (region.Type == RegionType::Free)
    //         g_FreeSpace += region.Size;
    // }
}

#define TEST_ITERATIONS 1000000
#define MAX_USED_REGIONS 5000
#define BLOCK_SIZE_1_RATIO 75

void TestAllocator()
{
    Region usedRegions[MAX_USED_REGIONS];
    size_t usedRegionCount = 0;

    std::mt19937 generator(SRAND);
    std::uniform_int_distribution<int> randUniform(0, 100);
    std::geometric_distribution<size_t> randSize(0.05);

    size_t freeBlocks = g_FreeSpace / BLOCK_SIZE;
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
            ptr_t base = g_Allocator.Allocate(size);
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
                // g_Allocator.Dump();
            }
        }
        else
        {
            std::uniform_int_distribution<size_t> randRegion(0, usedRegionCount - 1);
            size_t reg = randRegion(generator);

            g_Allocator.Free(usedRegions[reg].Base, usedRegions[reg].Size);
            freeBlocks -= usedRegions[reg].Size;
            --usedRegionCount;
            usedRegions[reg] = usedRegions[usedRegionCount];
        }
    }

    for (size_t i = 0; i < usedRegionCount; i++)
        g_Allocator.Free(usedRegions[i].Base, usedRegions[i].Size);


    std::cerr << "failed allocations " << failedAllocations << std::endl;
}

int main(int argc, char* argv[])
{
    srand(SRAND);
    Debug::Initialize(stderr, true);
    
    InitializeAllocator();
    
    //TestAllocator();

    g_Allocator.Dump();
    std::cout << "\n\n";

    return 0;
}

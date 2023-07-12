#include <iostream>
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



void TestAllocator()
{
 
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

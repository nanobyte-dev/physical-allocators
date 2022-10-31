#include <thirdparty/catch2/catch_amalgamated.hpp>
#include <phallocators/allocators/Allocator.hpp>
#include <Config.hpp>
#include <Utils.hpp>

TEMPLATE_TEST_CASE("Simple initialization test", "[initialization]", ALL_ALLOCATORS)
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

    REQUIRE(          allocator.GetState(basePtr + 0x00000000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000001) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000010) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000100) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000250) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000004FF) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000500) == RegionType::Reserved); // Block size is 0x1000, so this will also be reserved
    REQUIRE(          allocator.GetState(basePtr + 0x00000FFF) == RegionType::Reserved); // Block size is 0x1000, so this will also be reserved
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00001000), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00007C00), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x0000FF00), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x0007FFFF), RegionType::Free, RegionType::Allocator));
    REQUIRE(          allocator.GetState(basePtr + 0x00080000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00090000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000EFFFF) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000F0000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000F0001) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000FFFFF) == RegionType::Reserved);
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00100000), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00100001), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00200000), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + MEM_SIZE - 1), RegionType::Free, RegionType::Allocator));
    REQUIRE(          allocator.GetState(basePtr + MEM_SIZE) == RegionType::Unmapped);
    REQUIRE(          allocator.GetState(basePtr + MEM_SIZE + 0x10000) == RegionType::Unmapped);

    delete[] basePtr;
}

TEMPLATE_TEST_CASE("Overlapping regions initialization test", "[initialization]", ALL_ALLOCATORS)
{
    TestType allocator;
    uint8_t* basePtr = new uint8_t[MEM_SIZE];
    Region regions[] = 
    {
        { basePtr + 0x00100000, MEM_SIZE - 0x00100000, RegionType::Free },
        { basePtr + 0x00080000, 0x00075000, RegionType::Reserved },
        { basePtr + 0x00000000, 0x00000500, RegionType::Reserved },
        { basePtr + 0x000F0000, 0x00010000, RegionType::Reserved },
        { basePtr + 0x00000200, 0x00090000, RegionType::Free     },
    };

    REQUIRE(allocator.Initialize(BLOCK_SIZE, regions, ArraySize(regions)));

    REQUIRE(          allocator.GetState(basePtr + 0x00000000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000001) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000010) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000100) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000250) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000004FF) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00000500) == RegionType::Reserved); // block size is 0x1000, so this is also reserved
    REQUIRE(          allocator.GetState(basePtr + 0x00000FFF) == RegionType::Reserved); // block size is 0x1000, so this is also reserved
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00001000), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00007C00), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x0000FF00), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x0007FFFF), RegionType::Free, RegionType::Allocator));
    REQUIRE(          allocator.GetState(basePtr + 0x00080000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x00090000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000EFFFF) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000F0000) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000F0001) == RegionType::Reserved);
    REQUIRE(          allocator.GetState(basePtr + 0x000FFFFF) == RegionType::Reserved);
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00100000), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00100001), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + 0x00200000), RegionType::Free, RegionType::Allocator));
    REQUIRE(is_one_of(allocator.GetState(basePtr + MEM_SIZE - 1), RegionType::Free, RegionType::Allocator));
    REQUIRE(          allocator.GetState(basePtr + MEM_SIZE) == RegionType::Unmapped);
    REQUIRE(          allocator.GetState(basePtr + MEM_SIZE + 0x10000) == RegionType::Unmapped);

    delete[] basePtr;
}

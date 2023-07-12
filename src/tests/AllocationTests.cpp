#include <thirdparty/catch2/catch_amalgamated.hpp>
#include <phallocators/allocators/Allocator.hpp>
#include <Config.hpp>
#include <Utils.hpp>

inline int Increment(int i)
{
    // for i smaller than 513, this will increment by 1
    // then, it will increment faster and faster
    // do this so that the tests are a bit faster, otherwise we have to wait forever for them to finish
    return i + 1 + (i / 513) * (i / 513);
}

TEMPLATE_TEST_CASE("Simple allocation test", "[allocation]", ALL_ALLOCATORS)
{
    TestType allocator;
    uint8_t* basePtr = new uint8_t[MEM_SIZE];
    Region regions[] = 
    {
        { basePtr + 0x00000000, MEM_SIZE, RegionType::Free },
    };

    REQUIRE(allocator.Initialize(BLOCK_SIZE, regions, ArraySize(regions)));
    REQUIRE(allocator.Allocate(0) == nullptr);

    // Allocate some blocks
    for (int i = 1; i < ((MEM_SIZE / BLOCK_SIZE) * 3 / 4); i = Increment(i))
    {
        INFO(i);

        uint8_t* ptr = reinterpret_cast<uint8_t*>(allocator.Allocate(i));
        REQUIRE(ptr != nullptr);

        REQUIRE(allocator.GetState(ptr) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + 0x1) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + (BLOCK_SIZE / 2)) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + BLOCK_SIZE - 1) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + (i * BLOCK_SIZE) - 1) == RegionType::Reserved);

        allocator.Free(ptr, i);

        REQUIRE(allocator.GetState(ptr) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + 0x1) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + (BLOCK_SIZE / 2)) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + BLOCK_SIZE - 1) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + (i * BLOCK_SIZE) - 1) == RegionType::Free);

        // ensure entire memory is free
        for (uint64_t j = 0; j < MEM_SIZE; j += BLOCK_SIZE)
        {
            INFO(j);
            REQUIRE(is_one_of(allocator.GetState(basePtr + j), RegionType::Free, RegionType::Allocator));
        }
    }


    delete[] basePtr;
}


TEMPLATE_TEST_CASE("Allocation test with some initial regions", "[allocation]", ALL_ALLOCATORS)
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
    REQUIRE(allocator.Allocate(0) == nullptr);

    // Allocate some blocks
    for (int i = 1; i < ((MEM_SIZE / BLOCK_SIZE) * 3 / 4); i = Increment(i))
    {
        INFO(i);

        uint8_t* ptr = reinterpret_cast<uint8_t*>(allocator.Allocate(i));
        REQUIRE(ptr != nullptr);

        REQUIRE(allocator.GetState(ptr) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + 0x1) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + (BLOCK_SIZE / 2)) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + BLOCK_SIZE - 1) == RegionType::Reserved);
        REQUIRE(allocator.GetState(ptr + (i * BLOCK_SIZE) - 1) == RegionType::Reserved);

        allocator.Free(ptr, i);

        REQUIRE(allocator.GetState(ptr) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + 0x1) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + (BLOCK_SIZE / 2)) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + BLOCK_SIZE - 1) == RegionType::Free);
        REQUIRE(allocator.GetState(ptr + (i * BLOCK_SIZE) - 1) == RegionType::Free);
    }

    // ensure entire memory is free
    for (uint64_t i = 0; i < MEM_SIZE; i += BLOCK_SIZE)
        if ((i >= 0x1000 && i < 0x80000) || i >= 0x00100000)
        {
            INFO(i);
            REQUIRE(is_one_of(allocator.GetState(basePtr + i), RegionType::Free, RegionType::Allocator));
        }

    delete[] basePtr;
}
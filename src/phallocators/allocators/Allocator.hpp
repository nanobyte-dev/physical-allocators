#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

enum class RegionType
{
    Free,
    Reserved,
    Unmapped,
    Allocator,
};

using ptr_t = void*;

struct Region
{
    ptr_t Base;
    uint64_t Size;
    RegionType Type;
};

class Allocator
{
public:
    virtual bool Initialize(uint64_t blockSize, Region regions[], size_t regionCount) = 0;
    virtual ptr_t Allocate(uint32_t blocks = 1) = 0;
    virtual void Free(ptr_t base, uint32_t blocks) = 0;
    
    // for statistics
    virtual RegionType GetState(ptr_t address) = 0;
    virtual void Dump() = 0;
};

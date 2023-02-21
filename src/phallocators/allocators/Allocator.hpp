#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "../util/JsonWriter.hpp"
#include "../math/MathHelpers.hpp"

enum class RegionType
{
    Free,
    Reserved,
    Unmapped,
    Allocator,
};

using ptr_t = void*;

/**
 * Represents a memory region
 */
struct Region
{
    ptr_t Base;
    uint64_t Size;
    RegionType Type;
};

/**
 * Same as Region, but 'Base' and 'Size' are measured in "blocks"
 */
struct RegionBlocks
{
    uint64_t Base;
    uint64_t Size;
    RegionType Type;
};

class Allocator
{
public:
    Allocator();
    bool Initialize(uint64_t blockSize, const Region regions[], size_t regionCount);
    virtual ptr_t Allocate(uint32_t blocks = 1) = 0;
    virtual void Free(ptr_t base, uint32_t blocks) = 0;
    
    // for statistics
    virtual RegionType GetState(ptr_t address) = 0;
    void Dump(std::string filename = "");

protected:
    template<typename TPtr>
    inline uint64_t ToBlock(TPtr ptr)
    {
        uint8_t* u8Ptr = reinterpret_cast<uint8_t*>(ptr);
        return (u8Ptr - m_MemBase) / m_BlockSize;
    }

    template<typename TPtr>
    inline uint64_t ToBlockRoundUp(TPtr ptr)
    {
        uint8_t* u8Ptr = reinterpret_cast<uint8_t*>(ptr);
        return DivRoundUp(static_cast<uint64_t>(u8Ptr - m_MemBase), m_BlockSize);
    }

    inline ptr_t ToPtr(uint64_t block)
    {
        uint8_t* u8Ptr = m_MemBase + block * m_BlockSize;
        return reinterpret_cast<ptr_t>(u8Ptr);
    }

    virtual bool InitializeImpl(RegionBlocks regions[], size_t regionCount) = 0;
    virtual void DumpImpl(JsonWriter& writer) = 0;

private:
    void DetermineMemoryRange(const Region regions[], size_t regionCount);
    void FixOverlappingRegions(RegionBlocks regions[], size_t& regionCount);

protected:
    uint64_t m_BlockSize;
    uint64_t m_MemSizeBytes;
    uint64_t m_MemSize;

private:
    uint8_t* m_MemBase;
};

#include "BitmapAllocator.hpp"
#include <math/MathHelpers.hpp>
#include <memory.h>
#include <stdio.h>

BitmapAllocator::BitmapAllocator()
{
}

bool BitmapAllocator::Initialize(uint64_t blockSize, Region regions[], size_t regionCount)
{
    m_BlockSize = blockSize;

    // determine where memory begins and ends
    uint8_t* memBase = reinterpret_cast<uint8_t*>(-1);
    uint8_t* memEnd = nullptr;
    Region *biggestFreeRegion = nullptr;

    // go through list of blocks to determine 3 things: where memory begins, where it ends and biggest free region
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Base < memBase)
            memBase = reinterpret_cast<uint8_t*>(regions[i].Base);

        if (reinterpret_cast<uint8_t*>(regions[i].Base) + regions[i].Size > memEnd)
            memEnd = reinterpret_cast<uint8_t*>(regions[i].Base) + regions[i].Size;

        if (regions[i].Type == RegionType::Free && (!biggestFreeRegion || regions[i].Size > biggestFreeRegion->Size))
            biggestFreeRegion = &regions[i];
    }

    // not enough space :(
    if (biggestFreeRegion == nullptr)
        return false;

    m_MemBase = memBase;
    uint64_t memSizeBytes = memEnd - memBase;

    // determine minimum block size
    uint64_t minBlockSize = memSizeBytes / (biggestFreeRegion->Size - 1);
    if (blockSize < minBlockSize && minBlockSize < 1024 * 1024 * 128)
    {
        // keep blocks a reasonable size
        if (minBlockSize < 1024 * 1024 * 32)
        {
            minBlockSize = RoundToPowerOf2(minBlockSize);
            fprintf(stderr, "Warning: not enough free memory for bitmap with blockSize %d, changing block size to %d", blockSize, minBlockSize);
            m_BlockSize = minBlockSize;
        }
        else
        {
            // not enough free memory
            return false;
        }
    }
    
    m_MemSize = memSizeBytes / blockSize;
    m_Bitmap = reinterpret_cast<uint8_t*>(biggestFreeRegion->Base);
    m_BitmapSize = (m_MemSize + 7) / 8;                                // 1 bit per block

    // initialize bitmap with everything marked as "used"
    memset(m_Bitmap, 0xFF, m_BitmapSize);

    // process free regions first
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Type == RegionType::Free)
            MarkRegion(regions[i].Base, regions[i].Size, false);
    }
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Type != RegionType::Free)
            MarkRegion(regions[i].Base, regions[i].Size, true);
    }

    // mark region used by bitmap
    MarkRegion(m_Bitmap, m_BitmapSize, true);

    return true;
}

ptr_t BitmapAllocator::Allocate(uint32_t blocks) 
{
    size_t currentRegionSize = 0;
    uint64_t currentRegionStart = 0;
    bool currentRegionReset = true;

    for (uint64_t i = 0; i < m_MemSize; i++)
    {
        // used
        if (Get(i))
        {
            currentRegionReset = true;
        }
        else
        {
            if (currentRegionReset)
            {
                currentRegionSize = 1;
                currentRegionStart = i;
            }
            else currentRegionSize++;

            if (currentRegionSize >= blocks)
            {
                MarkBlocks(currentRegionStart, blocks, true);
                return ToPtr<ptr_t>(currentRegionStart);
            }
        }
    }

    // nothing found
    return nullptr;
}

void BitmapAllocator::Free(ptr_t base, uint32_t blocks)
{
    MarkRegion(base, m_BlockSize * blocks, false);
}

// for statistics
size_t BitmapAllocator::GetRegionCount()
{
    return m_MemSize;
}

void BitmapAllocator::GetRegion(size_t index, Region& regionOut)
{
    regionOut.Base = ToPtr<ptr_t>(index * m_BlockSize);
    regionOut.Size = m_BlockSize;
    regionOut.Type = Get(index) ? RegionType::Reserved : RegionType::Free;
}

void BitmapAllocator::MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed)
{
    uint64_t base = ToBlock(basePtr);
    size_t size = (sizeBytes + m_BlockSize - 1) / m_BlockSize;
    MarkBlocks(base, size, isUsed);
}

void BitmapAllocator::MarkBlocks(uint64_t base, size_t size, bool isUsed)
{
    // partial bytes at the beginning
    for (; base % BitmapUnit && size > 0; ++base, --size)
        Set(base, isUsed);

    // entire bytes in the middle
    if (size > 0)
    {
        memset(m_Bitmap + (base / BitmapUnit), isUsed ? 0xFF : 0, size / BitmapUnit);
        base += (size - size % BitmapUnit);
        size = size % BitmapUnit;
    }

    // partial bytes at the end
    for (; size > 0; ++base, --size)
        Set(base, isUsed);
}

void BitmapAllocator::Dump()
{
    printf("{\n");
    printf("  \"blockSize\": %u,\n", m_BlockSize);
    printf("  \"memBase\": %u,\n", m_MemBase);
    printf("  \"memSize\": %u,\n", m_MemSize);
    printf("  \"bitmapSize\": %u,\n", m_BitmapSize);
    
    printf("  \"bitmap\": \"");
    for (size_t i = 0; i < m_MemSize; i++)
        printf("%d", Get(i));
    printf("\"\n");

    printf("}\n");
}
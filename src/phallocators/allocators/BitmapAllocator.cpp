#include "BitmapAllocator.hpp"
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>
#include <Debug.hpp>
#include <Config.hpp>
#include <memory.h>
#include <iostream>
#include <sstream>

#define INVALID_BLOCK           ((uint64_t)-1)

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
            Debug::Warn("BitmapAllocator", "Warning: not enough free memory for bitmap with blockSize %d, changing block size to %d", blockSize, minBlockSize);
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
    m_BitmapSize = DivRoundUp(m_MemSize, BitmapUnit);

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
    if (blocks == 0)
        return nullptr;

    uint64_t pickedRegion = FindFreeRegion(blocks);
    if (pickedRegion == INVALID_BLOCK)
        return nullptr;

    MarkBlocks(pickedRegion, blocks, true);
    return ToPtr(pickedRegion);
}

void BitmapAllocator::Free(ptr_t base, uint32_t blocks)
{
    MarkRegion(base, m_BlockSize * blocks, false);
}

void BitmapAllocator::MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed)
{
    uint64_t base = ToBlock(basePtr);
    size_t size = DivRoundUp(sizeBytes, m_BlockSize);
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

// for statistics
RegionType BitmapAllocator::GetState(ptr_t address)
{
    if (address >= m_Bitmap && address < m_Bitmap + m_BitmapSize)
        return RegionType::Allocator;

    uint64_t base = ToBlock(address);
    if (base >= m_MemSize)
        return RegionType::Unmapped;

    return Get(base) ? RegionType::Reserved : RegionType::Free;
}

void BitmapAllocator::Dump()
{
    JsonWriter writer(std::cout, true);
    writer.BeginObject();
    writer.Property("blockSize", m_BlockSize);
    writer.Property("memBase", m_MemBase);
    writer.Property("memSize", m_MemSize);
    writer.Property("bitmapSize", m_BitmapSize);

    std::stringstream bitmap;
    for (size_t i = 0; i < m_MemSize; i++)
        bitmap << static_cast<int>(Get(i));

    writer.Property("bitmap", bitmap.str());
    writer.EndObject();
}

uint64_t BitmapAllocatorFirstFit::FindFreeRegion(uint32_t blocks)
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
                currentRegionReset = false;
            }
            else currentRegionSize++;

            if (currentRegionSize >= blocks)
                return currentRegionStart;
        }
    }

    return INVALID_BLOCK;
}


BitmapAllocatorNextFit::BitmapAllocatorNextFit()
    : BitmapAllocator(),
      m_Next(0)
{
}
    
uint64_t BitmapAllocatorNextFit::FindFreeRegion(uint32_t blocks)
{
    size_t currentRegionSize = 0;
    uint64_t currentRegionStart = 0;
    bool currentRegionReset = true;

    // Next fit only makes sense if the block previous to m_Next is used
    if (m_Next > 0 && !Get(m_Next - 1))
        m_Next = 0;

    for (uint64_t it = 0; it < m_MemSize; it++)
    {
        uint64_t i = (it + m_Next) % m_MemSize;
        if (i == 0)
            currentRegionReset = true;

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
                currentRegionReset = false;
            }
            else currentRegionSize++;

            if (currentRegionSize >= blocks) {
                m_Next = (currentRegionStart + 1) % m_MemSize;
                return currentRegionStart;
            }
        }
    }

    return INVALID_BLOCK;
}

uint64_t BitmapAllocatorBestFit::FindFreeRegion(uint32_t blocks)
{
    uint64_t currentRegionStart = 0;
    bool currentRegionType = Get(0);

    uint64_t pickedRegionStart = INVALID_BLOCK;
    bool pickedRegionSize = 0;

    for (uint64_t i = 0; i <= m_MemSize; i++)
    {
        if (i == m_MemSize || currentRegionType != Get(i))
        {
            // we have a region, see if we can use it
            size_t regionSize = i - currentRegionStart;
            if (currentRegionType == false && regionSize >= blocks)
            {
                if (pickedRegionStart == INVALID_BLOCK || pickedRegionSize > regionSize)
                {
                    pickedRegionStart = currentRegionStart;
                    pickedRegionSize = regionSize;
                }
            }

            currentRegionStart = i;
            currentRegionType = Get(i);
        }
    }

    return pickedRegionStart;
}

uint64_t BitmapAllocatorWorstFit::FindFreeRegion(uint32_t blocks)
{
    uint64_t currentRegionStart = 0;
    bool currentRegionType = Get(0);

    uint64_t pickedRegionStart = INVALID_BLOCK;
    bool pickedRegionSize = 0;

    for (uint64_t i = 0; i <= m_MemSize; i++)
    {
        if (i == m_MemSize || currentRegionType != Get(i))
        {
            // we have a region, see if we can use it
            size_t regionSize = i - currentRegionStart;
            if (currentRegionType == false && regionSize >= blocks)
            {
                if (pickedRegionStart == INVALID_BLOCK || pickedRegionSize < regionSize)
                {
                    pickedRegionStart = currentRegionStart;
                    pickedRegionSize = regionSize;
                }
            }

            currentRegionStart = i;
            currentRegionType = Get(i);
        }
    }

    return pickedRegionStart;
}
#include "BitmapAllocator.hpp"
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>
#include <Debug.hpp>
#include <Config.hpp>
#include <memory.h>
#include <iostream>
#include <sstream>

#define INVALID_BLOCK                   ((uint64_t)-1)

BitmapAllocator::BitmapAllocator()
    : Allocator()
{
}

bool BitmapAllocator::InitializeImpl(RegionBlocks regions[], size_t regionCount)
{
    m_BitmapSize = DivRoundUp(m_MemSize, 8ul);

    // Find free region to fit BitmapSize
    RegionBlocks *freeRegion = nullptr;
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Type == RegionType::Free && regions[i].Size * m_BlockSize >= m_BitmapSize)
            freeRegion = &regions[i];
    }

    // no free space :(
    if (freeRegion == nullptr)
    {
        Debug::Error("BitmapAllocator", "Not enough free memory - needed %u!", m_BitmapSize);
        return false;
    }

    m_Bitmap = reinterpret_cast<BitmapUnitType*>(ToPtr(freeRegion->Base));

    // initialize bitmap with everything marked as "used"
    memset(m_Bitmap, 0xFF, m_BitmapSize);
    
    // process free regions first
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Type == RegionType::Free)
            MarkBlocks(regions[i].Base, regions[i].Size, false);
    }
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Type != RegionType::Free)
            MarkBlocks(regions[i].Base, regions[i].Size, true);
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
    uint64_t base; 
    size_t size;
    
    if (isUsed) {
        base = ToBlock(basePtr);
        size = DivRoundUp(sizeBytes, m_BlockSize);
    }
    else {
        base = ToBlockRoundUp(basePtr);
        size = sizeBytes / m_BlockSize;
    }

    MarkBlocks(base, size, isUsed);
}

void BitmapAllocator::MarkBlocks(uint64_t base, size_t size, bool isUsed)
{
    // partial byte at the beginning
    for (; base % 8 && size > 0; ++base, --size)
        Set(base, isUsed);

    // entire bytes in the middle
    if (size >= 8)
    {
        memset(reinterpret_cast<uint8_t*>(m_Bitmap) + base / 8, isUsed ? 0xFF : 0, size / 8);
        base += (size - size % 8);
        size = size % 8;
    }

    // partial byte at the end
    for (; size > 0; ++base, --size)
        Set(base, isUsed);
}

// for statistics
RegionType BitmapAllocator::GetState(ptr_t address)
{
    if (address >= m_Bitmap && address < reinterpret_cast<uint8_t*>(m_Bitmap) + m_BitmapSize)
        return RegionType::Allocator;

    uint64_t base = ToBlock(address);
    if (base >= m_MemSize)
        return RegionType::Unmapped;

    return Get(base) ? RegionType::Reserved : RegionType::Free;
}

void BitmapAllocator::DumpImpl(JsonWriter& writer)
{
    writer.Property("bitmapSize", m_BitmapSize);

    std::stringstream bitmap;
    for (size_t i = 0; i < m_MemSize; i++)
        bitmap << static_cast<int>(Get(i));

    writer.Property("bitmap", bitmap.str());
}

uint64_t BitmapAllocatorFirstFit::FindFreeRegion(uint32_t blocks)
{
    uint64_t currentRegionStart = 0;
    size_t currentRegionSize = 0;

    for (uint64_t i = 0; i <= m_MemSize / BlocksPerUnit; i++)
    {
        // used
        if (m_Bitmap[i] == static_cast<BitmapUnitType>(-1))
        {
            currentRegionSize = 0;
            currentRegionStart = i * BlocksPerUnit + 1;
        }
        else
        {
            BitmapUnitType val = m_Bitmap[i];
            for (size_t off = 0; off < BlocksPerUnit && (i * BlocksPerUnit + off) < m_MemSize; off++, val>>=1)
            {
                // region is used
                if (val & 1)
                {
                    currentRegionSize = 0;
                    currentRegionStart = i * BlocksPerUnit + off + 1;
                }
                else
                {
                    currentRegionSize++;
                    if (currentRegionSize >= blocks)
                        return currentRegionStart;
                }
            }
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
    // Next fit only makes sense if the block previous to m_Next is used
    if (m_Next > 0 && !Get(m_Next - 1))
        m_Next = 0;

    size_t currentRegionSize = 0;
    uint64_t currentRegionStart = m_Next % m_MemSize;

    for (uint64_t it = 0; it < m_MemSize; it++)
    {
        uint64_t i = (it + m_Next) % m_MemSize;
        if (i == 0)
        {
            currentRegionSize = 0;
            currentRegionStart = 0;
        }

        // used
        if (Get(i))
        {
            currentRegionSize = 0;
            currentRegionStart = i + 1;
        }
        else
        {
            currentRegionSize++;
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

    while (currentRegionStart < m_MemSize)
    {
        // determine region size
        size_t size = 0;
        uint64_t i;

        for (i = currentRegionStart; i < m_MemSize; i++)
        {
            if (i % BlocksPerUnit == 0 && 
                (m_Bitmap[i / BlocksPerUnit] == static_cast<BitmapUnitType>(0) || 
                 m_Bitmap[i / BlocksPerUnit] == static_cast<BitmapUnitType>(-1)))
            {
                auto value = m_Bitmap[i / BlocksPerUnit];
                if ((value & 1) != currentRegionType)
                    break;

                i += BlocksPerUnit - 1;
            }

            else if (Get(i) != currentRegionType)
                break;
        }

        size = i - currentRegionStart;

        // check region type
        if (currentRegionType == false && 
            size >= blocks && 
            (pickedRegionStart == INVALID_BLOCK || pickedRegionSize > size))
        {
            pickedRegionStart = currentRegionStart;
            pickedRegionSize = size;
        }

        // start next region
        currentRegionStart = i;
        currentRegionType = Get(i);
    }

    return pickedRegionStart;
}

uint64_t BitmapAllocatorWorstFit::FindFreeRegion(uint32_t blocks)
{
    uint64_t currentRegionStart = 0;
    bool currentRegionType = Get(0);

    uint64_t pickedRegionStart = INVALID_BLOCK;
    bool pickedRegionSize = 0;

    while (currentRegionStart < m_MemSize)
    {
        // determine region size
        size_t size = 0;
        uint64_t i;

        for (i = currentRegionStart; i < m_MemSize; i++)
        {
            if (i % BlocksPerUnit == 0 && 
                (m_Bitmap[i / BlocksPerUnit] == static_cast<BitmapUnitType>(0) || 
                 m_Bitmap[i / BlocksPerUnit] == static_cast<BitmapUnitType>(-1)))
            {
                auto value = m_Bitmap[i / BlocksPerUnit];
                if ((value & 1) != currentRegionType)
                    break;

                i += BlocksPerUnit - 1;
            }

            else if (Get(i) != currentRegionType)
                break;
        }

        size = i - currentRegionStart;

        // check region type
        if (currentRegionType == false && 
            size >= blocks && 
            (pickedRegionStart == INVALID_BLOCK || pickedRegionSize < size))
        {
            pickedRegionStart = currentRegionStart;
            pickedRegionSize = size;
        }

        // start next region
        currentRegionStart = i;
        currentRegionType = Get(i);
    }

    return pickedRegionStart;
}

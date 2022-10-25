#include "BuddyAllocator.hpp"
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>
#include <Debug.hpp>
#include <Config.hpp>
#include <memory.h>
#include <iostream>
#include <sstream>

#define LAYER_COUNT 10

BuddyAllocator::BuddyAllocator()
{
}

bool BuddyAllocator::Initialize(uint64_t blockSize, Region regions[], size_t regionCount)
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
            Debug::Warn("BuddyAllocator", "Warning: not enough free memory for bitmap with blockSize %d, changing block size to %d", blockSize, minBlockSize);
            m_BlockSize = minBlockSize;
        }
        else
        {
            // not enough free memory
            return false;
        }
    }
    
    m_MemSize = memSizeBytes / blockSize;
    m_BitmapFirstLayerSize = DivRoundUp(m_MemSize, BitmapUnit);
    m_BitmapSize = IndexOfLayer(LAYER_COUNT);
    m_Bitmap = reinterpret_cast<uint8_t*>(biggestFreeRegion->Base);

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

ptr_t BuddyAllocator::Allocate(uint32_t blocks) 
{
    // figure out closest layer
    int layer = Log2(blocks) + (IsPowerOf2(blocks) ? 0 : 1);

    // number of blocks is larger than any block size we store in the bitmaps
    // so we need to search using the same tactic as in the bitmap allocator
    // but on the last layer (with the biggest blocks)
    if (layer >= LAYER_COUNT)
    {
        size_t currentRegionCount = 0;
        uint64_t currentRegionStart = 0;
        bool currentRegionReset = true;

        for (uint64_t i = 0; i < BlocksOnLayer(LAYER_COUNT - 1); i++)
        {
            // used
            if (Get(LAYER_COUNT - 1, i))
            {
                currentRegionReset = true;
            }
            else
            {
                if (currentRegionReset)
                {
                    currentRegionCount = 1;
                    currentRegionStart = i;
                    currentRegionReset = false;
                }
                else currentRegionCount++;

                if (currentRegionCount * (1 << (LAYER_COUNT - 1)) >= blocks)
                {
                    MarkBlocks(currentRegionStart, blocks, true);
                    return ToPtr(currentRegionStart);
                }
            }
        }
    }
    else
    {
        // we only need to find 1 free block on "layer"
        for (size_t i = 0; i < BlocksOnLayer(layer); i++)
            if (!Get(layer, i))
            {
                // found free block
                uint64_t base = i * (1 << layer);
                MarkBlocks(base, blocks, true);
                return ToPtr(base);
            }
    }

    // nothing found
    return nullptr;
}

void BuddyAllocator::Free(ptr_t base, uint32_t blocks)
{
    MarkRegion(base, m_BlockSize * blocks, false);
}

void BuddyAllocator::MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed)
{
    uint64_t base = ToBlock(basePtr);
    size_t size = DivRoundUp(sizeBytes, m_BlockSize);
    MarkBlocks(base, size, isUsed);
}

void BuddyAllocator::MarkBlocks(uint64_t block, size_t count, bool isUsed)
{
    size_t blockOrig = block;
    size_t countOrig = count;

    // partial bytes at the beginning
    for (; block % BitmapUnit && count > 0; ++block, --count)
        Set(0, block, isUsed);

    // entire bytes in the middle
    if (count > 0)
    {
        memset(m_Bitmap + IndexOfLayer(0) + (block / BitmapUnit), isUsed ? 0xFF : 0, count / BitmapUnit);
        block += (count - count % BitmapUnit);
        count = count % BitmapUnit;
    }

    // partial bytes at the end
    for (; count > 0; ++block, --count)
        Set(0, block, isUsed);

    BubbleUp(0, blockOrig, countOrig);
}

void BuddyAllocator::BubbleUp(int layerStart, uint64_t block, size_t count)
{
    // start counting from layer + 1, and process whole units
    block = (block / BitmapUnit) >> 1;
    count = (DivRoundUp(count, BitmapUnit) + 1) >> 1;

    for (int layer = layerStart + 1; layer < LAYER_COUNT; layer++)
    {
        uint64_t prevLayerAddr = IndexOfLayer(layer - 1) + 2 * block;
        uint64_t layerAddr = IndexOfLayer(layer) + block;
        size_t layerSize = DivRoundUp(BlocksOnLayer(layer), BitmapUnit);

        for (size_t i = 0; i < count && i < layerSize; i++)
        {
            auto prev0 = m_Bitmap[prevLayerAddr + i * 2];
            prev0 |= (prev0 >> 1);                  // combine (OR) buddies
            prev0 &= 0x55;                          // xaxbxcxd => 0a0b0c0d
            prev0 = (prev0 | (prev0 >> 1)) & 0x33;  // 0a0b0c0d => 00ab00cd
            prev0 = (prev0 | (prev0 >> 2)) & 0x0F;  // 00ab00cd => 0000abcd

            auto prev1 = m_Bitmap[prevLayerAddr + i * 2 + 1];
            prev1 |= (prev1 >> 1);                  // combine (OR) buddies
            prev1 &= 0x55;                          // xaxbxcxd => 0a0b0c0d
            prev1 = (prev1 | (prev1 >> 1)) & 0x33;  // 0a0b0c0d => 00ab00cd
            prev1 = (prev1 | (prev1 >> 2)) & 0x0F;  // 00ab00cd => 0000abcd

            m_Bitmap[layerAddr + i] = (prev1 << 4) | prev0;
        }

        block >>= 1;
        count = (count + 1) >> 1;
    }
}

// for statistics
void BuddyAllocator::GetRegions(std::vector<Region>& regions)
{
    uint64_t currentRegionStart = 0;
    bool currentRegionType = Get(0, 0);

    for (size_t i = 0; i <= m_MemSize; i++)
    {
        if (i == m_MemSize || currentRegionType != Get(0, i))
        {
            Region region;
            region.Base = ToPtr(currentRegionStart);
            region.Size = m_BlockSize * (i - currentRegionStart);
            region.Type = currentRegionType ? RegionType::Reserved : RegionType::Free;
            regions.push_back(region);

            if (i < m_MemSize) 
            {
                currentRegionStart = i;
                currentRegionType = Get(0, i);
            }
        }
    }
}

void BuddyAllocator::Dump()
{
    JsonWriter writer(std::cout, true);
    writer.BeginObject();
    writer.Property("blockSize", m_BlockSize);
    writer.Property("memBase", m_MemBase);
    writer.Property("memSize", m_MemSize);
    writer.Property("bitmapSize", m_BitmapSize);

    writer.BeginObject("bitmap");

    for (int layer = 0; layer < LAYER_COUNT; layer++)
    {
        std::stringstream bitmap;
        for (size_t i = 0; i < BlocksOnLayer(layer); i++)
            bitmap << static_cast<int>(Get(layer, i));

        writer.Property(std::to_string(layer), bitmap.str());
    }

    writer.EndObject();
    writer.EndObject();
}

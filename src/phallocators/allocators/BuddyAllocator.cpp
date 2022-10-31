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
    : m_LastAllocatedBlock(0),
      m_LastAllocatedLayer(-1)
{
}

bool BuddyAllocator::Initialize(uint64_t blockSize, Region regions[], size_t regionCount)
{
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

    // no free space :(
    if (biggestFreeRegion == nullptr)
        return false;

    m_MemBase = memBase;
    uint64_t memSizeBytes = memEnd - memBase;
    m_SmallBlockSize = blockSize;
    m_BigBlockSize = blockSize * (1 << (LAYER_COUNT - 1));
    m_BlocksLayer0 = DivRoundUp(memSizeBytes, m_BigBlockSize);
    m_BitmapSize = IndexOfLayer(LAYER_COUNT);
    m_Bitmap = reinterpret_cast<uint8_t*>(biggestFreeRegion->Base);

    // make sure we have enough space
    if (m_BitmapSize > biggestFreeRegion->Size)
    {
        Debug::Error("BuddyAllocator", "Not enough free memory - needed %u, only have %u!", m_BitmapSize, biggestFreeRegion->Size);
        return false;
    }

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

    // mark region used by bitmap as used
    MarkRegion(m_Bitmap, m_BitmapSize, true);

    return true;
}

ptr_t BuddyAllocator::Allocate(uint32_t blocks) 
{
    if (blocks == 0)
        return nullptr;

    // figure out closest layer
    int layer = LAYER_COUNT - 1 - Log2(blocks) - (IsPowerOf2(blocks) ? 0 : 1);

    // number of blocks is larger than any block size we store in the bitmaps
    // so we need to search using the same tactic as in the bitmap allocator
    // but on the last layer (with the biggest blocks)
    if (layer < 0)
    {
        size_t currentRegionCount = 0;
        uint64_t currentRegionStart = 0;
        bool currentRegionReset = true;

        for (uint64_t i = 0; i < BlocksOnLayer(0); i++)
        {
            // used
            if (Get(0, i))
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
                    m_LastAllocatedBlock = currentRegionStart;
                    m_LastAllocatedCount = currentRegionCount;
                    m_LastAllocatedLayer = 0;

                    uint64_t base = currentRegionStart * (1 << (LAYER_COUNT - 1));
                    MarkBlocks(base, blocks, true);
                    return ToPtr(base);
                }
            }
        }
    }
    else
    {
        for(int layerSplit = layer; layerSplit >= 0; layerSplit--)
        {
            auto count = BlocksOnLayer(layerSplit);
            for (size_t iSplit = 0; iSplit < count; iSplit += 2)
            {
                // a block is considered free if its buddy is used
                // otherwise we have to "split" a block on a lower layer
                if (Get(layerSplit, iSplit) != Get(layerSplit, iSplit + 1))
                {
                    // set i to point to the free block
                    if (Get(layerSplit, iSplit))
                        ++iSplit;

                    // if we are on a lower level than "layer", we need to split from "splitLayer" to "layer"
                    // always split on the left side
                    int i = iSplit;
                    for (int l = layerSplit; l < layer; l++, i <<= 1)
                        Set(l, i, true);

                    // bubble down - mark blocks below as used
                    int iBubble = i << 1;
                    int countBubble = 2;
                    for (int l = layer + 1; l < LAYER_COUNT; l++, iBubble <<= 1, countBubble <<= 1)
                    {
                        for (int j = 0; j < countBubble; j++)
                            Set(l, iBubble + j, true);
                    }

                    // i points to index of block we want to return
                    Set(layer, i, true);
                    m_LastAllocatedBlock = i;
                    m_LastAllocatedCount = 1;
                    m_LastAllocatedLayer = layer;

                    uint64_t base = i * (1 << (LAYER_COUNT - 1 - layer));
                    return ToPtr(base);
                }
            }
        }
    }

    // nothing found
    return nullptr;
}

void BuddyAllocator::Free(ptr_t base, uint32_t blocks)
{
    // figure out closest layer
    int layer = LAYER_COUNT - 1 - Log2(blocks) - (IsPowerOf2(blocks) ? 0 : 1);
    if (layer < 0) {
        MarkBlocks(ToBlock(base), blocks, false);
    }
    else {
        MarkBlocks(ToBlock(base), RoundToPowerOf2(blocks), false);
    }
}

void BuddyAllocator::MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed)
{
    uint64_t base = ToBlock(basePtr);
    size_t size = DivRoundUp(sizeBytes, m_SmallBlockSize);
    MarkBlocks(base, size, isUsed);
}

void BuddyAllocator::MarkBlocks(uint64_t block, size_t count, bool isUsed)
{
    // partial bytes at the beginning
    for (uint64_t i = block; i < block + count; i++)
        Set(LAYER_COUNT - 1, i, isUsed);

    // bubble up all the way to layer 0
    for (int layer = LAYER_COUNT - 2; layer >= 0; layer--)
    {
        block /= 2;
        count = DivRoundUp(count, 2UL);
        for (uint64_t i = block; i < block + count; i++)
        {
            bool val = Get(layer + 1, i * 2) || Get(layer + 1, i * 2 + 1);
            Set(layer, i, val);
        }
    }
}

// for statistics
RegionType BuddyAllocator::GetState(ptr_t address)
{
    if (address >= m_Bitmap && address < m_Bitmap + m_BitmapSize)
        return RegionType::Allocator;

    uint64_t base = ToBlock(address);
    if (base >= BlocksOnLayer(LAYER_COUNT - 1))
        return RegionType::Unmapped;

    return Get(LAYER_COUNT - 1, base) ? RegionType::Reserved : RegionType::Free;
}

// void BuddyAllocator::GetRegions(std::vector<Region>& regions)
// {
//     uint64_t currentRegionStart = 0;
//     bool currentRegionType = Get(LAYER_COUNT - 1, 0);

//     for (size_t i = 0; i <= BlocksOnLayer(LAYER_COUNT - 1); i++)
//     {
//         if (i == BlocksOnLayer(LAYER_COUNT - 1) || currentRegionType != Get(LAYER_COUNT - 1, i))
//         {
//             Region region;
//             region.Base = ToPtr(currentRegionStart);
//             region.Size = m_SmallBlockSize * (i - currentRegionStart);
//             region.Type = currentRegionType ? RegionType::Reserved : RegionType::Free;
//             regions.push_back(region);

//             if (i < BlocksOnLayer(LAYER_COUNT - 1)) 
//             {
//                 currentRegionStart = i;
//                 currentRegionType = Get(LAYER_COUNT - 1, i);
//             }
//         }
//     }
// }

void BuddyAllocator::Dump()
{
    JsonWriter writer(std::cout, true);
    writer.BeginObject();
    writer.Property("blockSize", m_SmallBlockSize);
    writer.Property("memBase", m_MemBase);
    writer.Property("blocksLayer0", m_BlocksLayer0);
    writer.Property("bitmapSize", m_BitmapSize);

    writer.BeginObject("bitmap");

    for (int layer = 0; layer < LAYER_COUNT; layer++)
    {
        std::stringstream bitmap;
        for (size_t i = 0; i < BlocksOnLayer(layer); i++) {
            bool used = Get(layer, i);

            if (used 
                && layer == m_LastAllocatedLayer  
                && i >= m_LastAllocatedBlock 
                && i < m_LastAllocatedBlock + m_LastAllocatedCount) 
            {
                bitmap << 2;
            }
            else
            {
                bitmap << static_cast<int>(used);
            }
        }

        writer.Property(std::to_string(layer), bitmap.str());
    }

    writer.EndObject();
    writer.EndObject();
}

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
    : Allocator(),
      m_LastAllocatedBlock(0),
      m_LastAllocatedLayer(-1)
{
}

bool BuddyAllocator::InitializeImpl(RegionBlocks regions[], size_t regionCount)
{
    m_SmallBlockSize = m_BlockSize;
    m_BigBlockSize = m_BlockSize * (1 << (LAYER_COUNT - 1));
    m_BlocksLayer0 = DivRoundUp(m_MemSizeBytes, m_BigBlockSize);
    m_BitmapSize = IndexOfLayer(LAYER_COUNT);

    // Find free region to fit BitmapSize
    RegionBlocks *freeRegion = nullptr;
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Type == RegionType::Free && regions[i].Size >= m_BitmapSize)
            freeRegion = &regions[i];
    }

    // no free space :(
    if (freeRegion == nullptr)
    {
        Debug::Error("BuddyAllocator", "Not enough free memory - needed %u!", m_BitmapSize);
        return false;
    }

    m_Bitmap = reinterpret_cast<uint8_t*>(ToPtr(freeRegion->Base));

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
        int layerFound = layer;
        uint64_t iFound = FindFreeBlock(layerFound);

        // out of memory
        if (iFound == (uint64_t)-1)
            return nullptr;

        // if we are on a lower level than "layer", we need to split all the blocks all the way to "layer"
        // always split on the left side
        int i = iFound;
        for (int l = layerFound; l < layer; l++, i <<= 1)
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

    // nothing found
    return nullptr;
}

uint64_t BuddyAllocator::FindFreeBlock(int& layer)
{
    for(; layer > 0; layer--)
    {
        auto count = BlocksOnLayer(layer);
        for (uint64_t i = 0; i < count; i += 2)
        {
            // a block is considered free if its buddy is used (e.g. 01 or 10)
            // otherwise we have to "split" a block on a lower layer
            if (Get(layer, i) != Get(layer, i + 1))
            {
                // return the free block   
                return (!Get(layer, i)) ? i : i + 1;
            }
        }
    }

    // haven't found any free block, just split a block from level 0
    auto count = BlocksOnLayer(layer);
    for (uint64_t i = 0; i < count; i++)
    {
        if (!Get(layer, i))
            return i;
    }

    // out of memory
    return (uint64_t)-1;
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
    // start by marking everything on the last layer
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

void BuddyAllocator::DumpImpl(JsonWriter& writer)
{
    writer.Property("smallBlockSize", m_SmallBlockSize);
    writer.Property("bigBlockSize", m_BigBlockSize);
    writer.Property("bitmapSize", m_BitmapSize);
    writer.Property("blocksLayer0", m_BlocksLayer0);

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
}


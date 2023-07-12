#include "BBSTAllocator.hpp"
#include <algorithm>
#include <memory.h>
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>

BBSTBlock::BBSTBlock(uint64_t base, 
                    uint64_t size,
                    RegionType type)
    : Base(base),
      Size(size),
      Type(type)
{
}

BBSTAllocator::BBSTAllocator()
    : Allocator(),
      m_Map()
{
}

bool BBSTAllocator::InitializeImpl(RegionBlocks regions[], size_t regionCount)
{
    for (size_t i = 0; i < regionCount; i++)
    {
        m_Map.emplace(regions[i].Base, BBSTBlock(regions[i].Base, regions[i].Size, regions[i].Type));
    }

    return true;
}

ptr_t BBSTAllocator::Allocate(uint32_t blocks)
{
    if (blocks == 0)
        return nullptr;

    // find region
    for (auto it = m_Map.begin(); it != m_Map.end(); it++)
    {
        if (it->second.Type == RegionType::Free && it->second.Size >= blocks)
        {
            ptr_t ret = ToPtr(it->second.Base);

            // size is equal, just modify the type of the existing block
            if (it->second.Size == blocks)
            {
                it->second.Type = RegionType::Reserved;
            }
            else
            {
                BBSTBlock block = it->second;
                m_Map.erase(it);
                m_Map.emplace(block.Base, BBSTBlock(block.Base, blocks, RegionType::Reserved));

                block.Base += blocks;
                block.Size -= blocks;
                m_Map.emplace(block.Base, block);
            }
            return ret;
        }
    }

    return nullptr;
}

void BBSTAllocator::Free(void* basePtr, uint32_t blocks)
{
    uint64_t base = ToBlock(basePtr);

    // Find node
    auto it = m_Map.find(base);
    
    // not found, or region is already free
    if (it == m_Map.end())
        return;

    it->second.Type = RegionType::Free;

    // can we merge with predecessor?
    if (it != m_Map.begin())
    {
        auto prev = it;
        --prev;

        if (prev->second.Type == RegionType::Free)
        {
            prev->second.Size += it->second.Size;
            m_Map.erase(it);
            it = prev;
        }
    }
    
    // can we merge with the successor
    auto next = it;
    ++next;
    if (next != m_Map.end() && next->second.Type == RegionType::Free)
    {
        it->second.Size += next->second.Size;
        m_Map.erase(next);
    }
}

// for statistics
RegionType BBSTAllocator::GetState(ptr_t address)
{
    uint64_t block = ToBlock(address);

    auto it = m_Map.upper_bound(block);     // returns iterator to first element GREATER than 'block'
    if (it == m_Map.begin())
        return RegionType::Unmapped;

    --it;
    if (block >= it->second.Base && block < it->second.Base + it->second.Size)
        return it->second.Type;
        
    return RegionType::Unmapped;
}

// for debugging
void BBSTAllocator::DumpImpl(JsonWriter& writer)
{
    writer.BeginArray("blockList");

    for (auto& pair : m_Map)
    {
        writer.BeginObject();
        writer.Property("base", pair.second.Base);
        writer.Property("size", pair.second.Size);
        writer.Property("type", static_cast<int>(pair.second.Type));
        writer.EndObject();
    }

    writer.EndArray();
}

uint64_t BBSTAllocator::MeasureWastedMemory()
{
    // not accurate
    size_t mapSize = sizeof(m_Map) + m_Map.size() * sizeof(std::multimap<uint64_t, BBSTBlock>::node_type);
    return DivRoundUp(sizeof(*this) + mapSize, m_BlockSize);
}

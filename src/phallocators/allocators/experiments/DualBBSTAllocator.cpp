#include "DualBBSTAllocator.hpp"
#include <algorithm>
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>

DualBBSTRegion::DualBBSTRegion(uint64_t base,
                             uint64_t size,
                             RegionType type)
    : Base(base),
      Size(size),
      Type(type)
{
}

DualBBSTAllocator::DualBBSTAllocator()
    : Allocator(),
      m_FreeMap(),
      m_ReservedMap()
{
}

bool DualBBSTAllocator::InitializeImpl(RegionBlocks regions[], size_t regionCount)
{
    // Initially add all the regions to the "reseved" map
    for (size_t i = 0; i < regionCount; i++)
    {
        uint64_t base = regions[i].Base;
        uint64_t size = regions[i].Size;

        if (regions[i].Type == RegionType::Free)
            m_FreeMap.emplace(base, DualBBSTRegion(base, size, regions[i].Type));
        else
            m_ReservedMap.emplace(base, DualBBSTRegion(base, size, regions[i].Type));
    }

    return true;
}

ptr_t DualBBSTAllocator::Allocate(uint32_t blocks)
{
    if (blocks == 0)
        return nullptr;

    for (auto it = m_FreeMap.begin(); it != m_FreeMap.end(); it++)
    {
        if (it->second.Size >= blocks)
        {
            ptr_t ret = ToPtr(it->second.Base);

            DualBBSTRegion block = it->second;
            m_FreeMap.erase(it);
            m_ReservedMap.emplace(block.Base, DualBBSTRegion(block.Base, blocks, RegionType::Reserved));

            if (block.Size > blocks)
            {
                block.Base += blocks;
                block.Size -= blocks;
                m_FreeMap.emplace(block.Base, block);
            }
            
            return ret;
        }
    }

    return nullptr;
}

void DualBBSTAllocator::Free(void* basePtr, uint32_t blocks)
{
    uint64_t base = ToBlock(basePtr);

    // Find node
    auto it = m_ReservedMap.find(base);
    if (it == m_ReservedMap.end())
        return;

    // Remove from reserved map, add to free map
    DualBBSTRegion block = it->second;
    m_ReservedMap.erase(it);

    block.Type = RegionType::Free;
    auto freeIt = m_FreeMap.emplace(block.Base, block);

	// Can we merge with predecessor?
	for (auto predIt = m_FreeMap.begin(); predIt != m_FreeMap.end(); predIt++)
	{
		if (predIt->second.Base <= freeIt->second.Base && predIt->second.Base + predIt->second.Size >= freeIt->second.Base)
		{
			auto predBase = predIt->second.Base;
			predIt->second.Size += freeIt->second.Size;
			m_FreeMap.erase(freeIt);

			// iterators are invalidated, so restore them
			freeIt = m_FreeMap.find(predBase);
			break;
		}
	}

	// Can we merge with successor?
	for (auto succIt = m_FreeMap.begin(); succIt != m_FreeMap.end(); succIt++)
	{
		if (freeIt->second.Base <= succIt->second.Base && freeIt->second.Base + freeIt->second.Size >= succIt->second.Base)
		{
			freeIt->second.Size += succIt->second.Size;
			m_FreeMap.erase(succIt);
			break;
		}
	}
}

// for statistics
RegionType DualBBSTAllocator::GetState(ptr_t address)
{
    uint64_t block = ToBlock(address);

    auto it = m_ReservedMap.upper_bound(block);     // returns iterator to first element GREATER than 'block'
    if (it != m_ReservedMap.begin())
    {
        --it;
        if (block >= it->second.Base && block < it->second.Base + it->second.Size)
            return it->second.Type;
    }

    it = m_FreeMap.upper_bound(block);             // returns iterator to first element GREATER than 'block'
    if (it != m_FreeMap.begin())
    {
        --it;
        if (block >= it->second.Base && block < it->second.Base + it->second.Size)
            return it->second.Type;
    }

    return RegionType::Unmapped;
}

// for debugging
void DualBBSTAllocator::DumpImpl(JsonWriter& writer)
{
    writer.BeginArray("freeMap");

    for (auto& pair : m_FreeMap)
    {
        writer.BeginObject();
        writer.Property("base", pair.second.Base);
        writer.Property("size", pair.second.Size);
        writer.Property("type", static_cast<int>(pair.second.Type));
        writer.EndObject();
    }

    writer.EndArray();
    writer.BeginArray("reservedMap");

    for (auto& pair : m_ReservedMap)
    {
        writer.BeginObject();
        writer.Property("base", pair.second.Base);
        writer.Property("size", pair.second.Size);
        writer.Property("type", static_cast<int>(pair.second.Type));
        writer.EndObject();
    }

    writer.EndArray();
}

uint64_t DualBBSTAllocator::MeasureWastedMemory()
{
    // not accurate
    size_t freeSize = sizeof(m_FreeMap) + m_FreeMap.size() * sizeof(std::multimap<uint64_t, DualBBSTRegion>::node_type); // NOLINT(bugprone-sizeof-container)
    size_t usedSize = sizeof(m_ReservedMap) + m_ReservedMap.size() * sizeof(std::multimap<uint64_t, DualBBSTRegion>::node_type); // NOLINT(bugprone-sizeof-container)
    return DivRoundUp(sizeof(*this) + freeSize + usedSize, m_BlockSize);
}

#include "Allocator.hpp"
#include <algorithm>
#include <memory.h>
#include <cassert>
#include <fstream>

template <typename T>
void ArrayDeleteElement(T array[], size_t indexToDelete, size_t& count)
{
    memmove(array + indexToDelete, array + indexToDelete + 1, sizeof(T) * (count - indexToDelete - 1));
    --count;
}

class RegionCompare
{
public:
    bool operator()(const RegionBlocks& a, const RegionBlocks& b)
    {
        if (a.Base == b.Base) 
            return a.Size < b.Size;
        return a.Base < b.Base;
    }
};

Allocator::Allocator()
    : m_BlockSize(),
      m_MemSizeBytes(),
      m_MemSize(),
      m_MemBase(nullptr)
{    
}

bool Allocator::Initialize(uint64_t blockSize, const Region regions[], size_t regionCount) 
{
    m_BlockSize = blockSize;
    DetermineMemoryRange(regions, regionCount);

    RegionBlocks tempRegions[1024];
    assert(regionCount < 1024);
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Type == RegionType::Free) 
        {
            tempRegions[i].Base = ToBlockRoundUp(regions[i].Base);
            tempRegions[i].Size = regions[i].Size / blockSize;
        }
        else 
        {
            tempRegions[i].Base = ToBlock(regions[i].Base);
            tempRegions[i].Size = DivRoundUp(regions[i].Size, blockSize);
        }
        tempRegions[i].Type = regions[i].Type;

    }

    FixOverlappingRegions(tempRegions, regionCount);
    return InitializeImpl(tempRegions, regionCount);
}

void Allocator::DetermineMemoryRange(const Region regions[], size_t regionCount)
{
    // determine where memory begins and ends
    auto* memBase = reinterpret_cast<uint8_t*>(-1);
    uint8_t* memEnd = nullptr;

    // go through list of blocks to determine 3 things: where memory begins, where it ends and biggest free region
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Base < memBase)
            memBase = reinterpret_cast<uint8_t*>(regions[i].Base);

        if (reinterpret_cast<uint8_t*>(regions[i].Base) + regions[i].Size > memEnd)
            memEnd = reinterpret_cast<uint8_t*>(regions[i].Base) + regions[i].Size;
    }

    m_MemBase = memBase;
    m_MemSizeBytes = memEnd - memBase;
    m_MemSize = m_MemSizeBytes / m_BlockSize;
}

void Allocator::FixOverlappingRegions(RegionBlocks regions[], size_t& regionCount)
{
    std::sort(regions, regions + regionCount, RegionCompare());

    for (size_t i = 0; i < regionCount; i++)
    {
        // delete 0 sized regions
        if (regions[i].Size == 0)
        {
            ArrayDeleteElement(regions, i, regionCount);
            --i;
        }
        else if (i < regionCount - 1)
        {
            // Regions of the same type overlapping/touching? Merge them
            if (regions[i].Type == regions[i+1].Type &&
                regions[i].Base + regions[i].Size >= regions[i+1].Base)
            {
                uint64_t end = std::max(regions[i].Base + regions[i].Size,
                                        regions[i+1].Base + regions[i+1].Size);
                regions[i].Size = end - regions[i].Base;
                
                ArrayDeleteElement(regions, i + 1, regionCount);
                --i;
            }

            // Regions have different types, but they overlap - prioritize reserved/used blocks
            // note: blocks are already sorted by base and size
            else if (regions[i].Type != regions[i+1].Type &&
                     regions[i].Base + regions[i].Size > regions[i+1].Base)
            {
                uint64_t overlapSize = regions[i].Base + regions[i].Size -
                                       regions[i+1].Base;
                    
                // reserved blocks have priority
                if (regions[i].Type != RegionType::Free)
                {
                    // give overlapping space to current block
                    if (overlapSize < regions[i+1].Size)
                    {   
                        regions[i+1].Base = regions[i+1].Base + overlapSize;
                        regions[i+1].Size -= overlapSize;
                    }
                    // current block completely overlaps next one... just remove the next one
                    else
                    {
                        ArrayDeleteElement(regions, i + 1, regionCount);
                        --i;
                    }
                }
                    
                // give overlapping space to next block
                else if (overlapSize < regions[i].Size)
                {
                    regions[i].Size -= overlapSize;
                }
   
                // next block completely overlaps current one... delete current one
                else 
                {
                    ArrayDeleteElement(regions, i, regionCount);
                    --i;
                }
            }
        }
    }
}

void Allocator::Dump(const std::string& filename)
{
    std::ofstream fout;
    std::ostream* out = &std::cout;

    if (!filename.empty())
    {
        fout.open(filename, std::ios::out);
        out = &fout;
    }

    JsonWriter writer(*out, true);
    writer.BeginObject();
    writer.Property("memBase", m_MemBase);
    writer.Property("memSizeBytes", m_MemSizeBytes);
    writer.Property("memSize", m_MemSize);
    writer.Property("blockSize", m_BlockSize);
    DumpImpl(writer);
    writer.EndObject();
}

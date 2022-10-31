#include "LinkedListAllocator.hpp"
#include <algorithm>
#include <memory.h>
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>

void LinkedListBlock::Clear()
{
    this->BlockUsed = false;
}

void LinkedListBlock::Set(uint64_t base, 
                          uint64_t size,
                          RegionType type,
                          LinkedListBlock* prev,
                          LinkedListBlock* next)
{
    this->Base = base;
    this->Size = size;
    this->Type = type;
    this->Prev = prev;
    this->Next = next;
    this->BlockUsed = true;
}

LinkedListAllocator::LinkedListAllocator()
    : m_BlockSize(0),
      m_MemBase(nullptr),
      m_First(nullptr),
      m_Last(nullptr),
      m_LastAllocated(nullptr),
      m_TotalCapacity(STATIC_POOL_SIZE),
      m_UsedBlocks(0),
      m_CurrentPool(&m_FirstPool),
      m_CurrentPoolNextFree(0)
{
    memset(m_StaticBlockPool, 0, sizeof(m_StaticBlockPool));
    m_FirstPool.Blocks = m_StaticBlockPool;
    m_FirstPool.Size = STATIC_POOL_SIZE;
    m_FirstPool.Next = &m_FirstPool;
}

bool LinkedListAllocator::Initialize(uint64_t blockSize, Region regions[], size_t regionCount)
{
    m_BlockSize = blockSize;
    m_First = nullptr;
    m_Last = nullptr;

    // determine mem base
    m_MemBase = reinterpret_cast<uint8_t*>(-1);

    // determine where memory begins
    for (size_t i = 0; i < regionCount; i++)
    {
        if (regions[i].Base < m_MemBase)
            m_MemBase = reinterpret_cast<uint8_t*>(regions[i].Base);
    }

    for (size_t i = 0; i < regionCount; i++)
        AddRegion(regions[i].Base, regions[i].Size, regions[i].Type);

    return true;
}

void LinkedListAllocator::AddRegion(ptr_t basePtr, uint64_t sizeBytes, RegionType type) 
{
    uint64_t base = ToBlock(basePtr);
    uint64_t size = DivRoundUp(sizeBytes, m_BlockSize);

    LinkedListBlock* block = NewBlock();
    block->Set(base, size, type);

    // first block in the list
    if (m_First == nullptr)
    {
        m_First = block;
        m_Last = block;
    }
    else
    {
        // find insertion position
        LinkedListBlock* current = m_First;
        while (current != nullptr && (base > current->Base || (base == current->Base && size > current->Size)))
            current = current->Next;

        InsertBlock(block, current);
        CheckAndMergeBlocks();
    }
}

ptr_t LinkedListAllocator::Allocate(uint32_t blocks)
{
    ptr_t ret = AllocateInternal(blocks, RegionType::Reserved);

    // over 80% usage => add another block pool
    if (m_UsedBlocks >= (m_TotalCapacity * 4) / 5)
    {
        // allocate another pool
        uint8_t* u8NewPool = reinterpret_cast<uint8_t*>(AllocateInternal(1, RegionType::Allocator));
        
        LinkedListBlockPool* newPool = reinterpret_cast<LinkedListBlockPool*>(u8NewPool);
        newPool->Blocks = reinterpret_cast<LinkedListBlock*>(u8NewPool + sizeof(LinkedListBlockPool));
        newPool->Size = (m_BlockSize - sizeof(LinkedListBlockPool)) / sizeof(LinkedListBlock);
        memset(newPool->Blocks, 0, sizeof(LinkedListBlock) * newPool->Size);

        newPool->Next = m_FirstPool.Next;
        m_FirstPool.Next = newPool;
        m_TotalCapacity += newPool->Size;
    }

    return ret;
}

ptr_t LinkedListAllocator::AllocateInternal(uint32_t blocks, RegionType type)
{
    if (blocks == 0)
        return nullptr;

    LinkedListBlock* found = nullptr;

#if LINKED_LIST_ALLOCATOR_STRATEGY == FIRST_FIT
    LinkedListBlock* current = m_First;
    while (current != nullptr && (current->Type != RegionType::Free || current->Size < blocks))
        current = current->Next;

    found = current;

#elif LINKED_LIST_ALLOCATOR_STRATEGY == NEXT_FIT
    LinkedListBlock* current = m_LastAllocated;
    while (current != m_LastAllocated && (current->Type != RegionType::Free || current->Size < blocks))
    {
        current = current->Next;
        if (current == nullptr)
            current = m_First;
    }

    found = current;

#elif LINKED_LIST_ALLOCATOR_STRATEGY == BEST_FIT
    LinkedListBlock* current = m_First;
    while (current != nullptr)
    {
        if (current->Type == RegionType::Free 
            && current->Size > blocks 
            && (found == nullptr || current->Size < found->Size))
            found = current;
    }

#else
    LinkedListBlock* current = m_First;
    while (current != nullptr)
    {
        if (current->Type == RegionType::Free 
            && current->Size > blocks 
            && (found == nullptr || current->Size > found->Size))
            found = current;
    }

#endif

    // out of memory?
    if (found == nullptr)
        return nullptr;

    ptr_t ret = ToPtr(found->Base);

    // create reserved block
    if (found->Size == blocks)
    {
        found->Type = type;
    }
    else
    {
        LinkedListBlock* newBlock = NewBlock();
        newBlock->Set(found->Base, blocks, type);
        InsertBlock(newBlock, found);

        found->Base += blocks;
        found->Size -= blocks;
    }

    return ret;    
}

void LinkedListAllocator::Free(void* basePtr, uint32_t blocks)
{
    uint64_t base = ToBlock(basePtr);

    LinkedListBlock* current = m_First;
    while (current != nullptr && base > current->Base)
        current = current->Next;
    
    if (current == nullptr 
        || current->Type == RegionType::Free
        || current->Base != base)
        return; // not found, or region is already free

    current->Type = RegionType::Free;

    // can we merge with the previous block?
    if (current->Prev != nullptr && current->Prev->Type == RegionType::Free)
    {
        current = current->Prev;
        current->Size += current->Next->Size;
        DeleteBlock(current->Next);
    }

    // can we merge with the next block
    if (current->Next != nullptr && current->Next->Type == RegionType::Free)
    {
        current->Size += current->Next->Size;
        DeleteBlock(current->Next);
    }

    // TODO: under 20% usage? compress and free up some pools
}

LinkedListBlock* LinkedListAllocator::NewBlock()
{
    do {
        if (m_CurrentPoolNextFree >= m_CurrentPool->Size)
        {
            m_CurrentPool = m_CurrentPool->Next;
            m_CurrentPoolNextFree = 0;
        }
        if (m_CurrentPool == nullptr)
        {
            m_CurrentPool = &m_FirstPool;
            m_CurrentPoolNextFree = 0;
        }
       
    } while (m_CurrentPool->Blocks[m_CurrentPoolNextFree++].BlockUsed);

    ++m_UsedBlocks;
    return &m_CurrentPool->Blocks[m_CurrentPoolNextFree - 1];
}

void LinkedListAllocator::InsertBlock(LinkedListBlock* block, LinkedListBlock* insertBefore)
{
    // last
    if (insertBefore == nullptr)
    {
        block->Prev = m_Last;
        block->Next = nullptr;
        m_Last->Next = block;
        m_Last = block;
    }
    // first
    else if (insertBefore->Prev == nullptr) 
    {
        block->Prev = nullptr;
        block->Next = m_First;
        m_First->Prev = block;
        m_First = block; 
    }
    // middle
    else
    {
        block->Prev = insertBefore->Prev;
        block->Next = insertBefore;
        insertBefore->Prev->Next = block;
        insertBefore->Prev = block;
    }
}

void LinkedListAllocator::DeleteBlock(LinkedListBlock* block)
{
    // first?
    if (block->Prev == nullptr)
        m_First = block->Next;
    else
        block->Prev->Next = block->Next;

    // last?
    if (block->Next == nullptr)
        m_Last = block->Prev;
    else
        block->Next->Prev = block->Prev;

    block->Clear();
    m_UsedBlocks--;
}

void LinkedListAllocator::CheckAndMergeBlocks()
{
    LinkedListBlock* current = m_First;
    while (current != nullptr)
    {
        LinkedListBlock* next = current->Next;

        // delete 0 sized blocks
        if (current->Size == 0)
        {
            DeleteBlock(current);
        }
        else if (next != nullptr)
        {
            // Free regions overlapping/touching? Merge them
            if (current->Type == RegionType::Free 
                && current->Type == next->Type 
                && current->Base + current->Size >= next->Base)
            {
                uint64_t end = std::max(current->Base + current->Size, next->Base + next->Size);
                current->Size = end - current->Base;
                DeleteBlock(next);
                continue;
            }

            // Regions have different types, but they overlap - prioritize reserved/used blocks
            // note: blocks are already sorted by base and size
            if (current->Type != next->Type && current->Base + current->Size > next->Base)
            {
                uint64_t overlapSize = current->Base + current->Size - next->Base;
                    
                // reserved blocks have priority
                if (current->Type != RegionType::Free)
                {
                    // give overlapping space to current block
                    if (overlapSize < next->Size)
                    {   
                        next->Base += overlapSize;
                        next->Size -= overlapSize;
                    }
                    // current block completely overlaps next one... just remove the 2nd one
                    else
                        DeleteBlock(next);
                }
                else
                {
                    // give overlapping space to next block
                    if (overlapSize < current->Size)
                    {
                        current->Size -= overlapSize;
                    }
                    // next block completely overlaps current one... delete current one
                    else 
                        DeleteBlock(current);
                }
            }
        }
        current = next;
    }
}

// for statistics
RegionType LinkedListAllocator::GetState(ptr_t address)
{
    uint64_t block = ToBlock(address);
    for (auto current = m_First; current != nullptr; current = current->Next)
    {
        if (block >= current->Base && block < current->Base + current->Size)
            return current->Type;
    }

    return RegionType::Unmapped;
}
// void LinkedListAllocator::GetRegions(std::vector<Region>& regions)
// {
// }

// for debugging
void LinkedListAllocator::Dump()
{
    JsonWriter writer(std::cout, true);
    writer.BeginObject();
    writer.Property("blockSize", m_BlockSize);
    writer.Property("memBase", m_MemBase);
    writer.BeginArray("blockList");

    for (auto current = m_First; current != nullptr; current = current->Next)
    {
        writer.BeginObject();
        writer.Property("id", current);
        writer.Property("prev", current->Prev);
        writer.Property("next", current->Next);
        writer.Property("base", current->Base);
        writer.Property("size", current->Size);
        writer.Property("type", static_cast<int>(current->Type));
        writer.EndObject();
    }

    writer.EndArray();
    writer.EndObject();
}
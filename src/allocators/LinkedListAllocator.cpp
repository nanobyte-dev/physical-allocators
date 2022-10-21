#include "LinkedListAllocator.hpp"
#include <algorithm>

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
    : m_First(nullptr),
      m_Last(nullptr)
{
}

bool LinkedListAllocator::Initialize(uint64_t blockSize, Region regions[], size_t regionCount)
{
    m_BlockSize = blockSize;
    m_First = nullptr;
    m_Last = nullptr;

    for (size_t i = 0; i < regionCount; i++)
        AddRegion(regions[i].Base, regions[i].Size, regions[i].Type);

    return true;
}

void LinkedListAllocator::AddRegion(ptr_t basePtr, uint64_t sizeBytes, RegionType type) 
{
    uint64_t base = reinterpret_cast<uint64_t>(basePtr) / m_BlockSize;
    uint64_t size = (sizeBytes + m_BlockSize - 1) / m_BlockSize;

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

void* LinkedListAllocator::Allocate(uint32_t blocks)
{
    LinkedListBlock* found = nullptr;

#if LINKED_LIST_ALLOCATOR_STRATEGY == FIRST_FIT
    LinkedListBlock* current = m_First;
    while (current != nullptr && (current->Type == RegionType::Reserved || current->Size < blocks))
        current = current->Next;

    found = current;

#elif LINKED_LIST_ALLOCATOR_STRATEGY == NEXT_FIT
    LinkedListBlock* current = m_LastAllocated;
    while (current != m_LastAllocated && (current->Type == RegionType::Reserved || current->Size < blocks))
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

    ptr_t ret = reinterpret_cast<ptr_t>(found->Base * m_BlockSize);

    // create reserved block
    if (found->Size == blocks)
    {
        found->Type = RegionType::Reserved;
    }
    else
    {
        LinkedListBlock* newBlock = NewBlock();
        newBlock->Set(found->Base, blocks, RegionType::Reserved);
        InsertBlock(newBlock, found);

        found->Base += blocks;
        found->Size -= blocks;
    }

    return ret;    
}

void LinkedListAllocator::Free(void* basePtr, uint32_t blocks)
{
    uint64_t base = reinterpret_cast<uint64_t>(basePtr) / m_BlockSize;

    LinkedListBlock* current = m_First;
    while (current != nullptr && base > current->Base + current->Size)
        current = current->Next;
    
    if (current == nullptr || current->Type == RegionType::Free)
        return; // not found, or region is already free

    // Split current block into 3... the used portion before, the free portion, and the used portion after
    if (base > current->Base)
    {
        uint64_t size = base - current->Base;
        LinkedListBlock* newBlock = NewBlock();
        newBlock->Set(current->Base, size, RegionType::Reserved);
        InsertBlock(newBlock, current);

        current->Base = base;
        current->Size -= size;
    }
    if (base + blocks > current->Base + current->Size)
    {

    }
}

LinkedListBlock* LinkedListAllocator::NewBlock()
{
    if (!m_BlockPool[m_BlockPoolNextFree].BlockUsed)
    {
        ++m_BlockPoolNextFree;
    }
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
            // Regions have same type and they are overlapping/touching? Merge them
            if (current->Type == next->Type && current->Base + current->Size >= next->Base)
            {
                uint64_t end = std::max(current->Base + current->Size, next->Base + next->Size);
                current->Size = end - current->Base;
                DeleteBlock(next);
                continue;
            }

            // Regions have different types, but they overlap - prioritize rezerved/used blocks
            // note: blocks are already sorted by base and size
            if (current->Type != next->Type && current->Base + current->Size > next->Base)
            {
                uint64_t overlapSize = current->Base + current->Size - next->Base;
                    
                // reserved blocks have priority
                if (current->Type == RegionType::Reserved)
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
    }
}

// for statistics
size_t LinkedListAllocator::GetRegionCount()
{
}

void LinkedListAllocator::GetRegion(size_t index, Region& regionOut)
{
}

// for debugging
void LinkedListAllocator::Dump()
{
    // todo
}
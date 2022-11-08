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
    : Allocator(),
      m_First(nullptr),
      m_Last(nullptr),
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

bool LinkedListAllocator::InitializeImpl(RegionBlocks regions[], size_t regionCount)
{
    m_First = nullptr;
    m_Last = nullptr;

    for (size_t i = 0; i < regionCount; i++)
    {
        LinkedListBlock* block = NewBlock();
        block->Set(regions[i].Base, regions[i].Size, regions[i].Type);

        // find insertion position
        LinkedListBlock* insertPos = FindInsertionPosition(block->Base, block->Size);
        InsertBlock(block, insertPos);
    }

    return true;
}

ptr_t LinkedListAllocator::Allocate(uint32_t blocks)
{
    ptr_t ret = AllocateInternal(blocks, RegionType::Reserved);

    // over 80% usage => add another block pool
    if (m_UsedBlocks >= (m_TotalCapacity * 4) / 5)
        GrowPool();

    return ret;
}

ptr_t LinkedListAllocator::AllocateInternal(uint32_t blocks, RegionType type)
{
    if (blocks == 0)
        return nullptr;

    LinkedListBlock* found = FindFreeRegion(blocks);

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
        DeleteAndReleaseBlock(current->Next);
    }

    // can we merge with the next block
    if (current->Next != nullptr && current->Next->Type == RegionType::Free)
    {
        current->Size += current->Next->Size;
        DeleteAndReleaseBlock(current->Next);
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

void LinkedListAllocator::ReleaseBlock(LinkedListBlock* block)
{
    block->Clear();
    m_UsedBlocks--;
}

void LinkedListAllocator::GrowPool()
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

LinkedListBlock* LinkedListAllocator::FindBlock(uint64_t base)
{
    LinkedListBlock* current = m_First;
    while (current != nullptr && base >= current->Base + current->Size)
        current = current->Next;

    if (current != nullptr && base >= current->Base)
        return current;

    return nullptr;
}

LinkedListBlock* LinkedListAllocator::FindInsertionPosition(uint64_t base, size_t size)
{
    LinkedListBlock* insertPos = m_First;
    while (insertPos != nullptr && (base > insertPos->Base || (base == insertPos->Base && size > insertPos->Size)))
        insertPos = insertPos->Next;

    return insertPos;
}

void LinkedListAllocator::InsertBlock(LinkedListBlock* block, LinkedListBlock* insertBefore)
{
    // First block in list
    if (m_First == nullptr)
    {
        m_First = m_Last = block;
        block->Next = nullptr;
        block->Prev = nullptr;
    }
    // last
    else if (insertBefore == nullptr)
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

}

void LinkedListAllocator::DeleteAndReleaseBlock(LinkedListBlock* block)
{
    DeleteBlock(block);
    ReleaseBlock(block);
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

// for debugging
void LinkedListAllocator::DumpImpl(JsonWriter& writer)
{
    writer.Property("totalCapacity", m_TotalCapacity);
    writer.Property("usedBlocks", m_UsedBlocks);
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
}

LinkedListBlock* LinkedListAllocatorFirstFit::FindFreeRegion(uint32_t blocks)
{
    LinkedListBlock* current = m_First;
    while (current != nullptr && (current->Type != RegionType::Free || current->Size < blocks))
        current = current->Next;

    return current;
}

LinkedListAllocatorNextFit::LinkedListAllocatorNextFit()
    : LinkedListAllocator(),
      m_Next(nullptr)
{
}

LinkedListBlock* LinkedListAllocatorNextFit::FindFreeRegion(uint32_t blocks)
{
    if (m_Next == nullptr)
        m_Next = m_First;

    LinkedListBlock* current = m_Next;
    while (current->Type != RegionType::Free || current->Size < blocks)
    {
        current = current->Next;

        if (current == nullptr)
            current = m_First;

        // we wrapped around and found nothing
        if (current == m_Next)
            return nullptr;
    }

    m_Next = current;
    return current;
}

void LinkedListAllocatorNextFit::DeleteBlock(LinkedListBlock* block)
{
    LinkedListAllocator::DeleteBlock(block);

    if (block == m_Next)
        m_Next = nullptr;
}


LinkedListBlock* LinkedListAllocatorBestFit::FindFreeRegion(uint32_t blocks)
{
    LinkedListBlock* found = nullptr;
    LinkedListBlock* current = m_First;

    while (current != nullptr)
    {
        if (current->Type == RegionType::Free 
            && current->Size > blocks 
            && (found == nullptr || current->Size < found->Size))
            found = current;

        current = current->Next;
    }

    return found;
}

LinkedListBlock* LinkedListAllocatorWorstFit::FindFreeRegion(uint32_t blocks)
{
    LinkedListBlock* found = nullptr;
    LinkedListBlock* current = m_First;
    
    while (current != nullptr)
    {
        if (current->Type == RegionType::Free 
            && current->Size > blocks 
            && (found == nullptr || current->Size > found->Size))
            found = current;
            
        current = current->Next;
    }

    return found;
}
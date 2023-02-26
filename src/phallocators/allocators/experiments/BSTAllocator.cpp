#include "BSTAllocator.hpp"
#include <algorithm>
#include <memory.h>
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>

void BSTBlock::Clear()
{
    this->BlockUsed = false;
}

void BSTBlock::Set(uint64_t base, 
                   uint64_t size,
                   RegionType type,
                   BSTBlock* parent,
                   BSTBlock* left,
                   BSTBlock* right)
{
    this->Base = base;
    this->Size = size;
    this->Type = type;
    this->Parent = parent;
    this->Left = left;
    this->Right = right;
    this->BlockUsed = true;
}

BSTAllocator::BSTAllocator()
    : Allocator(),
      m_Root(nullptr),
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

bool BSTAllocator::InitializeImpl(RegionBlocks regions[], size_t regionCount)
{
    m_Root = nullptr;

    for (size_t i = 0; i < regionCount; i++)
    {
        BSTBlock* block = NewBlock();
        block->Set(regions[i].Base, regions[i].Size, regions[i].Type);
        InsertBlock(block);
    }

    return true;
}

ptr_t BSTAllocator::Allocate(uint32_t blocks)
{
    ptr_t ret = AllocateInternal(blocks, RegionType::Reserved);

    // over 80% usage => add another block pool
    if (m_UsedBlocks >= (m_TotalCapacity * 4) / 5)
    {
        // allocate another pool
        uint8_t* u8NewPool = reinterpret_cast<uint8_t*>(AllocateInternal(1, RegionType::Allocator));
        
        BSTBlockPool* newPool = reinterpret_cast<BSTBlockPool*>(u8NewPool);
        newPool->Blocks = reinterpret_cast<BSTBlock*>(u8NewPool + sizeof(BSTBlockPool));
        newPool->Size = (m_BlockSize - sizeof(BSTBlockPool)) / sizeof(BSTBlock);
        memset(newPool->Blocks, 0, sizeof(BSTBlock) * newPool->Size);

        newPool->Next = m_FirstPool.Next;
        m_FirstPool.Next = newPool;
        m_TotalCapacity += newPool->Size;
    }

    return ret;
}

ptr_t BSTAllocator::AllocateInternal(uint32_t blocks, RegionType type)
{
    if (blocks == 0)
        return nullptr;

    // find region
    BSTBlock* found = FindFreeRegion(m_Root, blocks);

    // out of memory?
    if (found == nullptr)
        return nullptr;

    ptr_t ret = ToPtr(found->Base);

    // create reserved block
    if (found->Size == blocks)
    {
        // size is equal, just modify the type of the existing block
        found->Type = type;
    }
    else
    {
        // size not equal, split block in 2
        BSTBlock* newBlock = NewBlock();
        newBlock->Set(found->Base, blocks, type);

        found->Base += blocks;
        found->Size -= blocks;

        DeleteBlock(found);
        InsertBlock(newBlock);
        InsertBlock(found);
    }

    return ret;    
}

BSTBlock* BSTAllocator::FindFreeRegion(BSTBlock* root, size_t blocks)
{
    // find region
    if (root == nullptr)
        return nullptr;

    // found
    if (root->Type == RegionType::Free && root->Size >= blocks)
        return root;

    // go left
    BSTBlock* left = FindFreeRegion(root->Left, blocks);
    if (left != nullptr)
        return left;

    // go right
    return FindFreeRegion(root->Right, blocks);
}

void BSTAllocator::Free(void* basePtr, uint32_t blocks)
{
    uint64_t base = ToBlock(basePtr);

    // Find node
    BSTBlock* current = m_Root;
    while (current != nullptr && current->Base != base)
        current = (base < current->Base) ? current->Left : current->Right;
    
    // not found, or region is already free
    if (current == nullptr || current->Type == RegionType::Free)
        return;

    current->Type = RegionType::Free;

    // can we merge with predecessor?
    BSTBlock* prev = GetPredecessor(current);
    if (prev != nullptr && prev->Type == RegionType::Free)
    {
        prev->Size += current->Size;
        DeleteAndReleaseBlock(current);
        current = prev;
    }

    // can we merge with the successor
    BSTBlock* next = GetSuccessor(current);
    if (next != nullptr && next->Type == RegionType::Free)
    {
        current->Size += next->Size;
        DeleteAndReleaseBlock(next);
    }

    // TODO: under 20% usage? compress and free up some pools
}

BSTBlock* BSTAllocator::NewBlock()
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

void BSTAllocator::ReleaseBlock(BSTBlock* block)
{
    block->Clear();
    m_UsedBlocks--;
}


void BSTAllocator::InsertBlock(BSTBlock* block)
{
    BSTBlock* parent = nullptr;
    BSTBlock* current = m_Root;
    while (current != nullptr)
    {
        parent = current;
        current = (block->Base < current->Base) 
            ? current->Left
            : current->Right;
    }

    // insert
    block->Parent = parent;
    if (parent == nullptr)
        m_Root = block;
    else if (block->Base < parent->Base)
        parent->Left = block;
    else
        parent->Right = block;
}

void BSTAllocator::DeleteBlock(BSTBlock* block)
{
    if (block->Left == nullptr)
        ReplaceBlockWith(block, block->Right);
    else if (block->Right == nullptr)
        ReplaceBlockWith(block, block->Left);
    else
    {
        BSTBlock* next = GetSuccessor(block);
        if (next->Parent != block)
        {
            ReplaceBlockWith(next, next->Right);
            next->Right = block->Right;
            next->Right->Parent = next;
        }
        ReplaceBlockWith(block, next);
        next->Left = block->Left;
        next->Left->Parent = next;
    }

    block->Parent = nullptr;
    block->Left = nullptr;
    block->Right = nullptr;
}

void BSTAllocator::ReplaceBlockWith(BSTBlock* block, BSTBlock* replaceWith)
{
    if (block == m_Root)
        m_Root = replaceWith;
    else if (block == block->Parent->Left)
        block->Parent->Left = replaceWith;
    else if (block == block->Parent->Right)
        block->Parent->Right = replaceWith;

    if (replaceWith != nullptr)
        replaceWith->Parent = block->Parent;
}



BSTBlock* BSTAllocator::GetFirst()
{
    BSTBlock* current = m_Root;
    while (current->Left != nullptr)
        current = current->Left;

    return current;
}

BSTBlock* BSTAllocator::GetPredecessor(BSTBlock* block)
{
    if (block == nullptr)
        return nullptr;

    if (block->Left != nullptr)
    {
        // Return right-est item in the left subtree
        block = block->Left;
        while (block->Right != nullptr)
            block = block->Right;

        return block;
    }

    while (block->Parent != nullptr)
    {
        if (block == block->Parent->Right)
            return block->Parent;
        else if (block == block->Parent->Left)
            block = block->Parent;
    }

    // last node
    return nullptr;   
}

BSTBlock* BSTAllocator::GetSuccessor(BSTBlock* block)
{
    if (block == nullptr)
        return nullptr;

    if (block->Right != nullptr)
    {
        // Return left-est item in the right subtree
        block = block->Right;
        while (block->Left != nullptr)
            block = block->Left;

        return block;
    }

    while (block->Parent != nullptr)
    {
        if (block == block->Parent->Left)
            return block->Parent;
        else if (block == block->Parent->Right)
            block = block->Parent;
    }

    // last node
    return nullptr;   
}

// for statistics
RegionType BSTAllocator::GetState(ptr_t address)
{
    uint64_t block = ToBlock(address);

    BSTBlock* prev = nullptr;
    BSTBlock* current = m_Root;
    while (current != nullptr && current->Base != block)
    {
        prev = current;
        current = (block < current->Base) ? current->Left : current->Right;
    }

    if (current == nullptr) 
    {
        current = prev;
        if (block < current->Base)
            current = GetPredecessor(prev);
    }
    
    if (current == nullptr || block >= current->Base + current->Size)
        return RegionType::Unmapped;

    return current->Type;
}

// for debugging
void BSTAllocator::DumpImpl(JsonWriter& writer)
{
    writer.BeginArray("blockList");

    for (auto current = GetFirst(); current != nullptr; current = GetSuccessor(current))
    {
        writer.BeginObject();
        writer.Property("id", current);
        writer.Property("parent", current->Parent);
        writer.Property("left", current->Left);
        writer.Property("right", current->Right);
        writer.Property("base", current->Base);
        writer.Property("size", current->Size);
        writer.Property("type", static_cast<int>(current->Type));
        writer.EndObject();
    }

    writer.EndArray();
}

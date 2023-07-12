#include "BSTAllocator.hpp"
#include <memory.h>
#include <math/MathHelpers.hpp>
#include <util/JsonWriter.hpp>

void BSTRegion::Clear()
{
    this->BlockUsed = false;
}

void BSTRegion::Set(uint64_t base, 
                   uint64_t size,
                   RegionType type,
                   BSTRegion* parent,
                   BSTRegion* left,
                   BSTRegion* right)
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
      m_UsedElements(0),
      m_FirstPool(),
	  m_StaticRegionPool(),
      m_CurrentPool(&m_FirstPool),
      m_CurrentPoolNextFree(0)
{
    memset(m_StaticRegionPool, 0, sizeof(m_StaticRegionPool));
    m_FirstPool.Regions  = m_StaticRegionPool;
    m_FirstPool.Size = STATIC_POOL_SIZE;
    m_FirstPool.Next = &m_FirstPool;
}

bool BSTAllocator::InitializeImpl(RegionBlocks regions[], size_t regionCount)
{
    m_Root = nullptr;

    for (size_t i = 0; i < regionCount; i++)
    {
        BSTRegion* region = NewRegion();
	    region->Set(regions[i].Base, regions[i].Size, regions[i].Type);
	    InsertRegion(region);
    }

    return true;
}

ptr_t BSTAllocator::Allocate(uint32_t blocks)
{
    ptr_t ret = AllocateInternal(blocks, RegionType::Reserved);

    // over 80% usage => add another block pool
    if (m_UsedElements >= (m_TotalCapacity * 4) / 5)
    {
        // allocate another pool
        auto* u8NewPool = reinterpret_cast<uint8_t*>(AllocateInternal(1, RegionType::Allocator));
        
        auto* newPool = reinterpret_cast<BSTRegionPool*>(u8NewPool);
        newPool->Regions = reinterpret_cast<BSTRegion*>(u8NewPool + sizeof(BSTRegionPool));
        newPool->Size = (m_BlockSize - sizeof(BSTRegionPool)) / sizeof(BSTRegion);
        memset(newPool->Regions, 0, sizeof(BSTRegion) * newPool->Size);

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
    BSTRegion* found = FindFreeRegion(m_Root, blocks);

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
        BSTRegion* newBlock = NewRegion();
        newBlock->Set(found->Base, blocks, type);

        found->Base += blocks;
        found->Size -= blocks;

	    DeleteRegion(found);
	    InsertRegion(newBlock);
	    InsertRegion(found);
    }

    return ret;    
}

BSTRegion* BSTAllocator::FindFreeRegion(BSTRegion* root, size_t blocks)
{
    // find region
    if (root == nullptr)
        return nullptr;

    // found
    if (root->Type == RegionType::Free && root->Size >= blocks)
        return root;

    // go left
    BSTRegion* left = FindFreeRegion(root->Left, blocks);
    if (left != nullptr)
        return left;

    // go right
    return FindFreeRegion(root->Right, blocks);
}

void BSTAllocator::Free(void* basePtr, uint32_t blocks)
{
    uint64_t base = ToBlock(basePtr);

    // Find node
    BSTRegion* current = m_Root;
    while (current != nullptr && current->Base != base)
        current = (base < current->Base) ? current->Left : current->Right;
    
    // not found, or region is already free
    if (current == nullptr || current->Type == RegionType::Free)
        return;

    current->Type = RegionType::Free;

    // can we merge with predecessor?
    BSTRegion* prev = GetPredecessor(current);
    if (prev != nullptr && prev->Type == RegionType::Free)
    {
        prev->Size += current->Size;
	    DeleteAndReleaseRegion(current);
        current = prev;
    }

    // can we merge with the successor
    BSTRegion* next = GetSuccessor(current);
    if (next != nullptr && next->Type == RegionType::Free)
    {
        current->Size += next->Size;
	    DeleteAndReleaseRegion(next);
    }

    // TODO: under 20% usage? compress and free up some pools
}

BSTRegion* BSTAllocator::NewRegion()
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
       
    } while (m_CurrentPool->Regions[m_CurrentPoolNextFree++].BlockUsed);

    ++m_UsedElements;
    return &m_CurrentPool->Regions[m_CurrentPoolNextFree - 1];
}

void BSTAllocator::ReleaseRegion(BSTRegion* region)
{
	region->Clear();
    m_UsedElements--;
}

void BSTAllocator::InsertRegion(BSTRegion* region)
{
    BSTRegion* parent = nullptr;
    BSTRegion* current = m_Root;
    while (current != nullptr)
    {
        parent = current;
        current = (region->Base < current->Base)
            ? current->Left
            : current->Right;
    }

    // insert
	region->Parent = parent;
    if (parent == nullptr)
        m_Root = region;
    else if (region->Base < parent->Base)
        parent->Left = region;
    else
        parent->Right = region;
}

void BSTAllocator::DeleteRegion(BSTRegion* region)
{
    if (region->Left == nullptr)
	    ReplaceRegionWith(region, region->Right);
    else if (region->Right == nullptr)
	    ReplaceRegionWith(region, region->Left);
    else
    {
        BSTRegion* next = GetSuccessor(region);
        if (next->Parent != region)
        {
	        ReplaceRegionWith(next, next->Right);
            next->Right = region->Right;
            next->Right->Parent = next;
        }
	    ReplaceRegionWith(region, next);
        next->Left = region->Left;
        next->Left->Parent = next;
    }

	region->Parent = nullptr;
	region->Left = nullptr;
	region->Right = nullptr;
}

void BSTAllocator::ReplaceRegionWith(BSTRegion* region, BSTRegion* replaceWith)
{
    if (region == m_Root)
        m_Root = replaceWith;
    else if (region == region->Parent->Left)
	    region->Parent->Left = replaceWith;
    else if (region == region->Parent->Right)
	    region->Parent->Right = replaceWith;

    if (replaceWith != nullptr)
        replaceWith->Parent = region->Parent;
}



BSTRegion* BSTAllocator::GetFirst()
{
    BSTRegion* current = m_Root;
    while (current->Left != nullptr)
        current = current->Left;

    return current;
}

BSTRegion* BSTAllocator::GetPredecessor(BSTRegion* region)
{
    if (region == nullptr)
        return nullptr;

    if (region->Left != nullptr)
    {
        // Return right-est item in the left subtree
        region = region->Left;
        while (region->Right != nullptr)
            region = region->Right;

        return region;
    }

    while (region->Parent != nullptr)
    {
        if (region == region->Parent->Right)
            return region->Parent;
        else if (region == region->Parent->Left)
            region = region->Parent;
    }

    // last node
    return nullptr;   
}

BSTRegion* BSTAllocator::GetSuccessor(BSTRegion* region)
{
    if (region == nullptr)
        return nullptr;

    if (region->Right != nullptr)
    {
        // Return left-est item in the right subtree
        region = region->Right;
        while (region->Left != nullptr)
            region = region->Left;

        return region;
    }

    while (region->Parent != nullptr)
    {
        if (region == region->Parent->Left)
            return region->Parent;
        else if (region == region->Parent->Right)
            region = region->Parent;
    }

    // last node
    return nullptr;   
}

// for statistics
RegionType BSTAllocator::GetState(ptr_t address)
{
    uint64_t block = ToBlock(address);

    BSTRegion* prev = nullptr;
    BSTRegion* current = m_Root;
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

uint64_t BSTAllocator::MeasureWastedMemory()
{
    uint64_t total = DivRoundUp(sizeof(*this), m_BlockSize);
    for (auto current = GetFirst(); current != nullptr; current = GetSuccessor(current))
    {
        if (current->Type == RegionType::Allocator)
            total += current->Size;
    }
    return total;
}

#include "Allocator.hpp"

#define STATIC_POOL_SIZE 256

struct LinkedListBlock
{
    uint64_t Base;
    uint64_t Size;
    RegionType Type;
    LinkedListBlock* Next;
    LinkedListBlock* Prev;
    bool BlockUsed;

    void Clear();
    void Set(uint64_t base, 
             uint64_t size,
             RegionType type,
             LinkedListBlock* prev = nullptr,
             LinkedListBlock* next = nullptr);
};

struct LinkedListBlockPool
{
    LinkedListBlockPool* Next;
    uint64_t Size;
    LinkedListBlock* Blocks;
};


class LinkedListAllocator : public Allocator
{
public:
    LinkedListAllocator();
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(void* base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;
    uint64_t MeasureWastedMemory() override;

protected:
    bool InitializeImpl(RegionBlocks regions[], size_t regionCount) override;
    void DumpImpl(JsonWriter& writer) override;

    virtual LinkedListBlock* FindFreeRegion(uint32_t blocks) = 0;

    // Block pool management
    LinkedListBlock* NewBlock();
    virtual void ReleaseBlock(LinkedListBlock* block);
    void GrowPool();

    // Linked list operations
    LinkedListBlock* FindBlock(uint64_t base);
    LinkedListBlock* FindInsertionPosition(uint64_t base, size_t size);
    void InsertBlock(LinkedListBlock* block, LinkedListBlock* insertBefore);
    virtual void DeleteBlock(LinkedListBlock* block);
    void DeleteAndReleaseBlock(LinkedListBlock* block);

private:
    ptr_t AllocateInternal(uint32_t blocks, RegionType type);

protected:
    LinkedListBlock *m_First, *m_Last;

    uint64_t m_TotalCapacity;
    uint64_t m_UsedBlocks;

    LinkedListBlockPool m_FirstPool;
    LinkedListBlock m_StaticBlockPool[STATIC_POOL_SIZE];
    LinkedListBlockPool* m_CurrentPool;
    uint32_t m_CurrentPoolNextFree;
};


class LinkedListAllocatorFirstFit : public LinkedListAllocator
{
protected:
    LinkedListBlock* FindFreeRegion(uint32_t blocks) override;
};


class LinkedListAllocatorNextFit : public LinkedListAllocator
{
public:
    LinkedListAllocatorNextFit();

protected:
    LinkedListBlock* FindFreeRegion(uint32_t blocks) override;
    void DeleteBlock(LinkedListBlock* block) override;

private:
    LinkedListBlock* m_Next;
};


class LinkedListAllocatorBestFit : public LinkedListAllocator
{
protected:
    LinkedListBlock* FindFreeRegion(uint32_t blocks) override;
};


class LinkedListAllocatorWorstFit : public LinkedListAllocator
{
protected:
    LinkedListBlock* FindFreeRegion(uint32_t blocks) override;
};
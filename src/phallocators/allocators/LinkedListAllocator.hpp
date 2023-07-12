#include "Allocator.hpp"

#define STATIC_POOL_SIZE 256

struct LinkedListRegion
{
    uint64_t Base;
    uint64_t Size;
    RegionType Type;
    LinkedListRegion* Next;
    LinkedListRegion* Prev;
    bool ElementUsed;

    void Clear();
    void Set(uint64_t base,
             uint64_t size,
             RegionType type,
             LinkedListRegion* prev = nullptr,
             LinkedListRegion* next = nullptr);
};

struct LinkedListRegionPool
{
    LinkedListRegionPool* Next;
    uint64_t Size;
    LinkedListRegion* Elements;
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

    virtual LinkedListRegion* FindFreeRegion(uint32_t blocks) = 0;

    // Block pool management
    LinkedListRegion* NewRegion();
    virtual void ReleaseRegion(LinkedListRegion* region);
    void GrowPool();

    // Linked list operations
    LinkedListRegion* FindRegion(uint64_t base);
    LinkedListRegion* FindInsertionPosition(uint64_t base, size_t size);
    void InsertRegion(LinkedListRegion* region, LinkedListRegion* insertBefore);
    virtual void DeleteRegion(LinkedListRegion* region);
    void DeleteAndReleaseRegion(LinkedListRegion* region);

private:
    ptr_t AllocateInternal(uint32_t blocks, RegionType type);

protected:
    LinkedListRegion *m_First, *m_Last;

    uint64_t m_PoolCapacity;
    uint64_t m_PoolUsedElements;

    LinkedListRegionPool m_FirstPool;
    LinkedListRegion m_StaticRegionPool[STATIC_POOL_SIZE];
    LinkedListRegionPool* m_CurrentPool;
    uint32_t m_CurrentPoolNextFree;
};


class LinkedListAllocatorFirstFit : public LinkedListAllocator
{
protected:
    LinkedListRegion* FindFreeRegion(uint32_t blocks) override;
};


class LinkedListAllocatorNextFit : public LinkedListAllocator
{
public:
    LinkedListAllocatorNextFit();

protected:
    LinkedListRegion* FindFreeRegion(uint32_t blocks) override;
    void DeleteRegion(LinkedListRegion* region) override;

private:
    LinkedListRegion* m_Next;
};


class LinkedListAllocatorBestFit : public LinkedListAllocator
{
protected:
    LinkedListRegion* FindFreeRegion(uint32_t blocks) override;
};


class LinkedListAllocatorWorstFit : public LinkedListAllocator
{
protected:
    LinkedListRegion* FindFreeRegion(uint32_t blocks) override;
};
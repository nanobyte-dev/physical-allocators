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
    bool Initialize(uint64_t blockSize, Region regions[], size_t regionCount) override;
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(void* base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;
    //void GetRegions(std::vector<Region>& regions) override;

    // for debugging
    void Dump() override;

protected:
    virtual LinkedListBlock* FindFreeRegion(uint32_t blocks) = 0;

    template<typename TPtr>
    inline uint64_t ToBlock(TPtr ptr)
    {
        uint8_t* u8Ptr = reinterpret_cast<uint8_t*>(ptr);
        return (u8Ptr - m_MemBase) / m_BlockSize;
    }

    inline ptr_t ToPtr(uint64_t block)
    {
        uint8_t* u8Ptr = m_MemBase + block * m_BlockSize;
        return reinterpret_cast<ptr_t>(u8Ptr);
    }

    LinkedListBlock* NewBlock();
    void AddRegion(ptr_t base, uint64_t size, RegionType type);
    void InsertBlock(LinkedListBlock* block, LinkedListBlock* insertBefore);
    virtual void DeleteBlock(LinkedListBlock* block);
    ptr_t Align(ptr_t addr);
    void CheckAndMergeBlocks();
    ptr_t AllocateInternal(uint32_t blocks, RegionType type);

    uint64_t m_BlockSize;
    uint8_t* m_MemBase;
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
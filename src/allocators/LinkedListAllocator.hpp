#include "Allocator.hpp"

#define LINKED_LIST_POOL_SIZE 1024

#define FIRST_FIT 1
#define NEXT_FIT 2
#define BEST_FIT 3
#define WORST_FIT 4

#define LINKED_LIST_ALLOCATOR_STRATEGY BEST_FIT

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


class LinkedListAllocator : public Allocator
{
public:
    LinkedListAllocator();
    bool Initialize(uint64_t blockSize, Region regions[], size_t regionCount) override;
    void* Allocate(uint32_t blocks = 1) override;
    void Free(void* base, uint32_t blocks) override;
    
    // for statistics
    void GetRegions(std::vector<Region>& regions) override;

    // for debugging
    void Dump() override;

private:
    LinkedListBlock* NewBlock();
    void AddRegion(ptr_t base, uint64_t size, RegionType type);
    void InsertBlock(LinkedListBlock* block, LinkedListBlock* insertBefore);
    void DeleteBlock(LinkedListBlock* block);
    ptr_t Align(ptr_t addr);
    void CheckAndMergeBlocks();

    uint64_t m_BlockSize;
    LinkedListBlock *m_First, *m_Last;
    LinkedListBlock *m_LastAllocated;
    LinkedListBlock m_BlockPool[LINKED_LIST_POOL_SIZE];
    uint32_t m_BlockPoolNextFree;
};
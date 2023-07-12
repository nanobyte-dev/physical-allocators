#include "../Allocator.hpp"

#define STATIC_POOL_SIZE 256

struct BSTRegion
{
    uint64_t Base;
    uint64_t Size;
    RegionType Type;
    BSTRegion* Parent;
    BSTRegion* Left;
    BSTRegion* Right;
    bool BlockUsed;

    void Clear();
    void Set(uint64_t base, 
             uint64_t size,
             RegionType type,
             BSTRegion* parent = nullptr,
             BSTRegion* left = nullptr,
             BSTRegion* right = nullptr);
};

struct BSTRegionPool
{
    BSTRegionPool* Next;
    uint64_t Size;
    BSTRegion* Regions;
};


class BSTAllocator : public Allocator
{
public:
    BSTAllocator();
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(void* base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;
    uint64_t MeasureWastedMemory() override;

protected:
    bool InitializeImpl(RegionBlocks regions[], size_t regionCount) override;
    void DumpImpl(JsonWriter& writer) override;
    
private:
    ptr_t AllocateInternal(uint32_t blocks, RegionType type);
    BSTRegion* FindFreeRegion(BSTRegion* root, size_t blocks);

    // Pool management
    BSTRegion* NewRegion();
    void ReleaseRegion(BSTRegion* region);
    void GrowPool();

    // Binary search tree operations
    void InsertRegion(BSTRegion* region);
    void DeleteRegion(BSTRegion* region);
    void ReplaceRegionWith(BSTRegion* region, BSTRegion* replaceWith);
    
    inline void DeleteAndReleaseRegion(BSTRegion* region)
    {
	    DeleteRegion(region);
	    ReleaseRegion(region);
    }

    BSTRegion* GetFirst();
    BSTRegion* GetPredecessor(BSTRegion* region);
    BSTRegion* GetSuccessor(BSTRegion* region);

    BSTRegion *m_Root;

    uint64_t m_TotalCapacity;
    uint64_t m_UsedElements;

    BSTRegionPool m_FirstPool;
    BSTRegion m_StaticRegionPool[STATIC_POOL_SIZE];
    BSTRegionPool* m_CurrentPool;
    uint32_t m_CurrentPoolNextFree;
};
#include "../Allocator.hpp"

#define STATIC_POOL_SIZE 256

struct BSTBlock
{
    uint64_t Base;
    uint64_t Size;
    RegionType Type;
    BSTBlock* Parent;
    BSTBlock* Left;
    BSTBlock* Right;
    bool BlockUsed;

    void Clear();
    void Set(uint64_t base, 
             uint64_t size,
             RegionType type,
             BSTBlock* parent = nullptr,
             BSTBlock* left = nullptr,
             BSTBlock* right = nullptr);
};

struct BSTBlockPool
{
    BSTBlockPool* Next;
    uint64_t Size;
    BSTBlock* Blocks;
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
    BSTBlock* FindFreeRegion(BSTBlock* root, size_t blocks);

    // Pool management
    BSTBlock* NewBlock();
    void ReleaseBlock(BSTBlock* block);
    void GrowPool();

    // Binary search tree operations
    void InsertBlock(BSTBlock* block);
    void DeleteBlock(BSTBlock* block);
    void ReplaceBlockWith(BSTBlock* block, BSTBlock* replaceWith);
    
    inline void DeleteAndReleaseBlock(BSTBlock* block) 
    {
        DeleteBlock(block);
        ReleaseBlock(block);
    }

    BSTBlock* GetFirst();
    BSTBlock* GetPredecessor(BSTBlock* block);
    BSTBlock* GetSuccessor(BSTBlock* block);

    BSTBlock *m_Root;

    uint64_t m_TotalCapacity;
    uint64_t m_UsedBlocks;

    BSTBlockPool m_FirstPool;
    BSTBlock m_StaticBlockPool[STATIC_POOL_SIZE];
    BSTBlockPool* m_CurrentPool;
    uint32_t m_CurrentPoolNextFree;
};
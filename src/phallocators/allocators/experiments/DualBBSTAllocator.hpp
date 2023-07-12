#include "../Allocator.hpp"
#include <map>

#define STATIC_POOL_SIZE 256

struct DualBBSTBlock
{
    uint64_t Base;
    uint64_t Size;
    RegionType Type;

    DualBBSTBlock(uint64_t base,
              uint64_t size,
              RegionType type);
};

/**
 * Balanced Binary Search Tree allocator
 * (not suitable for an OS because it uses std::multimap)
 */
class DualBBSTAllocator : public Allocator
{
public:
    DualBBSTAllocator();
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(void* base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;
    uint64_t MeasureWastedMemory() override;

protected:
    bool InitializeImpl(RegionBlocks regions[], size_t regionCount) override;
    void DumpImpl(JsonWriter& writer) override;

private:
    std::multimap<uint64_t, DualBBSTBlock> m_FreeMap;
    std::multimap<uint64_t, DualBBSTBlock> m_ReservedMap;
};
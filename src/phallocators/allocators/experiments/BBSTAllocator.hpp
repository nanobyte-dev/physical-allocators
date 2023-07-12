#include "../Allocator.hpp"
#include <map>

struct BBSTRegion
{
    uint64_t Base;
    uint64_t Size;
    RegionType Type;

    BBSTRegion(uint64_t base,
               uint64_t size,
               RegionType type);
};

/**
 * Self Balancing Binary Search Tree allocator
 * - implemented using std::map because it uses black-red trees,
 * and because implementing black-red trees is hard
 * 
 * Note: not suitable for an OS because it uses std::map
 */
class BBSTAllocator : public Allocator
{
public:
    BBSTAllocator();
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(void* base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;
    uint64_t MeasureWastedMemory() override;

protected:
    bool InitializeImpl(RegionBlocks regions[], size_t regionCount) override;
    void DumpImpl(JsonWriter& writer) override;

private:
    std::map<uint64_t, BBSTRegion> m_Map;
};
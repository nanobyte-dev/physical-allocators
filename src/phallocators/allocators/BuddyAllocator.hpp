#include "Allocator.hpp"
#include "../math/MathHelpers.hpp"

#define LAYER_COUNT 10
#define BIG_BLOCK_MULTIPLIER (1ull << (LAYER_COUNT - 1))

class BuddyAllocator : public Allocator
{
public:
    BuddyAllocator();
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(ptr_t base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;

protected:
    bool InitializeImpl(RegionBlocks regions[], size_t regionCount) override;
    void DumpImpl(JsonWriter& writer) override;

private:
    uint64_t FindFreeBlock(int& layer);
    void MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed);
    void MarkBlocks(uint64_t block, size_t count, bool isUsed);

    inline uint64_t BlocksOnLayer(int layer)
    {
        return (1ull << layer) * m_BlocksLayer0;
    }

    inline uint64_t IndexOfLayer(int layer)
    {
        return DivRoundUp(m_BlocksLayer0, BitmapUnit) * ((1ull << layer) - 1);
    }

    inline int GetNearestLayer(int blocks)
    {
        return LAYER_COUNT - 1 - Log2(blocks) - (IsPowerOf2(blocks) ? 0 : 1);
    }

    inline bool Get(int layer, uint64_t block)
    {
        uint64_t addr = IndexOfLayer(layer) + block / BitmapUnit;
        uint64_t offset = block % BitmapUnit;
        return (m_Bitmap[addr] & (1 << offset)) != 0;
    }

    inline void Set(int layer, uint64_t block, bool value)
    {
        uint64_t addr = IndexOfLayer(layer) + block / BitmapUnit;
        uint64_t offset = block % BitmapUnit;
        if (value)
            m_Bitmap[addr] |= (1 << offset);
        else
            m_Bitmap[addr] &= ~(1 << offset);
    }

    void SetBulk(int layer, uint64_t blockStart, uint64_t count, bool value);

    uint64_t m_SmallBlockSize;
    uint64_t m_BigBlockSize;
    uint8_t* m_Bitmap;
    uint64_t m_BitmapSize;
    uint64_t m_BlocksLayer0;
    static constexpr size_t BitmapUnit = sizeof(m_Bitmap[0]) * 8;

    uint64_t m_LastAllocatedBlock;
    uint64_t m_LastAllocatedCount;
    int m_LastAllocatedLayer;

};
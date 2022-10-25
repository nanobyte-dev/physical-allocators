#include "Allocator.hpp"

class BuddyAllocator : public Allocator
{
public:
    BuddyAllocator();
    bool Initialize(uint64_t blockSize, Region regions[], size_t regionCount) override;
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(ptr_t base, uint32_t blocks) override;
    
    // for statistics
    void GetRegions(std::vector<Region>& regions) override;

    // for debugging
    void Dump() override;

private:
    void MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed);
    void MarkBlocks(uint64_t block, size_t count, bool isUsed);
    void BubbleUp(int layerStart, uint64_t block, size_t count);
    void CheckAndMergeBlocks();
    ptr_t Align(ptr_t addr);

    inline uint64_t BlocksOnLayer(int layer)
    {
        return m_MemSize / (1 << layer);
    }

    inline uint64_t IndexOfLayer(int layer)
    {
        if (layer == 0)
            return 0;

        return m_BitmapFirstLayerSize * ((1 << layer) - 1) / (1 << (layer - 1));
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

    uint64_t m_BlockSize;
    uint8_t* m_MemBase;
    uint64_t m_MemSize;
    uint8_t* m_Bitmap;
    uint64_t m_BitmapFirstLayerSize;
    uint64_t m_BitmapSize;
    uint64_t m_Next; // for next_fit strategy
    static constexpr size_t BitmapUnit = sizeof(m_Bitmap[0]) * 8;

};
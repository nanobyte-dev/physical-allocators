#include "Allocator.hpp"

#define FIRST_FIT 1
#define NEXT_FIT 2
#define BEST_FIT 3
#define WORST_FIT 4

#define BITMAP_ALLOCATOR_STRATEGY FIRST_FIT
#define BITMAP_STATIC_SIZE 1024


class BitmapAllocator : public Allocator
{
public:
    BitmapAllocator();
    bool Initialize(uint64_t blockSize, Region regions[], size_t regionCount) override;
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(ptr_t base, uint32_t blocks) override;
    
    // for statistics
    size_t GetRegionCount() override;
    void GetRegion(size_t index, Region& regionOut) override;

    // for debugging
    void Dump() override;

private:
    void MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed);
    void MarkBlocks(uint64_t base, size_t size, bool isUsed);
    ptr_t Align(ptr_t addr);
    void CheckAndMergeBlocks();
    ptr_t AllocateSmall(uint32_t blocks = 1);

    inline bool Get(uint64_t block)
    {
        uint64_t addr = block / BitmapUnit;
        uint64_t offset = block % BitmapUnit;
        return (m_Bitmap[addr] & (1 << offset)) != 0;
    }

    inline void Set(uint64_t block, bool value)
    {
        uint64_t addr = block / BitmapUnit;
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

    template<typename TPtr>
    inline TPtr ToPtr(uint64_t block)
    {
        uint8_t* u8Ptr = m_MemBase + block * m_BlockSize;
        return reinterpret_cast<TPtr>(u8Ptr);
    }

    uint64_t m_BlockSize;
    uint8_t* m_MemBase;
    uint64_t m_MemSize;
    uint8_t* m_Bitmap;
    uint64_t m_BitmapSize;
    static constexpr size_t BitmapUnit = sizeof(m_Bitmap[0]);
};
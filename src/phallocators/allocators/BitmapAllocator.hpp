#include "Allocator.hpp"

class BitmapAllocator : public Allocator
{
public:
    BitmapAllocator();
    bool Initialize(uint64_t blockSize, Region regions[], size_t regionCount) override;
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(ptr_t base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;

    // for debugging
    void Dump() override;

protected:
    virtual uint64_t FindFreeRegion(uint32_t blocks) = 0;

    void MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed);
    void MarkBlocks(uint64_t base, size_t size, bool isUsed);
    void CheckAndMergeBlocks();
    ptr_t Align(ptr_t addr);

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

    inline ptr_t ToPtr(uint64_t block)
    {
        uint8_t* u8Ptr = m_MemBase + block * m_BlockSize;
        return reinterpret_cast<ptr_t>(u8Ptr);
    }

    uint64_t m_BlockSize;
    uint8_t* m_MemBase;
    uint64_t m_MemSize;
    uint8_t* m_Bitmap;
    uint64_t m_BitmapSize;
    static constexpr size_t BitmapUnit = sizeof(m_Bitmap[0]) * 8;
};


class BitmapAllocatorFirstFit : public BitmapAllocator
{
protected:
    uint64_t FindFreeRegion(uint32_t blocks) override;
};


class BitmapAllocatorNextFit : public BitmapAllocator
{
public:
    BitmapAllocatorNextFit();

protected:
    uint64_t FindFreeRegion(uint32_t blocks) override;

private:
    uint64_t m_Next;
};


class BitmapAllocatorBestFit : public BitmapAllocator
{
protected:
    uint64_t FindFreeRegion(uint32_t blocks) override;
};


class BitmapAllocatorWorstFit : public BitmapAllocator
{
protected:
    uint64_t FindFreeRegion(uint32_t blocks) override;
};
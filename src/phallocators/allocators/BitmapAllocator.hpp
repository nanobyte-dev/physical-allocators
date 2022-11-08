#include "Allocator.hpp"

class BitmapAllocator : public Allocator
{
public:
    BitmapAllocator();
    ptr_t Allocate(uint32_t blocks = 1) override;
    void Free(ptr_t base, uint32_t blocks) override;
    
    // for statistics
    RegionType GetState(ptr_t address) override;

protected:
    bool InitializeImpl(RegionBlocks regions[], size_t regionCount) override;
    void DumpImpl(JsonWriter& writer) override;

    virtual uint64_t FindFreeRegion(uint32_t blocks) = 0;

    void MarkRegion(ptr_t basePtr, size_t sizeBytes, bool isUsed);
    void MarkBlocks(uint64_t base, size_t size, bool isUsed);

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
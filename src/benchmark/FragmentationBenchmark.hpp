#include <phallocators/allocators/Allocator.hpp>
#include <Config.hpp>
#include <Utils.hpp>
#include <cstdint>
#include <random>
#include <iostream>
#include <cassert>
#include <memory>

#define MAX_USED_REGIONS 5000
#define ITERATIONS 2000
#define MEASURE_ITERATIONS 100

template<typename TAllocator>
class FragmentationBenchmark
{
public:
    FragmentationBenchmark(int seed)
        : m_Seed(seed),
          m_BasePtr(new uint8_t[MEM_SIZE]),
          m_Generator(seed),
          m_RandPercent(0, 100),
          m_RandSize(0.05),
          m_Allocator(),
          m_AllocatedRegions(),
          m_FreeBlocks(0),
          m_TotalBlocks(MEM_SIZE / BLOCK_SIZE)
    {
    }

    virtual void Setup()
    {
        // reset RNGs
        m_Generator.seed(m_Seed);
        m_RandPercent.reset();
        m_RandSize.reset();

        // setup allocator
        m_Allocator = std::make_unique<TAllocator>();
        InitializeAllocator();
        m_AllocatedRegions.clear();
    }

    double Run()
    {
        double totalFragmentation = 0.0;

        for (int i = 0; i < ITERATIONS; i++)
        {
            bool alloc;

            if (m_AllocatedRegions.empty())
                alloc = true;
            else if (m_FreeBlocks == 0)
                alloc = false;
            else
                alloc = (m_RandPercent(m_Generator) < 50) == 0;

            if (alloc)
                AllocRandomBlock();
            else
                FreeRandomBlock();


            if (i % MEASURE_ITERATIONS == 0)
                totalFragmentation += MeasureFragmentation();
        }

        return totalFragmentation / static_cast<double>(ITERATIONS / MEASURE_ITERATIONS);
    }

protected:

    void InitializeAllocator()
    {
        uint8_t* basePtr = m_BasePtr.get();

        Region regions[] = 
        {
            { basePtr + 0x00000000, 0x00000500, RegionType::Reserved },
            { basePtr + 0x00000500, 0x0007FB00, RegionType::Free     },
            { basePtr + 0x00080000, 0x00070000, RegionType::Reserved },
            { basePtr + 0x000F0000, 0x00010000, RegionType::Reserved },
            { basePtr + 0x00100000, MEM_SIZE - 0x00100000, RegionType::Free },
        };

        assert(m_Allocator->Initialize(BLOCK_SIZE, regions, ArraySize(regions)));

        RecalculateFreeBlocks();
    }

    void RecalculateFreeBlocks()
    {
        uint8_t* basePtr = m_BasePtr.get();
        
        m_FreeBlocks = 0;
        for (uint64_t i = 0; i < MEM_SIZE; i += BLOCK_SIZE)
            if (m_Allocator->GetState(basePtr + i) == RegionType::Free)
                ++m_FreeBlocks;
    }

    bool AllocRandomBlock(size_t maxSize = 0xFFFFFFFF)
    {
        size_t size = RandomSize(maxSize);

        // try to allocate
        ptr_t base = m_Allocator->Allocate(size);
        if (base != nullptr)
        {
            Region r = { base, size, RegionType::Reserved };
            m_AllocatedRegions.push_back(r);
            m_FreeBlocks -= size;
            return true;
        }
        else return false;
    }

    void FreeRandomBlock()
    {
        if (m_AllocatedRegions.size() == 0)
            return;

        std::uniform_int_distribution<int> randRegion(0, m_AllocatedRegions.size() - 1);
        int reg = randRegion(m_Generator);

        m_Allocator->Free(m_AllocatedRegions[reg].Base, m_AllocatedRegions[reg].Size);
        m_FreeBlocks += m_AllocatedRegions[reg].Size;

        m_AllocatedRegions[reg] = m_AllocatedRegions[m_AllocatedRegions.size() - 1];
        m_AllocatedRegions.pop_back();
    }

    size_t RandomSize(size_t maxSize = 0xFFFFFFFF)
    {
        if (m_RandPercent(m_Generator) < BLOCK_SIZE_1_RATIO)
            return 1;
        else
        {
            size_t size;
            do {
                size = m_RandSize(m_Generator);
            } while (size == 0 || size > m_FreeBlocks || size > maxSize);
            return size;
        }
    }

    double MeasureFragmentation()
    {
        uint64_t freeBlocks;
        auto regionSizes = GetFreeRegionSizes(&freeBlocks);
        
        double qualityTotal = 0;

        // See https://stackoverflow.com/a/74525096/794075
        for (uint64_t i = 1; i <= freeBlocks; i++)
        {
            uint64_t freeSlots = 0;
            for (uint64_t regionSize : regionSizes)
                freeSlots += regionSize / i;

            uint64_t idealFreeSlots = freeBlocks / i;

            qualityTotal += static_cast<double>(freeSlots) / static_cast<double>(idealFreeSlots);
        }

        return 1.0 - qualityTotal / (static_cast<double>(freeBlocks));
    }

    std::vector<uint64_t> GetFreeRegionSizes(uint64_t* totalFreeBlocks = nullptr)
    {
        std::vector<uint64_t> freeRegionSizes;
        uint64_t freeBlocks = 0;

        uint64_t regionStart = 0;
        bool reset = true;
        
        // not the nicest implementation, we just iterate over the whole memory space :(
        for (uint64_t i = 0; i < MEM_SIZE; i += BLOCK_SIZE)
        {
            if (m_Allocator->GetState(m_BasePtr.get() + i) == RegionType::Free)
            {
                if (reset)
                {
                    regionStart = i;
                    reset = false;
                }
            }
            else
            {
                if (!reset)
                {
                    uint64_t size = (i - regionStart) / BLOCK_SIZE;
                    freeBlocks += size;
                    freeRegionSizes.push_back(size);
                }
                reset = true;
            }
        }

        if (!reset)
        {
            uint64_t size = (MEM_SIZE - regionStart) / BLOCK_SIZE;
            freeBlocks += size;
            freeRegionSizes.push_back(size);
        }

        if (totalFreeBlocks)
            *totalFreeBlocks = freeBlocks;

        return freeRegionSizes;
    }

    // global state
    int m_Seed;
    std::unique_ptr<uint8_t[]> m_BasePtr;

    // test state
    std::mt19937 m_Generator;
    std::uniform_int_distribution<int> m_RandPercent;
    std::geometric_distribution<size_t> m_RandSize;
    std::unique_ptr<TAllocator> m_Allocator;
    std::vector<Region> m_AllocatedRegions;
    size_t m_FreeBlocks;
    const size_t m_TotalBlocks;
};
#include <phallocators/allocators/Allocator.hpp>
#include <Config.hpp>
#include <Utils.hpp>
#include <cstdint>
#include <random>
#include <iostream>
#include <cassert>
#include <memory>
#include "Clock.hpp"

#define MAX_USED_REGIONS 5000
#define INIT_ITERATIONS 100
#define TEST_ITERATIONS 100

template<typename TAllocator>
class AllocatorBenchmark
{
public:
    AllocatorBenchmark(int seed)
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

        SimulateUsage();
    }

    virtual double Run() = 0;

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

    void SimulateUsage()
    {
        for (int i = 0; i < INIT_ITERATIONS; i++)
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
        }
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


template<typename TAllocator>
class AllocatorBenchmark_Alloc : public AllocatorBenchmark<TAllocator>
{
typedef AllocatorBenchmark<TAllocator> Base;

public:
    AllocatorBenchmark_Alloc(int seed, int memUsageTarget)
        : Base(seed),
          m_MemUsageTarget(memUsageTarget)
    {
    }

    double Run() override
    {
        Clock clock;
        int failedAllocations = 0;

        for (int i = 0; i < TEST_ITERATIONS; i++)
        {
            EnforceUsageTarget();

            size_t size = this->RandomSize();
            ptr_t base;

            {
                clock.Start();
                base = this->m_Allocator->Allocate(size);
                clock.Stop();
            }
            
            if (base != nullptr)
            {
                Region r = { base, size, RegionType::Reserved };
                this->m_AllocatedRegions.push_back(r);
                this->m_FreeBlocks -= size;
            }

            else failedAllocations++;
        }

        return clock.ElapsedSeconds();
    }

private:

    void EnforceUsageTarget()
    {
        size_t targetFree = (100 - m_MemUsageTarget) * this->m_TotalBlocks / 100;

        // free up excess memory
        while (this->m_FreeBlocks < targetFree)
            this->FreeRandomBlock();

        // allocate extra memory
        int failures = 0;
        while (this->m_FreeBlocks > targetFree)
        {
            size_t maxSize = (this->m_FreeBlocks - targetFree);
            if (!this->AllocRandomBlock(maxSize))
                ++failures;

            // not enough memory? maybe our "free blocks" is not up to date
            if (failures > 5)
            {
                this->RecalculateFreeBlocks();
                return EnforceUsageTarget();
            }
        }
    }


    int m_MemUsageTarget;
};




template<typename TAllocator>
class AllocatorBenchmark_Free : public AllocatorBenchmark<TAllocator>
{
typedef AllocatorBenchmark<TAllocator> Base;

public:
    AllocatorBenchmark_Free(int seed)
        : Base(seed)
    {
    }

    double Run() override
    {
        Clock clock;
        int failedAllocations = 0;

        for (int i = 0; i < TEST_ITERATIONS; i++)
        {
            EnsureBlocksAllocated();

            std::uniform_int_distribution<int> randRegion(0, this->m_AllocatedRegions.size() - 1);
            int reg = randRegion(this->m_Generator);

            {
                clock.Start();
                this->m_Allocator->Free(this->m_AllocatedRegions[reg].Base, this->m_AllocatedRegions[reg].Size);
                clock.Stop();
            }

            this->m_FreeBlocks -= this->m_AllocatedRegions[reg].Size;

            this->m_AllocatedRegions[reg] = this->m_AllocatedRegions[this->m_AllocatedRegions.size() - 1];
            this->m_AllocatedRegions.pop_back();
        }

        return clock.ElapsedSeconds();
    }

private:

    void EnsureBlocksAllocated()
    {
        if (this->m_AllocatedRegions.empty())
        {
            for (int i = 0; i < 1 + this->m_RandPercent(this->m_Generator); i++)
                this->AllocRandomBlock();
        }
    }
};

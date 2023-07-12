#include <iostream>

#include <phallocators/allocators/BitmapAllocator.hpp>
#include <phallocators/allocators/BuddyAllocator.hpp>
#include <phallocators/allocators/LinkedListAllocator.hpp>
#include <phallocators/allocators/experiments/BSTAllocator.hpp>
#include <phallocators/allocators/experiments/BBSTAllocator.hpp>
#include <phallocators/allocators/experiments/DualBBSTAllocator.hpp>
#include "SpeedBenchmarks.hpp"
#include "FragmentationAndWasteBenchmark.hpp"
#include <algorithm>
#include <iomanip>
#include <thread>
#include <future>

#define ITERATIONS 240

double ToMicroseconds(double seconds)
{
    return seconds * 1000000.0;
}

template<typename TBenchmark, typename... Args>
void DoSpeedBenchmark(Args... args)
{
    std::vector<double> times(ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++)
    {
        TBenchmark bench(i + SRAND, args...);
        bench.Setup();
        times[i] = ToMicroseconds(bench.Run());
    }

    // calculate avg
    double avg = std::reduce(times.begin(), times.end()) / static_cast<double>(ITERATIONS);

    // 95th percentiles
    std::sort(times.begin(), times.end());
    double low_95th = times[ITERATIONS * 95 / 100];
    double high_95th = times[ITERATIONS - (ITERATIONS * 95 / 100)];

    // standard deviation
    double std_dev = 0;
    for (double time : times)
        std_dev += (time - avg) * (time - avg);
    std_dev = sqrt(std_dev / static_cast<double>(ITERATIONS));

    // print results
    std::cout.precision(8);

    std::cout << typeid(TBenchmark).name() << "|";
    ((std::cout << args << " "), ...);
    std::cout << ";";
    std::cout << MEM_SIZE << ";";
    std::cout << BLOCK_SIZE_1_RATIO << ";";
    std::cout << avg << ";";
    std::cout << low_95th << ";";
    std::cout << high_95th << ";";
    std::cout << std_dev << std::endl;
}

template <typename TAllocator>
void DoSpeedBenchmarks()
{
    DoSpeedBenchmark<AllocatorBenchmark_Alloc<TAllocator>>(25);
    DoSpeedBenchmark<AllocatorBenchmark_Alloc<TAllocator>>(50);
    DoSpeedBenchmark<AllocatorBenchmark_Alloc<TAllocator>>(75);
    DoSpeedBenchmark<AllocatorBenchmark_Free<TAllocator>>();
}

void SpeedBenchmarks()
{
    // print header
    // std::cout<<"Speed benchmark;";
    // std::cout<<"Average (us);";
    // std::cout<<"Low 95th (us);";
    // std::cout<<"High 95th (us);";
    // std::cout<<"Std dev (us)" << std::endl;

    DoSpeedBenchmarks<BitmapAllocatorFirstFit>();
    DoSpeedBenchmarks<BitmapAllocatorNextFit>();
    DoSpeedBenchmarks<BitmapAllocatorBestFit>();
    DoSpeedBenchmarks<BitmapAllocatorWorstFit>();
    DoSpeedBenchmarks<BuddyAllocator>();
    DoSpeedBenchmarks<LinkedListAllocatorFirstFit>();
    DoSpeedBenchmarks<LinkedListAllocatorNextFit>();
    DoSpeedBenchmarks<LinkedListAllocatorBestFit>();
    DoSpeedBenchmarks<LinkedListAllocatorWorstFit>();
    DoSpeedBenchmarks<BSTAllocator>();
    DoSpeedBenchmarks<BBSTAllocator>();
    DoSpeedBenchmarks<DualBBSTAllocator>();
}



template<typename TAllocator>
void DoFragmentationAndWasteBenchmark()
{
    std::vector<double> frags(ITERATIONS);
    std::vector<double> wastes(ITERATIONS);

    for (int i = 0; i < ITERATIONS / THREADS; i++)
    {
        std::future<std::pair<double, double>> futures[THREADS];
        for (int t = 0; t < THREADS; t++) 
        {
            futures[t] = std::async([=]() 
            {
                FragmentationAndWasteBenchmark<TAllocator> bench(i * THREADS + t + SRAND);
                bench.Setup();
                return bench.Run();
            });
        }

        for (int t = 0; t < THREADS; t++) 
        {
            auto result = futures[t].get();
            frags[i * THREADS + t] = result.first;
            wastes[i * THREADS + t] = result.second;
        }
    }

    // calculate avg
    double frags_avg = std::reduce(frags.begin(), frags.end()) / static_cast<double>(ITERATIONS);
    double wastes_avg = std::reduce(wastes.begin(), wastes.end()) / static_cast<double>(ITERATIONS);

    // 95th percentiles
    std::sort(frags.begin(), frags.end());
    double frags_low_95th = frags[ITERATIONS * 95 / 100];
    double frags_high_95th = frags[ITERATIONS - (ITERATIONS * 95 / 100)];

    std::sort(wastes.begin(), wastes.end());
    double wastes_low_95th = wastes[ITERATIONS * 95 / 100];
    double wastes_high_95th = wastes[ITERATIONS - (ITERATIONS * 95 / 100)];

    // standard deviation
    double frags_std_dev = 0;
    for (double frag : frags)
        frags_std_dev += (frag - frags_avg) * (frag - frags_avg);
    frags_std_dev = sqrt(frags_std_dev / static_cast<double>(ITERATIONS));

    double wastes_std_dev = 0;
    for (double waste : wastes)
        wastes_std_dev += (waste - wastes_avg) * (waste - wastes_avg);
    wastes_std_dev = sqrt(wastes_std_dev / static_cast<double>(ITERATIONS));

    // print results
    std::cout.precision(8);

    std::cout << typeid(TAllocator).name() << ";";
    std::cout << MEM_SIZE << ";";
    std::cout << BLOCK_SIZE_1_RATIO << ";";

    std::cout << frags_avg << ";";
    std::cout << frags_low_95th << ";";
    std::cout << frags_high_95th << ";";
    std::cout << frags_std_dev << ";";

    std::cout << wastes_avg << ";";
    std::cout << wastes_low_95th << ";";
    std::cout << wastes_high_95th << ";";
    std::cout << wastes_std_dev << std::endl;
}

void FragmentationAndWasteBenchmarks()
{
    // print header
    // std::cout<<"Fragmentation benchmark;";
    // std::cout<<"Average (us);";
    // std::cout<<"Low 95th (us);";
    // std::cout<<"High 95th (us);";
    // std::cout<<"Std dev (us)" << std::endl;

    DoFragmentationAndWasteBenchmark<BitmapAllocatorFirstFit>();
    DoFragmentationAndWasteBenchmark<BitmapAllocatorNextFit>();
    DoFragmentationAndWasteBenchmark<BitmapAllocatorBestFit>();
    DoFragmentationAndWasteBenchmark<BitmapAllocatorWorstFit>();
    DoFragmentationAndWasteBenchmark<BuddyAllocator>();
    DoFragmentationAndWasteBenchmark<LinkedListAllocatorFirstFit>();
    DoFragmentationAndWasteBenchmark<LinkedListAllocatorNextFit>();
    DoFragmentationAndWasteBenchmark<LinkedListAllocatorBestFit>();
    DoFragmentationAndWasteBenchmark<LinkedListAllocatorWorstFit>();
    DoFragmentationAndWasteBenchmark<BSTAllocator>();
    DoFragmentationAndWasteBenchmark<BBSTAllocator>();
    DoFragmentationAndWasteBenchmark<DualBBSTAllocator>();
}

int main()
{
    //SpeedBenchmarks();
    FragmentationAndWasteBenchmarks();
}

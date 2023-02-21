#include <iostream>

#include <phallocators/allocators/BitmapAllocator.hpp>
#include <phallocators/allocators/BuddyAllocator.hpp>
#include <phallocators/allocators/LinkedListAllocator.hpp>
#include <phallocators/allocators/experiments/BSTAllocator.hpp>
#include <phallocators/allocators/experiments/BBSTAllocator.hpp>
#include <phallocators/allocators/experiments/DualBBSTAllocator.hpp>
#include "SpeedBenchmarks.hpp"
#include "FragmentationBenchmark.hpp"
#include <algorithm>
#include <iomanip>

#define ITERATIONS 10000

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

    std::cout << typeid(TBenchmark).name() << " - ";
    ((std::cout << args << " "), ...);
    std::cout << ";";

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
    std::cout<<"Speed benchmark;";
    std::cout<<"Average (us);";
    std::cout<<"Low 95th (us);";
    std::cout<<"High 95th (us);";
    std::cout<<"Std dev (us)" << std::endl;

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
void DoFragmentationBenchmark()
{
    std::vector<double> frags(ITERATIONS);

    for (int i = 0; i < ITERATIONS; i++)
    {
        FragmentationBenchmark<TAllocator> bench(i + SRAND);
        bench.Setup();
        frags[i] = bench.Run();
    }

    // calculate avg
    double avg = std::reduce(frags.begin(), frags.end()) / static_cast<double>(ITERATIONS);

    // 95th percentiles
    std::sort(frags.begin(), frags.end());
    double low_95th = frags[ITERATIONS * 95 / 100];
    double high_95th = frags[ITERATIONS - (ITERATIONS * 95 / 100)];

    // standard deviation
    double std_dev = 0;
    for (double frag : frags)
        std_dev += (frag - avg) * (frag - avg);
    std_dev = sqrt(std_dev / static_cast<double>(ITERATIONS));

    // print results
    std::cout.precision(8);

    std::cout << typeid(TAllocator).name() << ";";
    std::cout << avg << ";";
    std::cout << low_95th << ";";
    std::cout << high_95th << ";";
    std::cout << std_dev << std::endl;
}

void FragmentationBenchmarks()
{
    // print header
    std::cout<<"Fragmentation benchmark;";
    std::cout<<"Average (us);";
    std::cout<<"Low 95th (us);";
    std::cout<<"High 95th (us);";
    std::cout<<"Std dev (us)" << std::endl;

    DoFragmentationBenchmark<BitmapAllocatorFirstFit>();
    DoFragmentationBenchmark<BitmapAllocatorNextFit>();
    DoFragmentationBenchmark<BitmapAllocatorBestFit>();
    DoFragmentationBenchmark<BitmapAllocatorWorstFit>();
    DoFragmentationBenchmark<BuddyAllocator>();
    DoFragmentationBenchmark<LinkedListAllocatorFirstFit>();
    DoFragmentationBenchmark<LinkedListAllocatorNextFit>();
    DoFragmentationBenchmark<LinkedListAllocatorBestFit>();
    DoFragmentationBenchmark<LinkedListAllocatorWorstFit>();
    DoFragmentationBenchmark<BSTAllocator>();
    DoFragmentationBenchmark<BBSTAllocator>();
    DoFragmentationBenchmark<DualBBSTAllocator>();
}

int main()
{
    SpeedBenchmarks();
    // FragmentationBenchmarks();
}

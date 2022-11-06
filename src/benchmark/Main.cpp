#include <iostream>

#include <phallocators/allocators/BitmapAllocator.hpp>
#include <phallocators/allocators/BuddyAllocator.hpp>
#include <phallocators/allocators/LinkedListAllocator.hpp>

#include "SpeedBenchmarks.hpp"
#include <algorithm>
#include <iomanip>

#define ITERATIONS 1000

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
}

int main()
{
    SpeedBenchmarks();
}

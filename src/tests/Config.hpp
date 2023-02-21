#define MEM_SIZE                (32 * 1024 * 1024)
#define BLOCK_SIZE              4096

#define SRAND                   123456



#include <phallocators/allocators/BitmapAllocator.hpp>
#include <phallocators/allocators/BuddyAllocator.hpp>
#include <phallocators/allocators/LinkedListAllocator.hpp>
#include <phallocators/allocators/experiments/BSTAllocator.hpp>
#include <phallocators/allocators/experiments/BBSTAllocator.hpp>
#include <phallocators/allocators/experiments/DualBBSTAllocator.hpp>

#define ALL_ALLOCATORS          BitmapAllocatorFirstFit,        \
                                BitmapAllocatorNextFit,         \
                                BitmapAllocatorBestFit,         \
                                BitmapAllocatorWorstFit,        \
                                BuddyAllocator,                 \
                                LinkedListAllocatorFirstFit,    \
                                LinkedListAllocatorNextFit,     \
                                LinkedListAllocatorBestFit,     \
                                LinkedListAllocatorWorstFit,    \
                                BSTAllocator,                   \
                                BBSTAllocator,                  \
                                DualBBSTAllocator

// #undef ALL_ALLOCATORS
// #define ALL_ALLOCATORS BuddyAllocator
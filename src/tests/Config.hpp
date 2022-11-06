//#define ALLOCATOR               BitmapAllocator
//#define ALLOCATOR               BuddyAllocator
#define ALLOCATOR               LinkedListAllocator
#define STRATEGY                STRATEGY_FIRST_FIT

#define MEM_SIZE                (32 * 1024 * 1024)
#define BLOCK_SIZE              4096

#define SRAND                   123456



#include <phallocators/allocators/BitmapAllocator.hpp>
#include <phallocators/allocators/BuddyAllocator.hpp>
#include <phallocators/allocators/LinkedListAllocator.hpp>

#define ALL_ALLOCATORS          BitmapAllocatorFirstFit,        \
                                BitmapAllocatorNextFit,         \
                                BitmapAllocatorBestFit,         \
                                BitmapAllocatorWorstFit,        \
                                BuddyAllocator,                 \
                                LinkedListAllocatorFirstFit,    \
                                LinkedListAllocatorNextFit,     \
                                LinkedListAllocatorBestFit,     \
                                LinkedListAllocatorWorstFit

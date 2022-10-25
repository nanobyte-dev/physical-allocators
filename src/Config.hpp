#pragma once

// defs

#define DEBUG_LEVEL_DEBUG       0
#define DEBUG_LEVEL_INFO        1
#define DEBUG_LEVEL_WARN        2
#define DEBUG_LEVEL_ERROR       3
#define DEBUG_LEVEL_CRITICAL    4

#define STRATEGY_FIRST_FIT      0
#define STRATEGY_NEXT_FIT       1
#define STRATEGY_BEST_FIT       2
#define STRATEGY_WORST_FIT      3

// config begins here

#define DEBUG_LEVEL             DEBUG_LEVEL_INFO

#define ALLOCATOR               BuddyAllocator
#define STRATEGY                STRATEGY_FIRST_FIT

#define MEM_SIZE                (32 * 1024 * 1024)
#define BLOCK_SIZE              4096

#define SRAND                   123456
#pragma once

#include <limits.h>

typedef unsigned char bitset_t;

#define BITMASK(_bit)           (1 << ((_bit) % CHAR_BIT))
#define BITSLOT(_bit)           ((_bit) / CHAR_BIT)
#define BITSET(_bitset, _bit)   ((_bitset)[BITSLOT(_bit)] |= BITMASK(_bit))
#define BITCLEAR(_bitset, _bit) (_bitset[BITSLOT(_bit)] &= ~BITMASK(_bit))
#define BITTEST(_bitset, _bit)  ((_bitset)[BITSLOT(_bit)] & BITMASK(_bit))
#define BITNSIZE(_bitset)       (sizeof(_bitset) * CHAR_BIT)
#define BITNSLOTS(_bitsize)     ((_bitsize + CHAR_BIT - 1) / CHAR_BIT)

// so I can make this SIMD by doing four comparisons at once
#define DEFINE_BITSET_N(_size) typedef unsigned char bitset##_size##_t __attribute__((vector_size(sizeof(bitset_t) * BITNSLOTS(_size))));
DEFINE_BITSET_N(4);
DEFINE_BITSET_N(8);
DEFINE_BITSET_N(16);
DEFINE_BITSET_N(64);
DEFINE_BITSET_N(128);
DEFINE_BITSET_N(256);
DEFINE_BITSET_N(512);
// 512 size bitset is 1 cache line.
static_assert(sizeof(bitset512_t) == 64, "");

#define DECL_CONCAT(a, b, c) a##b##c
#define BITSET_DECL(_, n) DECL_CONCAT(bitset, n, _t) _

#ifdef MID_IDE_ANALYSIS
#define TRAILING_ONES(_) 0
#elif __clang__
#define TRAILING_ONES(_) ((int)__builtin_ctz(~_))
#elif __GNUC__
#define TRAILING_ONES(_) ((int)__builtin_stdc_trailing_ones(_))
#endif

#ifdef MID_IDE_ANALYSIS
#define BIT_COUNT_ONES(_) 0
#elif __clang__
#define BIT_COUNT_ONES(_) static_assert(false, "BIT_COUNT_ONES not implemented in clang!")
#elif __GNUC__
#define BIT_COUNT_ONES(_) __builtin_popcount(_);
#endif

static inline int BitScanFirstZero(int setCount, bitset_t* pSet)
{
	int i = 0;
	for (; i < setCount; ++i)
		if (pSet[i] != 0xFF) break;
	return i == setCount ? -1 : TRAILING_ONES(pSet[i]) + (i * CHAR_BIT);
}

static inline int BitCountOnes(int setCount, bitset_t* pSet)
{
	int count = 0;
	for (int i = 0; i < setCount; ++i)
		count += BIT_COUNT_ONES(pSet[i]);
	return count;
}

//typedef unsigned char nibset_t;
//#define NIBSET_DECL
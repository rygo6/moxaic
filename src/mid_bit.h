#pragma once

#include <limits.h>

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)
#define BITNSIZE(a) (sizeof(a) * CHAR_BIT)

typedef unsigned char bitset_t;

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

static inline int BitScanFirstZero(int setCount, bitset_t* pSet)
{
	int i = 0;
	for (; i < setCount; ++i)
		if (pSet[i] != 0xFF)
			break;
	return i == setCount ? -1 : TRAILING_ONES(pSet[i]) + (i * CHAR_BIT);
}

//typedef unsigned char nibset_t;
//#define NIBSET_DECL
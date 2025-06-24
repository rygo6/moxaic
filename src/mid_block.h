#pragma once

#include "mid_common.h"
#include "mid_bit.h"

///////////
//// Handle
////
typedef u16 map_handle;
typedef map_handle mHnd;

typedef u16 block_handle;
typedef u32 block_key;

typedef block_handle bHnd;
typedef block_key    bKey;

#define HANDLE_INDEX_BIT_COUNT 12
#define HANDLE_INDEX_MASK      0x0FFF
#define HANDLE_INDEX_MAX       ((1u << HANDLE_INDEX_BIT_COUNT) - 1)

#define HANDLE_INDEX(handle)            (handle & HANDLE_INDEX_MASK)
#define HANDLE_INDEX_SET(handle, value) ((handle & HANDLE_GENERATION_MASK) | (value & HANDLE_INDEX_MASK))

#define HANDLE_GENERATION_BIT_COUNT 4
#define HANDLE_GENERATION_MASK      0xF000
#define HANDLE_GENERATION_MAX       ((1u << HANDLE_GENERATION_BIT_COUNT) - 1)

#define HANDLE_GENERATION(handle)            ((handle & HANDLE_GENERATION_MASK) >> HANDLE_INDEX_BIT_COUNT)
#define HANDLE_GENERATION_SET(handle, value) (value << HANDLE_INDEX_BIT_COUNT) | HANDLE_INDEX(handle)

// Generation 0 to signify it as invalid. Max index to make it assert errors if still used.
#define HANDLE_DEFAULT ((0 & HANDLE_GENERATION_MASK) | (UINT16_MAX & HANDLE_INDEX_MASK))

#define HANDLE_VALID(handle) (HANDLE_GENERATION(handle) != 0)

#define HANDLE_GENERATION_INCREMENT(handle) ({          \
	u8 g = HANDLE_GENERATION(handle);                   \
	g = g == HANDLE_GENERATION_MAX ? 1 : (g + 1) & 0xF; \
	HANDLE_GENERATION_SET(handle, g);                   \
})

#define HANDLE_CHECK(handle, error) \
	if (!HANDLE_VALID(handle)) {    \
		LOG_ERROR(#error "\n");     \
		return error;               \
	}

//////////
//// Block
////
#define BLOCK_DECL(type, n)       \
	struct {                      \
		BITSET_DECL(occupied, n); \
		block_key keys[n];        \
		type      blocks[n];      \
		u8        generations[n]; \
	}

static block_handle ClaimBlock(int occupiedCount, bitset_t* pOccupiedSet, block_key* pKeys, uint8_t* pGenerations, uint32_t key)
{
	int i = BitScanFirstZero(occupiedCount, pOccupiedSet);
	if (i == -1) return HANDLE_DEFAULT;
	BITSET(pOccupiedSet, i);
	pKeys[i] = key;
	pGenerations[i] = pGenerations[i] == HANDLE_GENERATION_MAX ? 1 : (pGenerations[i] + 1) & 0xF;
	return HANDLE_GENERATION_SET(i, pGenerations[i]);
}
static block_handle FindBlockByHash(int hashCount, block_key* pHashes, uint8_t* pGenerations, block_key hash)
{
	for (int i = 0; i < hashCount; ++i) {
		if (pHashes[i] == hash)
			return HANDLE_GENERATION_SET(i, pGenerations[i]);
	}
	return HANDLE_DEFAULT;
}

#define BLOCK_CLAIM(block, key)                                                                                           \
	({                                                                                                                    \
		int _handle = ClaimBlock(sizeof(block.occupied), (bitset_t*)&block.occupied, block.keys, block.generations, key); \
		assert(_handle >= 0 && #block ": Claiming handle. Out of capacity.");                                             \
		(block_handle) _handle;                                                                                           \
	})
#define BLOCK_RELEASE(block, handle)                                                                                                                       \
	({                                                                                                                                                     \
		assert(HANDLE_INDEX(handle) >= 0 && HANDLE_INDEX(handle) < COUNT(block.blocks) && #block ": Releasing block handle. Out of range."); \
		assert(BITSET(block.occupied, HANDLE_INDEX(handle)) && #block ": Releasing block handle. Should be occupied.");                                     \
		BITCLEAR(block.occupied, (int)HANDLE_INDEX(handle));                                                                                               \
	})
#define BLOCK_FIND(block, hash)                                                   \
	({                                                                            \
		assert(hash != 0);                                                        \
		FindBlockByHash(sizeof(block.keys), block.keys, block.generations, hash); \
	})
#define BLOCK_HANDLE(block, p)                                                                                                \
	({                                                                                                                        \
		assert(p >= block.blocks && p < block.blocks + COUNT(block.blocks) && #block ": Getting block handle. Out of range."); \
		HANDLE_GENERATION_SET((p - block.blocks), block.generations[(p - block.blocks)]);                                     \
	})
#define BLOCK_KEY(block, p)                                                                                                \
	({                                                                                                                     \
		assert(p >= block.blocks && p < block.blocks + COUNT(block.blocks) && #block ": Getting block key. Out of range."); \
		block.keys[p - block.blocks];                                                                                      \
	})
#define BLOCK_PTR(block, handle)                                                                                                       \
	({                                                                                                                                 \
		assert(HANDLE_INDEX(handle) >= 0 && HANDLE_INDEX(handle) < COUNT(block.blocks) && #block ": Getting block ptr. Out of range."); \
		&block.blocks[HANDLE_INDEX(handle)];                                                                                           \
	})


////////
//// Map
////
typedef struct MapBase {
	u32       count;
	block_key keys[];
	// keys could be any size
	//	block_handle handles[];
} MapBase;

#define MAP_DECL(n)              \
	struct {                     \
		u32          count;      \
		block_key    keys[n];    \
		block_handle handles[n]; \
	}

static inline block_handle* MapHandles(int capacity, MapBase* pMap)
{
	return (block_handle*)(pMap->keys + capacity);
}

// these could be implementation
static inline map_handle MapAdd(int capacity, MapBase* pMap, block_handle handle, block_key key)
{
	int i = pMap->count;
	if (i == capacity)
		return HANDLE_DEFAULT;

	MapHandles(capacity, pMap)[i] = handle;
	pMap->keys[i] = key;
	pMap->count++;

	return HANDLE_GENERATION_INCREMENT(i);
}

static inline block_handle MapFind(int capacity, MapBase* pMap, block_key key)
{
	for (u32 i = 0; i < pMap->count; ++i) {
		if (pMap->keys[i] == key)
			return MapHandles(capacity, pMap)[i];
	}

	return HANDLE_DEFAULT;
}

#define MAP_ADD(map, handle, key) MapAdd(COUNT(map.keys), (MapBase*)&map, handle, key)
#define MAP_FIND(map, key) MapFind(COUNT(map.keys), (MapBase*)&map, key)

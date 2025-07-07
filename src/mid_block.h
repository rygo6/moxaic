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

typedef block_handle block_h; // lets go to this

// it seems you cannot pack 4 and 12 bits
//typedef unsigned _BitInt(4) u4;
//typedef unsigned _BitInt(12) u12;
//typedef struct PACKED block_handle2 {
//	u4 generation : 4;
//	u12 handle : 12;
//} block_handle2;


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

static block_handle BlockClaim(int occupiedCount, bitset_t* pOccupiedSet, block_key* pKeys, uint8_t* pGenerations, uint32_t key)
{

	int i = BitScanFirstZero(occupiedCount, pOccupiedSet);
	if (i == -1) return HANDLE_DEFAULT;
	BITSET(pOccupiedSet, i);
	pKeys[i] = key;
	pGenerations[i] = pGenerations[i] == HANDLE_GENERATION_MAX ? 1 : (pGenerations[i] + 1) & 0xF;
	return HANDLE_GENERATION_SET(i, pGenerations[i]);
}
static block_handle BlockFindByHash(int hashCount, block_key* pHashes, uint8_t* pGenerations, block_key hash)
{
	for (int i = 0; i < hashCount; ++i) {
		if (pHashes[i] == hash)
			return HANDLE_GENERATION_SET(i, pGenerations[i]);
	}
	return HANDLE_DEFAULT;
}

static block_handle BlockCountOccupied(int occupiedCount, bitset_t* pOccupiedSet)
{
	return BitCountOnes(occupiedCount, pOccupiedSet);
}

#define HAS_FIELD(_, _field) (offsetof(_, _field) >= 0)
#define IS_TYPE(_var, _type) _Generic((_var), _type: 1, default: 0)
#define SAME_TYPE(_a, _b) _Generic((typeof(_a)){}, typeof(_b): 1, default: 0)

#define ASSERT(_assert, _message) assert(_assert && _message)
#define STATIC_ASSERT_HAS_FIELD(_, _field) static_assert(HAS_FIELD(_, _field), #_ " does not have field " #_field)
#define STATIC_ASSERT_TYPE(_var, _type) static_assert(IS_TYPE(_var, _type), #_var " is not typeof " #_type)
#define ASSERT_HANDLE_RANGE(_, _handle, _message) ASSERT(HANDLE_INDEX(_handle) >= 0 && HANDLE_INDEX(_handle) < COUNT(_.blocks), _message);

// someway these should take ptrs... or should this be static block? STATIC_BLOCK_CLAIM ?
#define BLOCK_CLAIM(_, _key)                                                                               \
	({                                                                                                     \
		int _handle = BlockClaim(sizeof(_.occupied), (bitset_t*)&_.occupied, _.keys, _.generations, _key); \
		ASSERT(_handle >= 0, #_ ": Claiming handle. Out of capacity.");                                    \
		(block_handle) _handle;                                                                            \
	})
#define BLOCK_RELEASE(_, _handle)                                                                                                    \
	({                                                                                                                               \
		STATIC_ASSERT_TYPE(_handle, block_handle);                                                                                   \
		ASSERT(HANDLE_INDEX(_handle) >= 0 && HANDLE_INDEX(_handle) < COUNT(_.blocks), #_ ": Releasing block handle. Out of range."); \
		ASSERT(BITTEST(_.occupied, HANDLE_INDEX(_handle)), #_ ": Releasing block handle Should be occupied.");                       \
		BITCLEAR(_.occupied, (int)HANDLE_INDEX(_handle));                                                                            \
		&_.blocks[HANDLE_INDEX(_handle)];                                                                                            \
	})
#define BLOCK_FIND(_, _hash)                                           \
	({                                                                 \
		ASSERT(_hash != 0, "Trying to search for 0 hash!");            \
		BlockFindByHash(sizeof(_.keys), _.keys, _.generations, _hash); \
	})
#define BLOCK_HANDLE(_, _p)                                                                                    \
	({                                                                                                         \
		ASSERT(_p >= _.blocks && _p < _.blocks + COUNT(_.blocks), #_ ": Getting block handle. Out of range."); \
		(block_handle)(HANDLE_GENERATION_SET((_p - _.blocks), _.generations[(_p - _.blocks)]));                \
	})
#define BLOCK_KEY(_, _p)                                                                                    \
	({                                                                                                      \
		ASSERT(_p >= _.blocks && _p < _.blocks + COUNT(_.blocks), #_ ": Getting block key. Out of range."); \
		(block_key)(_.keys[_p - _.blocks]);                                                                 \
	})
#define BLOCK_PTR(_, _handle)                                                     \
	({                                                                            \
		STATIC_ASSERT_TYPE(_handle, block_handle);                                \
		ASSERT_HANDLE_RANGE(_, _handle, #_ ": Getting block ptr. Out of range."); \
		&_.blocks[HANDLE_INDEX(_handle)];                                         \
	})
#define BLOCK_OCCUPIED(_, _handle)                                                      \
	({                                                                                  \
		STATIC_ASSERT_TYPE(_handle, block_handle);                                      \
		ASSERT_HANDLE_RANGE(_, _handle, #_ ": Checking block occupied. Out of range."); \
		BITTEST(_.occupied, _handle);                                                   \
	})
#define BLOCK_COUNT(_) BitCountOnes(sizeof(_.occupied), (bitset_t*)&_.occupied)

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

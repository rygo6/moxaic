#pragma once




//typedef struct SlubSlot {
//	bool occupied : 1;
//} SlubSlot;
//
//typedef bitset64_t slab_bitset_t;

// I don't know if wnat a SLUB... SLAB might be better in all scenarios? Maybe a SLUB for a the bigger, fewer, allocs?
//#define CACHE_SIZE 64
//typedef struct __attribute((aligned(CACHE_SIZE))) {
//	uint8_t data[CACHE_SIZE];
//} SlubBlock;
//
//#define POOL_BLOCK_COUNT 1U << 16
//typedef uint16_t SlubHandle;
//typedef struct {
//	SlubSlot    slots[POOL_BLOCK_COUNT];
//	SlubBlock   blocks[POOL_BLOCK_COUNT];
//} SlubMemory;
//
//typedef struct {
//	// explore 24 and 20 bit hashes
//	uint32_t value;
//} Hash;
//
//#define POOL_HANDLE_MAX_CAPACITY 1U << 8
//typedef uint8_t PoolHandle;
//typedef struct {
//	SlubSlot    slots[POOL_HANDLE_MAX_CAPACITY];
//	SlubHandle  handles[POOL_HANDLE_MAX_CAPACITY];
//	Hash        hashes[POOL_HANDLE_MAX_CAPACITY];
//	size_t      blockSize;
//} HashedPool;
//
//#define POOL_STRUCT(_type, _capacity)                                                                                  \
//	typedef struct {                                                                                                   \
//		SlubSlot   slots[_capacity];                                                                                   \
//		SlubHandle handles[_capacity];                                                                                 \
//		Hash       hashes[_capacity];                                                                                  \
//	} _type##HashedPool;                                                                                               \
//	_Static_assert(_capacity <= POOL_HANDLE_MAX_CAPACITY, #_type " " #_capacity " exceeds POOL_HANDLE_MAX_CAPACITY."); \
//	static inline void ClaimSessionPoolHandle(_type##HashedPool* pPool, PoolHandle* pPoolHandle)                       \
//	{                                                                                                                  \
//		ClaimPoolHandle(sizeof(_type), _capacity, pPool->slots, pPool->handles, pPool->hashes, pPoolHandle);           \
//	}
//
//extern SlubMemory poolMemory;
//
//static XrResult ClaimBlockHandle(size_t size, SlubHandle* pBlockHandle)
//{
//	assert((size / CACHE_SIZE) < POOL_BLOCK_COUNT);
//	uint32_t blockCount = (size / CACHE_SIZE) + 1;
//	printf("Scanning blockCount size %d\n", blockCount);
//
//	uint32_t  requiredBlockCount = blockCount;
//	uint32_t  blockHandle = UINT32_MAX;
//	for (int i = 0; i < POOL_BLOCK_COUNT; ++i) {
//		if (poolMemory.slots[i].occupied) {
//			requiredBlockCount = blockCount;
//			continue;
//		}
//
//		if (--requiredBlockCount != 0)
//			continue;
//
//		blockHandle = i;
//		break;
//	}
//
//	if (blockHandle == UINT32_MAX) {
//		LOG_ERROR("Couldn't find required blockCount size.\n");
//		return XR_ERROR_LIMIT_REACHED;
//	}
//
//	*pBlockHandle = blockHandle;
//
//	printf("Found blockHandle %d\n", blockHandle);
//	return XR_SUCCESS;
//}
//static void ReleaseBlockHandle(size_t size, SlubHandle blockHandle)
//{
//	assert((size / CACHE_SIZE) < POOL_BLOCK_COUNT);
//	uint8_t blockCount = (size / CACHE_SIZE) + 1;
//	printf("Releasing blockCount size %d\n", blockCount);
//
//	for (int i = 0; i < blockCount; ++i) {
//		assert((blockHandle + i) < POOL_BLOCK_COUNT);
//		poolMemory.slots[blockHandle + i].occupied = false;
//	}
//}
//
//static XrResult ClaimPoolHandle(int blockSize, int poolCapacity, SlubSlot* pSlots, SlubHandle* pBlockHandles, Hash* pHashes, PoolHandle* pPoolHandle)
//{
//	uint32_t poolHandle = UINT32_MAX;
//	for (int i = 0; i < poolCapacity; ++i) {
//		if (pSlots[i].occupied) {
//			continue;
//		}
//
//		pSlots[i].occupied = true;
//		poolHandle = i;
//		break;
//	}
//
//	if (poolHandle == UINT32_MAX) {
//		LOG_ERROR("Couldn't find available handle.\n");
//		return XR_ERROR_LIMIT_REACHED;
//	}
//
//	SlubHandle blockHandle;
//	if (ClaimBlockHandle(blockSize, &blockHandle) != XR_SUCCESS) {
//		return XR_ERROR_LIMIT_REACHED;
//	}
//
//	pBlockHandles[poolHandle] = blockHandle;
//
//	*pPoolHandle = poolHandle;
//
//	printf("Found PoolHandle %d\n", poolHandle);
//	return XR_SUCCESS;
//}
//static XrResult ReleasePoolHandle(int blockSize, SlubSlot* pSlots, SlubHandle* pBlockHandles, Hash* pHashes, PoolHandle poolHandle)
//{
//	assert(pSlots[poolHandle].occupied);
//
//	ReleaseBlockHandle(blockSize, pBlockHandles[poolHandle]);
//	pSlots[poolHandle].occupied = false;
//	pHashes[poolHandle].value = 0;
//
//	printf("Released PoolHandle %d\n", poolHandle);
//	return XR_SUCCESS;
//}


//#define ClaimHandleHashed(_pool, _pValue, _hash) XR_CHECK(_ClaimHandleHashed((Pool*)&_pool, sizeof(_pool.data[0]), COUNT(_pool.data), (void**)&_pValue, _hash))
//static XrResult _ClaimHandleHashed(Pool* pPool, int stride, int capacity, void** ppValue, XrHash _hash)
//{
//	if (pPool->currentIndex >= capacity) {
//		fprintf(stderr, "XR_ERROR_LIMIT_REACHED");
//		return XR_ERROR_LIMIT_REACHED;
//	}
//	const uint32_t handle = pPool->currentIndex++;
//	*ppValue = &pPool->data + (handle * stride);
//	XrHash* pHashStart = (XrHash*)(&pPool->data + (capacity * stride));
//	XrHash* pHash = pHashStart + (handle * sizeof(XrHash));
//	*pHash = _hash;
//	return XR_SUCCESS;
//}
//#define ClaimHandle(_pool, _pValue) XR_CHECK(_ClaimHandle(_pool.slots, _pool.data, sizeof(_pool.data[0]), COUNT(_pool.slots), (void**)&_pValue))
//static XrResult _ClaimHandle(SlubSlot* pSlots, void* pData, int stride, int capacity, void** ppValue)
//{
//	uint32_t handle = UINT32_MAX;
//	for (int i = 0; i < capacity; ++i) {
//		if (pSlots[i].occupied) {
//			continue;
//		}
//
//		pSlots[i].occupied = true;
//		handle = i;
//		break;
//	}
//
//	if (handle == UINT32_MAX) {
//		LOG_ERROR("XR_ERROR_LIMIT_REACHED\n");
//		return XR_ERROR_LIMIT_REACHED;
//	}
//
//	*ppValue = pData + (handle * stride);
//	return XR_SUCCESS;
//}
//#define ReleaseHandle(_pool, _handle) XR_CHECK(_ReleaseHandle(_pool.slots, _handle))
//static XrResult _ReleaseHandle(SlubSlot* pSlots, Handle_dep handle)
//{
//	assert(pSlots[handle].occupied == true);
//	pSlots[handle].occupied = false;
//	return XR_SUCCESS;
//}
//#define GetHandle(_pool, _pValue) _GetHandle(_pool.data, sizeof(_pool.data[0]), _pValue)
//static Handle_dep _GetHandle(void* pData, int stride, const void* pValue)
//{
//	return (Handle_dep)((pValue - pData) / stride);
//}
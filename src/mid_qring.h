/*
 *
 * Mid Queue Ring Header
 *
 * Requires -fno-strict-aliasing and -fwrapv.
 * //TODO rename to channel to reflect p9
 */
#ifndef MID_QRING_H
#define MID_QRING_H

#include <stdatomic.h>

#include "mid_common.h"

// We rely on natural type wrapping in atomic_fetch_add so CAPACITY must equal that inherit to the type itself. Could use bitint to control size
typedef u8 qring_h;
#define MID_QRING_CAPACITY (1 << (sizeof(qring_h) * CHAR_BIT))



typedef struct MidQRing {
	_Atomic qring_h head;
	_Atomic qring_h tail;
} MidQRing;

/* Enqueue by memcpy of value. */
MidResult midQRingEnqueue(MidQRing* pQ, int valueSize, int capacity, void* pValues, void* pValue);
MidResult midQRingDequeue(MidQRing* pQ, int valueSize, int capacity, void* pValues, void* pValue);
#define MID_QRING_ENQUEUE(_pQ, _pValues, _pValue) ({                                                     \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Array does not match value type!");                 \
	static_assert(IS_POWER_OF_2(COUNT(_pValues)), "Array size is not power of 2!");                      \
	static_assert(COUNT(_pValues) <= MID_QRING_CAPACITY, "Array size greater than MID_QRING_CAPACITY!"); \
	midQRingEnqueue(_pQ, sizeof(_pValues[0]), COUNT(_pValues), _pValues, _pValue);                       \
})
#define MID_QRING_DEQUEUE(_pQ, _pValues, _pValue) ({                                                     \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!");           \
    static_assert(IS_POWER_OF_2(COUNT(_pValues)), "Value array size is not power of 2!");                \
    static_assert(COUNT(_pValues) <= MID_QRING_CAPACITY, "Array size greater than MID_QRING_CAPACITY!"); \
	midQRingDequeue(_pQ, sizeof(_pValues[0]), COUNT(_pValues), _pValues, _pValue);                       \
})

/* Enqueue by writing to directly to a ptr. */
MidResult midQRingEnqueueBegin(MidQRing* pQ, int valueSize, int capacity, void* pValues, void** ppValue);
MidResult midQRingEnqueueEnd(MidQRing* pQ);
#define MID_QRING_ENQUEUE_SCOPE(_pQ, _pValues, _pValue)                                                                  \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!");                           \
    static_assert(IS_POWER_OF_2(COUNT(_pValues)), "Value array size is not power of 2!");                                \
    static_assert(COUNT(_pValues) <= MID_QRING_CAPACITY, "Array size greater than MID_QRING_CAPACITY!");                 \
	for (MidResult result = midQRingEnqueueBegin(_pQ, sizeof(_pValues[0]), COUNT(_pValues), _pValues, (void**)&_pValue); \
		result == MID_SUCCESS;                                                                                           \
		result = midQRingEnqueueEnd(_pQ))

#endif // MID_QRING_H

/*
 *
 * Mid Queue Ring Implementation
 *
 */
#if defined(MID_QRING_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)
#undef MID_QRING_IMPLEMENTATION

MidResult midQRingEnqueueBegin(MidQRing* pQ, int valueSize, int capacity, void* pValues, void** ppValue)
{
	qring_h h = atomic_load_explicit(&pQ->head, memory_order_acquire) & (capacity - 1);
	qring_h t = atomic_load_explicit(&pQ->tail, memory_order_acquire) & (capacity - 1);
	if (h + 1 == t) return MID_LIMIT_REACHED;
	*ppValue = pValues + (h * valueSize);
	return MID_SUCCESS;
}

MidResult midQRingEnqueueEnd(MidQRing* pQ)
{
	atomic_fetch_add(&pQ->head, 1); // Automatically wrap u8 to 0 at 256
	return MID_RESULT_FINALIZED;
}

MidResult midQRingEnqueue(MidQRing* pQ, int valueSize, int capacity, void* pValues, void* pValue)
{
	qring_h h = atomic_load_explicit(&pQ->head, memory_order_acquire) & (capacity - 1);
	qring_h t = atomic_load_explicit(&pQ->tail, memory_order_acquire) & (capacity - 1);
	if (h + 1 == t) return MID_LIMIT_REACHED;
    memcpy(pValues + (h * valueSize), pValue, valueSize);
	atomic_fetch_add_explicit(&pQ->head, 1, memory_order_release); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

MidResult midQRingDequeue(MidQRing* pQ, int valueSize, int capacity, void* pValues, void* pValue)
{
	qring_h h = atomic_load_explicit(&pQ->head, memory_order_acquire) & (capacity - 1);
	qring_h t = atomic_load_explicit(&pQ->tail, memory_order_acquire) & (capacity - 1);
	if (h == t) return MID_EMPTY;
	memcpy(pValue, pValues + (t * valueSize), valueSize);
	atomic_fetch_add_explicit(&pQ->tail, 1, memory_order_release); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

#endif // MID_QRING_IMPLEMENTATION
////
//// Mid Queue Ring Header
////
#ifndef MID_QRING_H
#define MID_QRING_H

#include "mid_common.h"

// We rely on natural type wrapping in atomic_fetch_add so CAPACITY must equal that inherit to the type itself. Could use bitint to control size
typedef u8 qring_h;
#define MID_QRING_CAPACITY (1 << (sizeof(qring_h) * CHAR_BIT))

typedef struct MidQRing {
	qring_h head;
	qring_h tail;
} MidQRing;

/* Enqueue by memcpy of value. */
MidResult midQRingEnqueue(MidQRing* pQ, int valueSize, void* pValues, void* pValue);
MidResult midQRingDequeue(MidQRing* pQ, int valueSize, void* pValues, void* pValue);
#define MID_QRING_ENQUEUE(_pQ, _pValues, _pValue) ({                                           \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!"); \
	midQRingEnqueue(_pQ, sizeof(*_pValue), _pValues, _pValue);                                 \
})
#define MID_QRING_DEQUEUE(_pQ, _pValues, _pValue) ({                                           \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!"); \
	midQRingDequeue(_pQ, sizeof(*_pValue), _pValues, _pValue);                                 \
})

/* Enqueue by writing to directly to a ptr. */
MidResult midQRingEnqueueBegin(MidQRing* pQ, int valueSize, void* pValues, void** ppValue);
MidResult midQRingEnqueueEnd(MidQRing* pQ);
#define MID_QRING_ENQUEUE_SCOPE(_pQ, _pValues, _pValue)                                              \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!");       \
	for (MidResult result = midQRingEnqueueBegin(_pQ, sizeof(*_pValue), _pValues, (void**)&_pValue); \
		result == MID_SUCCESS;                                                                       \
		result = midQRingEnqueueEnd(_pQ))

#endif // MID_QRING_H

/*
 * Mid Queue Ring Implementation
 */
#if defined(MID_QRING_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

MidResult midQRingEnqueueBegin(MidQRing* pQ, int valueSize, void* pValues, void** ppValue)
{
	MidQRing ring = atomic_load_explicit(pQ, memory_order_acquire);
	if (ring.head + 1 == ring.tail) return MID_LIMIT_REACHED;
	*ppValue = pValues + (ring.head * valueSize);
	return MID_SUCCESS;
}

MidResult midQRingEnqueueEnd(MidQRing* pQ)
{
	atomic_fetch_add(&pQ->head, 1); // Automatically wrap u8 to 0 at 256
	return MID_RESULT_FINALIZED;
}

MidResult midQRingEnqueue(MidQRing* pQ, int valueSize, void* pValues, void* pValue)
{
	MidQRing ring = atomic_load_explicit(pQ, memory_order_acquire);
	if (ring.head + 1 == ring.tail) return MID_LIMIT_REACHED;
	memcpy(pValues + (ring.head * valueSize), pValue, valueSize);
	atomic_fetch_add_explicit(&pQ->head, 1, memory_order_release); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

MidResult midQRingDequeue(MidQRing* pQ, int valueSize, void* pValues, void* pValue)
{
	MidQRing ring = atomic_load_explicit(pQ, memory_order_acquire);
	if (ring.head == ring.tail) return MID_EMPTY;
	memcpy(pValue, pValues + (ring.tail * valueSize), valueSize);
	atomic_fetch_add_explicit(&pQ->tail, 1, memory_order_release); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

#undef MID_QRING_IMPLEMENTATION
#endif // MID_QRING_IMPLEMENTATION
////
//// Mid Queue Ring Header
////
#ifndef MID_QRING_H
#define MID_QRING_H

#include <string.h>
#include "mid_common.h"

// We rely on natural type wrapping in atomic_fetch_add so CAPACITY must equal that inherit to the type itself
typedef u8 qring_h;
#define MID_QRING_CAPACITY (1 << (sizeof(qring_h) * CHAR_BIT))

typedef struct MidQRing {
	qring_h head;
	qring_h tail;
} MidQRing;

MidResult midQRingEnqueue(MidQRing* pQ, int valueSize, void* pValues, void* pValue);
MidResult midQRingDequeue(MidQRing* pQ, int valueSize, void* pValues, void* pValue);

#define MID_QRING_ENQUEUE(_capacity, _pQ, _pValues, _pValue) ({                                                                                 \
	static_assert(_capacity <= MID_QRING_CAPACITY, "Queue capacity passed to MID_QRING_ENQUEUE has smaller capacity than MID_QRING_CAPACITY!"); \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!");                                                  \
	midQRingEnqueue(_pQ, sizeof(*_pValue), _pValues, _pValue);                                                                                  \
})

#define MID_QRING_DEQUEUE(_capacity, _pQ, _pValues, _pValue) ({                                                                                 \
	static_assert(_capacity <= MID_QRING_CAPACITY, "Queue capacity passed to MID_QRING_ENQUEUE has smaller capacity than MID_QRING_CAPACITY!"); \
	static_assert(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!");                                                  \
	midQRingDequeue(_pQ, sizeof(*_pValue), _pValues, _pValue);                                                                                  \
})

#endif // MID_QRING_H

////
//// Mid Queue Ring Implementation
////
#if defined(MID_QRING_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

MidResult midQRingEnqueue(MidQRing* pQ, int valueSize, void* pValues, void* pValue)
{
	MidQRing ring = atomic_load_explicit(pQ, memory_order_acquire);
	if (ring.head + 1 == ring.tail) return MID_LIMIT_REACHED;
	memcpy(pValues + (ring.head * valueSize), pValue, valueSize);
	atomic_fetch_add(&pQ->head, 1); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

MidResult midQRingDequeue(MidQRing* pQ, int valueSize, void* pValues, void* pValue)
{
	MidQRing ring = atomic_load_explicit(pQ, memory_order_acquire);
	if (ring.head == ring.tail) return MID_EMPTY;
	memcpy(pValue, pValues + (ring.tail * valueSize), valueSize);
	atomic_fetch_add(&pQ->tail, 1); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

#undef MID_QRING_IMPLEMENTATION
#endif // MID_QRING_IMPLEMENTATION
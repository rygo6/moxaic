////
//// Mid Queue Ring Header
////
#ifndef MID_QRING_H
#define MID_QRING_H

#include <string.h>

#ifndef MID_QRING_TYPE
#define MID_QRING_TYPE
typedef u8 qring_h;
#endif

#ifndef MID_QRING_CAPACITY
#define MID_QRING_CAPACITY 256
#endif

#define MID_QRING_TYPE_CAPACITY (1 << (sizeof(qring_h) * CHAR_BIT))
static_assert(MID_QRING_CAPACITY <= MID_QRING_TYPE_CAPACITY, "MID_QRING_TYPE can't store MID_QRING_CAPACITY!");

typedef struct MidQRing {
	qring_h head;
	qring_h tail;
} MidQRing;

int midQRingEnqueue(MidQRing* pQ, int valueSize, void* pValues, void* pValue);
int midQRingDequeue(MidQRing* pQ, int valueSize, void* pValues, void* pValue);

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

int midQRingEnqueue(MidQRing* pQ, int valueSize, void* pValues, void* pValue)
{
	ATOMIC_FENCE_SCOPE {
		qring_h head = pQ->head;
		qring_h tail = pQ->tail;
		if (head + 1 == tail) {
			LOG_ERROR("Ring Buffer Wrapped!\n");
			return 1;
		}
		memcpy(pValues + head, pValue, valueSize);
		pQ->head = (head + 1) % MID_QRING_CAPACITY;
	}
	return 0;
}

int midQRingDequeue(MidQRing* pQ, int valueSize, void* pValues, void* pValue)
{
	ATOMIC_FENCE_SCOPE {
		qring_h head = pQ->head;
		qring_h tail = pQ->tail;
		if (head == tail) return 1;
		memcpy(pValue, pValues + tail, valueSize);
		pQ->tail = (tail + 1) % MID_QRING_CAPACITY;
	}
	return 0;
}

#undef MID_QRING_IMPLEMENTATION
#endif // MID_QRING_IMPLEMENTATION
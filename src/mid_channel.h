/*
 * Mid Channel Header
 *
 * IPC channel based on ring queue.
 * Requires -fno-strict-aliasing and -fwrapv.
 */
#ifndef MID_CHANNEL_H
#define MID_CHANNEL_H

#include <stdatomic.h>
#include "mid_common.h"

typedef u8 channel_ring_h;
#define MID_QRING_CAPACITY (1 << (sizeof(channel_ring_h) * CHAR_BIT))

typedef volatile struct MidChannelRing {
	channel_ring_h head;
	channel_ring_h tail;
} MidChannelRing;

/* Send/Receive by memcpy of value. */
MidResult midChannelSend(volatile MidChannelRing* pRing, int valueSize, int capacity, volatile void* pValues, void* pValue);
MidResult midChannelRecv(volatile MidChannelRing* pRing, int valueSize, int capacity, volatile void* pValues, void* pValue);
#define MID_CHANNEL_SEND(_pRing, _pValues, _pValue) ({ \
	STATIC_ASSERT(TYPES_EQUAL(*_pValues, *_pValue), "Array does not match value type!"); \
	STATIC_ASSERT(IS_POWER_OF_2(COUNT(_pValues)), "Array size is not power of 2!"); \
	STATIC_ASSERT(COUNT(_pValues) <= MID_QRING_CAPACITY, "Array size greater than MID_QRING_CAPACITY!"); \
	midChannelSend(_pRing, sizeof(_pValues[0]), COUNT(_pValues), _pValues, _pValue); \
})
#define MID_CHANNEL_RECV(_pRing, _pValues, _pValue) ({ \
	STATIC_ASSERT(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!"); \
    STATIC_ASSERT(IS_POWER_OF_2(COUNT(_pValues)), "Value array size is not power of 2!"); \
    STATIC_ASSERT(COUNT(_pValues) <= MID_QRING_CAPACITY, "Array size greater than MID_QRING_CAPACITY!"); \
	midChannelRecv(_pRing, sizeof(_pValues[0]), COUNT(_pValues), _pValues, _pValue); \
})

/* Send by writing to directly to a ptr. */
MidResult midChannelSendBegin(volatile MidChannelRing* pRing, int valueSize, int capacity, volatile void* pValues, volatile void** ppValue);
MidResult midChannelSendEnd(volatile MidChannelRing* pRing);
#define MID_CHANNEL_SEND_SCOPE(_pRing, _pValues, _pValue) \
	STATIC_ASSERT(TYPES_EQUAL(*_pValues, *_pValue), "Value array does not match value type!"); \
    STATIC_ASSERT(IS_POWER_OF_2(COUNT(_pValues)), "Value array size is not power of 2!"); \
    STATIC_ASSERT(COUNT(_pValues) <= MID_QRING_CAPACITY, "Array size greater than MID_QRING_CAPACITY!"); \
	for (MidResult result = midChannelSendBegin(_pRing, sizeof(_pValues[0]), COUNT(_pValues), _pValues, (volatile void**)&_pValue); \
		result == MID_SUCCESS; \
		result = midChannelSendEnd(_pRing))

#endif // MID_CHANNEL_H

/*
 * Mid Channel Implementation
 */
#if defined(MID_QRING_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)
#undef MID_QRING_IMPLEMENTATION

MidResult midChannelSendBegin(volatile MidChannelRing* pRing, int valueSize, int capacity, volatile void* pValues, volatile void** ppValue)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	channel_ring_h h = pRing->head & (capacity - 1);
	channel_ring_h t = pRing->tail & (capacity - 1);
	if (h + 1 == t) return MID_LIMIT_REACHED;
	*ppValue = pValues + (h * valueSize);
	__atomic_thread_fence(__ATOMIC_RELEASE);
	return MID_SUCCESS;
}

MidResult midChannelSendEnd(volatile MidChannelRing* pRing)
{
	__atomic_fetch_add(&pRing->head, 1, __ATOMIC_ACQUIRE); // Automatically wrap u8 to 0 at 256
	return MID_RESULT_FINALIZED;
}

MidResult midChannelSend(volatile MidChannelRing* pRing, int valueSize, int capacity, volatile void* pValues, void* pValue)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	channel_ring_h h = pRing->head & (capacity - 1);
	channel_ring_h t = pRing->tail & (capacity - 1);
	if (h + 1 == t) return MID_LIMIT_REACHED;
    memcpy((void*)pValues + (h * valueSize), pValue, valueSize);
	__atomic_fetch_add(&pRing->head, 1, __ATOMIC_RELEASE); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

MidResult midChannelRecv(volatile MidChannelRing* pRing, int valueSize, int capacity, volatile void* pValues, void* pValue)
{
	__atomic_thread_fence(__ATOMIC_ACQUIRE);
	channel_ring_h h = pRing->head & (capacity - 1);
	channel_ring_h t = pRing->tail & (capacity - 1);
	if (h == t) return MID_EMPTY;
	memcpy(pValue, (void*)pValues + (t * valueSize), valueSize);
	__atomic_fetch_add(&pRing->tail, 1, __ATOMIC_RELEASE); // Automatically wrap u8 to 0 at 256
	return MID_SUCCESS;
}

#endif // MID_QRING_IMPLEMENTATION

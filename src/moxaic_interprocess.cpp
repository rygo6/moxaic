#include "moxaic_interprocess.hpp"
#include "moxaic_node.hpp"

using namespace Moxaic;

constexpr StaticArray k_InterProcessTargetParamSize{
    sizeof(Node::ImportParam),
};

void InterProcessProducer::Enque(const InterProcessTargetFunc target, const void* param) const
{
    const auto pBuffer = static_cast<RingBuffer *>(m_pBuffer);
    pBuffer->pRingBuffer[pBuffer->head] = target;
    memcpy(pBuffer->pRingBuffer + pBuffer->head + RingBuffer::HeaderSize, param, k_InterProcessTargetParamSize[target]);
    pBuffer->head = pBuffer->head + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target];
}

MXC_RESULT InterProcessReceiver::Init(const std::string& sharedMemoryName,
                                      const StaticArray<InterProcessFunc, InterProcessTargetFunc::Count>&& targetFuncs)
{
    m_TargetFuncs = targetFuncs;
    return InterProcessBuffer::Init(sharedMemoryName);
}

int InterProcessReceiver::Deque() const
{
    // TODO this needs to actually cycle around the ring buffer, this is only half done

    const auto pBuffer = static_cast<RingBuffer *>(m_pBuffer);

    if (pBuffer->head == pBuffer->tail)
        return 0;

    MXC_LOG("IPC Polling.", pBuffer->head, pBuffer->tail);

    const InterProcessTargetFunc target = static_cast<InterProcessTargetFunc>(pBuffer->pRingBuffer[pBuffer->tail]);

    // TODO do you copy it out of the IPC or just send that chunk of shared memory on through?
    // If consumer consumes too slow then producer might run out of data in a stream?
    // From trusted parent app sending shared memory through is probably fine
    //    void *param = malloc(fbrIPCTargetParamSize(target));
    //    memcpy(param, pRingBuffer->pRingBuffer + pRingBuffer->tail + FBR_IPC_RING_HEADER_SIZE, fbrIPCTargetParamSize(target));
    void* param = pBuffer->pRingBuffer + pBuffer->tail + RingBuffer::HeaderSize;

    if (pBuffer->tail + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target] > RingBuffer::RingBufferSize) {
        // TODO this needs to actually cycle around the ring buffer, this is only half done
        MXC_LOG_ERROR("IPC BYTE ARRAY REACHED END!!!");
    }

    MXC_LOG("Calling IPC Target", target);
    m_TargetFuncs[target](param);

    pBuffer->tail = pBuffer->tail + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target];

    return 1;
}

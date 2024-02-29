// #define MXC_DISABLE_LOG

#include "moxaic_interprocess.hpp"
#include "moxaic_node.hpp"

using namespace Moxaic;

constexpr StaticArray k_InterProcessTargetParamSize{
  sizeof(Node::ImportParam),
};

void InterProcessProducer::Enque(InterProcessTargetFunc const target,
                                 void const* param) const
{
    auto const pBuffer = static_cast<RingBuffer*>(sharedBuffer);
    pBuffer->ringBuffer[pBuffer->head] = target;
    memcpy((void*)(pBuffer->ringBuffer + pBuffer->head + RingBuffer::HeaderSize), param, k_InterProcessTargetParamSize[target]);
    pBuffer->head = pBuffer->head + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target];
}

MXC_RESULT InterProcessReceiver::Init(std::string const& sharedMemoryName,
                                      StaticArray<InterProcessFunc, InterProcessTargetFunc::Count> const&& targetFuncs)
{
    this->targetFuncs = targetFuncs;
    return InterProcessBuffer::Init(sharedMemoryName);
}

int InterProcessReceiver::Deque() const
{
    // TODO this needs to actually cycle around the ring buffer, this is only half done

    auto const pBuffer = static_cast<RingBuffer*>(sharedBuffer);

    if (pBuffer->head == pBuffer->tail)
        return 0;

    MXC_LOG("IPC Polling.", pBuffer->head, pBuffer->tail);

    InterProcessTargetFunc const target = static_cast<InterProcessTargetFunc>(pBuffer->ringBuffer[pBuffer->tail]);

    // TODO do you copy it out of the IPC or just send that chunk of shared memory on through?
    // If consumer consumes too slow then producer might run out of data in a stream?
    // From trusted parent app sending shared memory through is probably fine
    //    void *param = malloc(fbrIPCTargetParamSize(target));
    //    memcpy(param, pRingBuffer->pRingBuffer + pRingBuffer->tail + FBR_IPC_RING_HEADER_SIZE, fbrIPCTargetParamSize(target));
    const auto param = (void*)(pBuffer->ringBuffer + pBuffer->tail + RingBuffer::HeaderSize);

    if (pBuffer->tail + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target] > RingBuffer::RingBufferSize) {
        // TODO this needs to actually cycle around the ring buffer, this is only half done
        MXC_LOG_ERROR("IPC BYTE ARRAY REACHED END!!!");
    }

    MXC_LOG("Calling IPC Target", target);
    targetFuncs[target](param);

    pBuffer->tail = pBuffer->tail + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target];

    return 1;
}

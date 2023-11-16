#include "moxaic_interprocess.hpp"
#include "moxaic_node.hpp"

using namespace Moxaic;

constexpr std::array k_InterProcessTargetParamSize{
        sizeof(Moxaic::Node::ImportParam),
};

MXC_RESULT Moxaic::InterProcessProducer::Init(const std::string &sharedMemoryName)
{
    m_RingBuffer.Init(sharedMemoryName);
    return MXC_SUCCESS;
}

void Moxaic::InterProcessProducer::Enque(InterProcessTargetFunc target, void *param)
{
    auto &buffer = m_RingBuffer.buffer();
    buffer.pRingBuffer[buffer.head] = target;
    memcpy(buffer.pRingBuffer + buffer.head + RingBuffer::HeaderSize, param, k_InterProcessTargetParamSize[target]);
    buffer.head = buffer.head + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target];
}

MXC_RESULT Moxaic::InterProcessReceiver::Init(const std::string &sharedMemoryName,
                                              const std::array<InterProcessFunc, InterProcessTargetFunc::Count> &&targetFuncs)
{
    m_RingBuffer.Init(sharedMemoryName);
    m_TargetFuncs = targetFuncs;
    return MXC_SUCCESS;
}

int Moxaic::InterProcessReceiver::Deque()
{
    // TODO this needs to actually cycle around the ring buffer, this is only half done

    auto &buffer = m_RingBuffer.buffer();

//    printf("%s %d %d", "IPC", buffer.head, buffer.tail);
    if (buffer.head == buffer.tail)
        return 0;

    MXC_LOG("IPC Polling.", buffer.head, buffer.tail);

    InterProcessTargetFunc target = static_cast<InterProcessTargetFunc>(buffer.pRingBuffer[buffer.tail]);

    // TODO do you copy it out of the IPC or just send that chunk of shared memory on through?
    // If consumer consumes too slow then producer might run out of data in a stream?
    // From trusted parent app sending shared memory through is probably fine
//    void *param = malloc(fbrIPCTargetParamSize(target));
//    memcpy(param, pRingBuffer->pRingBuffer + pRingBuffer->tail + FBR_IPC_RING_HEADER_SIZE, fbrIPCTargetParamSize(target));
    void *param = buffer.pRingBuffer + buffer.tail + RingBuffer::HeaderSize;

    if (buffer.tail + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target] > RingBuffer::RingBufferSize) {
        // TODO this needs to actually cycle around the ring buffer, this is only half done
        MXC_LOG_ERROR("IPC BYTE ARRAY REACHED END!!!");
    }

    MXC_LOG("Calling IPC Target", target);
    m_TargetFuncs[target](param);

    buffer.tail = buffer.tail + RingBuffer::HeaderSize + k_InterProcessTargetParamSize[target];

    return 1;
}
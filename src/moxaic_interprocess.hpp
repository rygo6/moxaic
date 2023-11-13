#pragma once

#include "moxaic_logging.hpp"

#include <cstdint>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    template<typename T>
    class InterProcessBuffer
    {
    public:
        InterProcessBuffer() = default;
        virtual ~InterProcessBuffer() = default;

        MXC_RESULT Init()
        {
            return MXC_SUCCESS;
        }

        MXC_RESULT InitFromImport()
        {
            return MXC_SUCCESS;
        }

        inline T &Buffer() { return *(T *) (m_pBuffer); }
        inline int BufferSize() const { return sizeof(T); }

    private:
        void *m_pBuffer{};

#ifdef WIN32
        HANDLE m_hMapFile{};
#endif
    };

    struct RingBuffer
    {
        static constexpr int k_RingBufferCount = 256;
        static constexpr int k_RingBufferSize = k_RingBufferCount * sizeof(uint8_t);
        static constexpr int k_HeaderSize = 1;

        uint8_t head;
        uint8_t tail;
        uint8_t pRingBuffer[k_RingBufferSize];
    };

    enum InterProcessTargetFunc
    {
        ImportCompositor = 0,
        ImportFramebuffer,
        ImportCamera,
        Count,
    };

    class InterProcessRingBuffer
    {
    public:
        InterProcessRingBuffer();
        virtual ~InterProcessRingBuffer();

        MXC_RESULT Init();

    private:
        InterProcessBuffer<RingBuffer> m_RingBuffer{};
        void (*pTargetFuncs[InterProcessTargetFunc::Count])(void *){};
    };
}

#pragma once

#include "moxaic_logging.hpp"

#include <cstdint>
#include <array>
#include <functional>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    template<typename T>
    class InterProcessBuffer
    {
        MXC_NO_VALUE_PASS(InterProcessBuffer);
    public:
        InterProcessBuffer() = default;

        virtual ~InterProcessBuffer()
        {
            UnmapViewOfFile(m_pBuffer);
            CloseHandle(m_hMapFile);
        };

        MXC_RESULT Init(std::string sharedMemoryName)
        {
            m_hMapFile = CreateFileMapping(
                    INVALID_HANDLE_VALUE, // use paging file
                    NULL, // default security
                    PAGE_READWRITE, // read/write access
                    0, // maximum object size (high-order DWORD)
                    Size(), // maximum object size (low-order DWORD)
                    sharedMemoryName.c_str()); // name of mapping object
            if (m_hMapFile == nullptr) {
                MXC_LOG_ERROR("Could not create file mapping object.", GetLastError());
                return MXC_FAIL;
            }

            m_pBuffer = MapViewOfFile(m_hMapFile,
                                      FILE_MAP_ALL_ACCESS,
                                      0,
                                      0,
                                      Size());
            memset(m_pBuffer, 0, Size());
            if (m_pBuffer == nullptr) {
                MXC_LOG_ERROR("Could not map view of file.", GetLastError());
                CloseHandle(m_pBuffer);
                return MXC_FAIL;
            }

            return MXC_SUCCESS;
        }

        MXC_RESULT InitFromImport(std::string sharedMemoryName)
        {
            // todo properly receive handle
            return Init(sharedMemoryName);
        }

        void CopyBuffer(const T &srcBuffer)
        {
            memcpy(m_pBuffer, &srcBuffer, Size());
        }

        inline constexpr int Size() const { return sizeof(T); }

        inline T &buffer() const { return *(T *) (m_pBuffer); }

    private:
#ifdef WIN32
        LPVOID m_pBuffer{nullptr};
        HANDLE m_hMapFile{nullptr};
#endif
    };

    struct RingBuffer
    {
        static constexpr int RingBufferCount = 256;
        static constexpr int RingBufferSize = RingBufferCount * sizeof(uint8_t);
        static constexpr int HeaderSize = 1;

        uint8_t head;
        uint8_t tail;
        uint8_t pRingBuffer[RingBufferSize];
    };

    using InterProcessFunc = std::function<void(void *)>;

    enum InterProcessTargetFunc
    {
        ImportCompositor = 0,
//        ImportFramebuffer,
//        ImportCamera,
        Count,
    };

    class InterProcessProducer
    {
        MXC_NO_VALUE_PASS(InterProcessProducer);
    public:
        InterProcessProducer() = default;
        MXC_RESULT Init(const std::string &sharedMemoryName);
        void Enque(InterProcessTargetFunc, void *param);
    private:
        InterProcessBuffer<RingBuffer> m_RingBuffer{};
    };

    class InterProcessReceiver
    {
        MXC_NO_VALUE_PASS(InterProcessReceiver);
    public:
        InterProcessReceiver() = default;
        MXC_RESULT Init(const std::string &sharedMemoryName,
                        const std::array<InterProcessFunc, InterProcessTargetFunc::Count> &&targetFuncs);
        int Deque();
    private:
        std::array<InterProcessFunc, InterProcessTargetFunc::Count> m_TargetFuncs{};
        InterProcessBuffer<RingBuffer> m_RingBuffer{};
    };
}

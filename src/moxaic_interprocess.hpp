#pragma once

#include "moxaic_logging.hpp"
#include "static_array.hpp"

#include <cstdint>
#include <functional>

#ifdef WIN32
#include <windows.h>
#endif

namespace Moxaic
{
    template<typename T>
    class InterProcessBuffer
    {
    public:
        MXC_NO_VALUE_PASS(InterProcessBuffer);

        InterProcessBuffer() = default;

        virtual ~InterProcessBuffer()
        {
            UnmapViewOfFile(m_pBuffer);
            CloseHandle(m_hMapFile);
        };

        MXC_RESULT Init(const std::string& sharedMemoryName)
        {
            m_hMapFile = CreateFileMapping(
              INVALID_HANDLE_VALUE,
              // use paging file
              nullptr,
              // default security
              PAGE_READWRITE,
              // read/write access
              0,
              // maximum object size (high-order DWORD)
              Size(),
              // maximum object size (low-order DWORD)
              sharedMemoryName.c_str());// name of mapping object
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

        MXC_RESULT InitFromImport(const std::string& sharedMemoryName)
        {
            // todo properly receive handle
            return Init(sharedMemoryName);
        }

        void CopyBuffer(const T& srcBuffer)
        {
            memcpy(m_pBuffer, &srcBuffer, Size());
        }

        static constexpr int Size() { return sizeof(T); }

        const T& buffer() const { return *static_cast<T*>(m_pBuffer); }

    protected:
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

    using InterProcessFunc = std::function<void(void*)>;

    enum InterProcessTargetFunc {
        ImportCompositor = 0,
        //        ImportFramebuffer,
        //        ImportCamera,
        Count,
    };

    class InterProcessProducer : public InterProcessBuffer<RingBuffer>
    {
    public:
        void Enque(InterProcessTargetFunc, const void* param) const;
    };

    class InterProcessReceiver : public InterProcessBuffer<RingBuffer>
    {
        MXC_NO_VALUE_PASS(InterProcessReceiver);

    public:
        InterProcessReceiver() = default;
        MXC_RESULT Init(const std::string& sharedMemoryName,
                        const StaticArray<InterProcessFunc, InterProcessTargetFunc::Count>&& targetFuncs);
        int Deque() const;

    private:
        StaticArray<InterProcessFunc, InterProcessTargetFunc::Count> m_TargetFuncs{};
    };
}// namespace Moxaic

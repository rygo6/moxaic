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
            UnmapViewOfFile(m_pSharedBuffer);
            CloseHandle(m_hMapFile);
        };

        MXC_RESULT Init(std::string const& sharedMemoryName)
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

            m_pSharedBuffer = MapViewOfFile(m_hMapFile,
                                            FILE_MAP_ALL_ACCESS,
                                            0,
                                            0,
                                            Size());
            memset(m_pSharedBuffer, 0, Size());
            if (m_pSharedBuffer == nullptr) {
                MXC_LOG_ERROR("Could not map view of file.", GetLastError());
                CloseHandle(m_pSharedBuffer);
                return MXC_FAIL;
            }

            return MXC_SUCCESS;
        }

        MXC_RESULT InitFromImport(std::string const& sharedMemoryName)
        {
            // todo properly receive handle
            return Init(sharedMemoryName);
        }

        void SyncLocalBuffer()
        {
            memcpy(&m_LocalBuffer, m_pSharedBuffer, Size());
        }

        void PushLocalBuffer()
        {
            memcpy(m_pSharedBuffer, &m_LocalBuffer, Size());
        }

        MXC_ACCESS(LocalBuffer);
        MXC_GETSET(LocalBuffer);

        T const& GetSharedBuffer() const { return *static_cast<T*>(m_pSharedBuffer); }

        static constexpr int Size() { return sizeof(T); }

    protected:
        T m_LocalBuffer;

#ifdef WIN32
        LPVOID m_pSharedBuffer{nullptr};
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
        void Enque(InterProcessTargetFunc, void const* param) const;
    };

    class InterProcessReceiver : public InterProcessBuffer<RingBuffer>
    {
        MXC_NO_VALUE_PASS(InterProcessReceiver);

    public:
        InterProcessReceiver() = default;
        MXC_RESULT Init(std::string const& sharedMemoryName,
                        StaticArray<InterProcessFunc, InterProcessTargetFunc::Count> const&& targetFuncs);
        int Deque() const;

    private:
        StaticArray<InterProcessFunc, InterProcessTargetFunc::Count> m_TargetFuncs{};
    };
}// namespace Moxaic

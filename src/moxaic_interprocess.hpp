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
            if (sharedBuffer != nullptr)
                UnmapViewOfFile(sharedBuffer);
            if (mapFile != nullptr)
                CloseHandle(mapFile);
        };

        MXC_RESULT Init(const std::string& sharedMemoryName)
        {
            mapFile = CreateFileMapping(
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
            if (mapFile == nullptr) {
                MXC_LOG_ERROR("Could not create file mapping object.", GetLastError());
                return MXC_FAIL;
            }

            sharedBuffer = MapViewOfFile(mapFile,
                                         FILE_MAP_ALL_ACCESS,
                                         0,
                                         0,
                                         Size());
            memset(sharedBuffer, 0, Size());
            if (sharedBuffer == nullptr) {
                MXC_LOG_ERROR("Could not map view of file.", GetLastError());
                CloseHandle(sharedBuffer);
                return MXC_FAIL;
            }

            return MXC_SUCCESS;
        }

        MXC_RESULT InitFromImport(const std::string& sharedMemoryName)
        {
            // todo properly receive handle
            return Init(sharedMemoryName);
        }

        void SyncLocalBuffer()
        {
            memcpy(&localBuffer, sharedBuffer, Size());
        }

        void WriteLocalBuffer()
        {
            memcpy(sharedBuffer, &localBuffer, Size());
        }

        T localBuffer;

        const T& GetSharedBuffer() const { return *static_cast<T*>(sharedBuffer); }

        static constexpr int Size() { return sizeof(T); }

    protected:
#ifdef WIN32
        volatile LPVOID sharedBuffer{nullptr};
        HANDLE mapFile{nullptr};
#endif
    };

    struct RingBuffer
    {
        static constexpr int RingBufferCount = 256;
        static constexpr int RingBufferSize = RingBufferCount * sizeof(uint8_t);
        static constexpr int HeaderSize = 1;

        volatile uint8_t head;
        volatile uint8_t tail;
        volatile uint8_t ringBuffer[RingBufferSize];
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
        StaticArray<InterProcessFunc, InterProcessTargetFunc::Count> targetFuncs{};
    };
}// namespace Moxaic

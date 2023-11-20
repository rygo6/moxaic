#pragma once

namespace Moxaic
{
    // Primarily for vulkan parameter arrays
    template<typename T, uint32_t N>
    struct StaticArray
    {
        // https://twitter.com/SebAaltonen/status/1719973253412761606
        T m_Data[N];

        T& operator[](size_t i) { return m_Data[i]; }
        const T& operator[](size_t i) const { return m_Data[i]; }

        // operator T*() { return m_Data; }
        // operator const T*() const { return m_Data; }

        const T* begin() const { return m_Data; }
        const T* end() const { return m_Data + N; }

        T* data() { return m_Data; }
        const T* data() const { return m_Data; }
        static uint32_t size() { return N; }
    };

    template<typename _Tp, typename... _Up>
    StaticArray(_Tp, _Up...) -> StaticArray<_Tp, 1 + sizeof...(_Up)>;

    // tf did I invent here....
    template<typename T>
    struct StaticRef
    {
        StaticRef(T data) { m_Data = data; }
        T m_Data;
        operator T *() { return &m_Data; }
        operator const T *() const { return &m_Data; }
    };
}

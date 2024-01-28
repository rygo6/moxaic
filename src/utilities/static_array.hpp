#pragma once

namespace Moxaic
{
    template<typename T, uint32_t N>
    struct StaticArray
    {
        T m_Data[N];

        T& operator[](size_t i) { return m_Data[i]; }
        T const& operator[](size_t i) const { return m_Data[i]; }

        operator T*() { return m_Data; }
        operator const T*() const { return m_Data; }

        T const* begin() const { return m_Data; }
        T const* end() const { return m_Data + N; }

        T* data() { return m_Data; }
        T const* data() const { return m_Data; }
        static uint32_t size() { return N; }
    };

    template<typename _Tp, typename... _Up>
    StaticArray(_Tp, _Up...) -> StaticArray<_Tp, 1 + sizeof...(_Up)>;

    // tf did I invent here....
    // because C++ doesn't seem to support taking a ref of a nested designated initializer
    template<typename T>
    struct StaticRef
    {
        constexpr StaticRef(T data) { Data = data; }
        T Data;
        constexpr operator T*() { return &Data; }
        constexpr operator T const*() const { return &Data; }
    };
}
#pragma once

namespace Moxaic
{
    template<typename T, uint32_t N>
    struct StaticArray
    {
        T internalData[N];

        T& operator[](size_t i) { return internalData[i]; }
        T const& operator[](size_t i) const { return internalData[i]; }

        operator T*() { return internalData; }
        operator const T*() const { return internalData; }

        T const* begin() const { return internalData; }
        T const* end() const { return internalData + N; }

        T* data() { return internalData; }
        T const* data() const { return internalData; }
        static uint32_t size() { return N; }
    };

    template<typename _Tp, typename... _Up>
    StaticArray(_Tp, _Up...) -> StaticArray<_Tp, 1 + sizeof...(_Up)>;

    // tf did I invent here....
    // because C++ doesn't seem to support taking a ref of a nested designated initializer
    template<typename T>
    struct StaticRef
    {
        StaticRef(T data) { internalData = data; }
        T internalData;
        operator T*() { return &internalData; }
        operator T const*() const { return &internalData; }
    };
}
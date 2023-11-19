#pragma once

#include "main.hpp"

#include <iostream>
#include <cstring>

#include <SDL2/SDL.h>

namespace Moxaic
{
    static void LogParams()
    {
        std::cout  << '\n';
    }

    template <typename T, typename... Types>
    void LogParams(T param, Types... params)
    {
        std::cout << ' ' << param;
        LogParams(params...);
    }

    template <typename... Types>
    void LogParams(const char* file, const int line, Types... params)
    {
        std::cout << string_Role(Moxaic::Role) << " (" << file << ':' << line << ")";
        LogParams(params...);
    }

    template <typename... Types>
    void LogError(const char* file, const int line, Types... params)
    {
        std::cout << "!!! " << string_Role(Moxaic::Role) << " (" << file << ':' << line << ")";
        LogParams(params...);
    }
}



#define MXC_FILE_NO_PATH (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define MXC_LOG(...) Moxaic::LogParams(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_FUNCTION() Moxaic::LogParams(MXC_FILE_NO_PATH, __LINE__, __FUNCTION__)
#define MXC_LOG_ERROR(...) Moxaic::LogError(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_NAMED(var) std::cout << string_Role(Moxaic::Role) << " (" << MXC_FILE_NO_PATH << ':' << __LINE__ << ") " << #var << " = " << var << '\n';

#define MXC_CHK(command) \
({ \
    if (command != MXC_SUCCESS) [[unlikely]] { \
        printf("(%s:%d) Fail: %s\n", MXC_FILE_NO_PATH, __LINE__, #command); \
        return false; \
    } \
})

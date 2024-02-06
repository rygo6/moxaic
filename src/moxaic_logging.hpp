#pragma once

#include "main.hpp"

#include <cstdio>
#include <cstring>
#include <iostream>

namespace Moxaic
{
    // dude said std:cerr is what you use for logging messages https://twitter.com/christianhujer/status/1726228754081722729
    // and chatgpt4 agreed... so... cerr it is

    // i hate this std stream stuff, replace with c stdio

    void SetConsoleTextDefault();

    void SetConsoleTextRed();

    static void LogParams()
    {
        std::cerr << '\n';
    }

    static void LogParamsMultiline()
    {
        std::cerr << '\n';
    }

    // I really don't know if I like all this std::cerr stuff and recursive templates... has got to be slow

    template<typename T, typename... Types>
    void LogParams(T param, Types... params)
    {
        std::cerr << ' ' << param;
        LogParams(params...);
    }

    template<typename T, typename... Types>
    void LogParamsMultiline(T param, Types... params)
    {
        std::cerr << "\n- " << param;
        LogParamsMultiline(params...);
    }

    template<typename... Types>
    void LogParams(char const* file, int const line, Types... params)
    {
        std::cerr << string_Role(Moxaic::role) << " " << file << ':' << line;
        LogParams(params...);
    }

    template<typename... Types>
    void LogParamsMultiline(char const* file, int const line, Types... params)
    {
        std::cerr << string_Role(Moxaic::role) << " " << file << ':' << line;
        LogParamsMultiline(params...);
    }

    template<typename... Types>
    void LogError(char const* file, int const line, Types... params)
    {
        SetConsoleTextRed();
        std::cerr << "!!! " << string_Role(Moxaic::role) << " " << file << ':' << line;
        LogParams(params...);
        SetConsoleTextDefault();
    }
}// namespace Moxaic

#define MXC_FILE_NO_PATH (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#ifdef MXC_DISABLE_LOG
#define MXC_LOG(...)
#define MXC_LOG_MULTILINE(...)
#define MXC_LOG_FUNCTION()
#define MXC_LOG_ERROR(...)
#define MXC_LOG_NAMED(var)
#else
#define MXC_LOG(...) Moxaic::LogParams(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_MULTILINE(...) Moxaic::LogParamsMultiline(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_FUNCTION() Moxaic::LogParams(MXC_FILE_NO_PATH, __LINE__, __FUNCTION__)
#define MXC_LOG_ERROR(...) Moxaic::LogError(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_NAMED(var) std::cerr << string_Role(Moxaic::role) << " " << MXC_FILE_NO_PATH << ':' << __LINE__ << " " << #var << " = " << var << '\n';
#endif

#define MXC_CHK(command)                                                                                          \
    ({                                                                                                            \
        if (command != MXC_SUCCESS) [[unlikely]] {                                                                \
            printf("%s %s:%d CHECK FAIL: %s\n", string_Role(Moxaic::role), MXC_FILE_NO_PATH, __LINE__, #command); \
            return false;                                                                                         \
        }                                                                                                         \
    })

#pragma once

#include "main.hpp"

#include <cstring>
#include <iostream>

#include <SDL2/SDL.h>

namespace Moxaic
{
    // dude said std:cerr is what you use for logging messages https://twitter.com/christianhujer/status/1726228754081722729
    // and chatgpt4 agreed... so... cerr it is

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
    void LogParams(const char* file, const int line, Types... params)
    {
        std::cerr << string_Role(Moxaic::Role) << " " << file << ':' << line;
        LogParams(params...);
    }

    template<typename... Types>
    void LogParamsMultiline(const char* file, const int line, Types... params)
    {
        std::cerr << string_Role(Moxaic::Role) << " " << file << ':' << line;
        LogParamsMultiline(params...);
    }

    template<typename... Types>
    void LogError(const char* file, const int line, Types... params)
    {
        SetConsoleTextRed();
        std::cerr << "!!! " << string_Role(Moxaic::Role) << " " << file << ':' << line;
        LogParams(params...);
        SetConsoleTextDefault();
    }
}// namespace Moxaic

#define MXC_FILE_NO_PATH (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define MXC_LOG(...) Moxaic::LogParams(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_MULTILINE(...) Moxaic::LogParamsMultiline(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_FUNCTION() Moxaic::LogParams(MXC_FILE_NO_PATH, __LINE__, __FUNCTION__)
#define MXC_LOG_ERROR(...) Moxaic::LogError(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_NAMED(var) std::cerr << string_Role(Moxaic::Role) << " " << MXC_FILE_NO_PATH << ':' << __LINE__ << " " << #var << " = " << var << '\n';

#define MXC_CHK2(command) command


#define MXC_CHK(command)                                                        \
    ({                                                                          \
        if (command != MXC_SUCCESS) [[unlikely]] {                              \
            printf("(%s:%d) Fail: %s\n", MXC_FILE_NO_PATH, __LINE__, #command); \
            return false;                                                       \
        }                                                                       \
    })

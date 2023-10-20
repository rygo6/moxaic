#pragma once

#include <iostream>

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
    void LogParamsWithFileAndLine(const char* file, int line, Types... params)
    {
        std::cout << '(' << file << ':' << line << ")";
        LogParams(params...);
    }
}

#define MXC_FILE_NO_PATH (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define MXC_LOG(...) Moxaic::LogParamsWithFileAndLine(MXC_FILE_NO_PATH, __LINE__, ##__VA_ARGS__)
#define MXC_LOG_NAMED(var) std::cout << '(' << MXC_FILE_NO_PATH << ':' << __LINE__ << ") " << #var << " = " << var << '\n';
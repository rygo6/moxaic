#include "moxaic_logging.hpp"

#include <windows.h>

void Moxaic::SetConsoleTextDefault()
{
    const HANDLE hConsoleOutput = GetStdHandle(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_BLUE);
}

void Moxaic::SetConsoleTextRed()
{
    const HANDLE hConsoleOutput = GetStdHandle(STD_ERROR_HANDLE);
    SetConsoleTextAttribute(hConsoleOutput, FOREGROUND_RED);
}

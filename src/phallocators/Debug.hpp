#pragma once
#include <cstdio>

namespace Debug
{
    void Initialize(FILE* output, bool colorOutput);
    void Debug(const char* module, const char* fmt, ...);
    void Info(const char* module, const char* fmt, ...);
    void Warn(const char* module, const char* fmt, ...);
    void Error(const char* module, const char* fmt, ...);
    void Critical(const char* module, const char* fmt, ...);
}
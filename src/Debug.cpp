#include "Debug.hpp"
#include "Config.hpp"
#include <cstdarg>

namespace
{
    static const char* const g_LogSeverityColors[] =
    {
        [DEBUG_LEVEL_DEBUG]        = "\033[2;37m",
        [DEBUG_LEVEL_INFO]         = "\033[37m",
        [DEBUG_LEVEL_WARN]         = "\033[1;33m",
        [DEBUG_LEVEL_ERROR]        = "\033[1;31m",
        [DEBUG_LEVEL_CRITICAL]     = "\033[1;37;41m",
    };
    
    static const char* const g_ColorReset = "\033[0m";
    FILE* g_OutputFile;
    bool g_Colored;
}

namespace Debug 
{

void Initialize(FILE* fileOut, bool colorOutput)
{
    g_OutputFile = fileOut;
    g_Colored = colorOutput;
}

static void Log(const char* module, int logLevel, const char* fmt, va_list args)
{
    // set color depending on level
    if (g_Colored)
        fprintf(g_OutputFile, g_LogSeverityColors[logLevel]);

    // write module
    fprintf(g_OutputFile, "[%s] ", module);

    // print log
    vfprintf(g_OutputFile, fmt, args);

    // reset color
    if (g_Colored)
        fprintf(g_OutputFile, g_ColorReset);

    fprintf(g_OutputFile, "\n");
}

void Debug(const char* module, const char* fmt, ...)
{
#if DEBUG_LEVEL <= DEBUG_LEVEL_DEBUG
    va_list args;
    va_start(args, fmt);
    Log(module, DEBUG_LEVEL_DEBUG, fmt, args);
    va_end(args);
#endif
}

void Info(const char* module, const char* fmt, ...)
{
#if DEBUG_LEVEL <= DEBUG_LEVEL_INFO
    va_list args;
    va_start(args, fmt);
    Log(module, DEBUG_LEVEL_INFO, fmt, args);
    va_end(args);
#endif
}

void Warn(const char* module, const char* fmt, ...)
{
#if DEBUG_LEVEL <= DEBUG_LEVEL_WARN
    va_list args;
    va_start(args, fmt);
    Log(module, DEBUG_LEVEL_WARN, fmt, args);
    va_end(args);
#endif
}

void Error(const char* module, const char* fmt, ...)
{
#if DEBUG_LEVEL <= DEBUG_LEVEL_ERROR
    va_list args;
    va_start(args, fmt);
    Log(module, DEBUG_LEVEL_ERROR, fmt, args);
    va_end(args);
#endif
}

void Critical(const char* module, const char* fmt, ...)
{
#if DEBUG_LEVEL <= DEBUG_LEVEL_CRITICAL
    va_list args;
    va_start(args, fmt);
    Log(module, DEBUG_LEVEL_CRITICAL, fmt, args);
    va_end(args);
#endif
}

}
#pragma once

#include <time.h>

class Clock
{
public:
    Clock();
    void Start();
    void Stop();
    void Reset();
    double ElapsedSeconds();    

private:
    void AddInterval(timespec& start, timespec& end);
    static timespec Sub(const timespec& a, const timespec& b);
    static timespec Add(const timespec& a, const timespec& b);

    timespec m_Start;
    timespec m_Elapsed;
};
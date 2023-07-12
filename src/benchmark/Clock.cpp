#include "Clock.hpp"

Clock::Clock()
{
    Reset();
}

void Clock::Start()
{
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &m_Start);
}

void Clock::Stop()
{
    timespec end;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
    AddInterval(m_Start, end);
}

void Clock::Reset()
{
    m_Elapsed.tv_sec = 0;
    m_Elapsed.tv_nsec = 0;
}

double Clock::ElapsedSeconds()
{
    return static_cast<double>(m_Elapsed.tv_sec) + 
            static_cast<double>(m_Elapsed.tv_nsec) / 1000000000.0;
}

void Clock::AddInterval(timespec& start, timespec& end)
{
    timespec diff = Sub(end, start);
    m_Elapsed = Add(m_Elapsed, diff);
}

timespec Clock::Sub(const timespec& a, const timespec& b)
{
    timespec temp;
    if (a.tv_nsec - b.tv_nsec < 0) 
    {
        temp.tv_sec = a.tv_sec - b.tv_sec - 1;
        temp.tv_nsec = 1000000000 + a.tv_nsec - b.tv_nsec;
    }
    else
    {
        temp.tv_sec = a.tv_sec - b.tv_sec;
        temp.tv_nsec = a.tv_nsec - b.tv_nsec;
    }
    return temp;
}

timespec Clock::Add(const timespec& a, const timespec& b)
{
    timespec temp;
    if (a.tv_nsec > 1000000000 - b.tv_nsec) 
    {
        temp.tv_sec = a.tv_sec + b.tv_sec + 1;
        temp.tv_nsec = a.tv_nsec - 1000000000 + b.tv_nsec;
    }
    else
    {
        temp.tv_sec = a.tv_sec + b.tv_sec;
        temp.tv_nsec = a.tv_nsec + b.tv_nsec;
    }
    return temp; 
}
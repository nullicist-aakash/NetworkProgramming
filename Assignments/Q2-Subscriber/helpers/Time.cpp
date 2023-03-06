#include "Time.h"

using short_time = std::chrono::_V2::system_clock::time_point;

short_time current_time()
{
    return std::chrono::high_resolution_clock::now();
}

std::string currentDateTime()
{
    time_t     now = std::time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

std::string DateTime(short_time time)
{
    time_t     now = std::chrono::system_clock::to_time_t(time);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
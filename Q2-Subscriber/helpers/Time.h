#pragma once
#include <chrono>
#include <string>

using short_time = std::chrono::_V2::system_clock::time_point;

short_time current_time();
std::string currentDateTime();
std::string DateTime(short_time time);
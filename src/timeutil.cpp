#include "timeutil.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>

namespace timeutil {

static std::tm local_tm(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return tm;
}

std::string format_iso_local(const TP& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    auto tm = local_tm(t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

bool parse_iso_local(const std::string& s, TP& out) {
    std::tm tm{};
    std::istringstream iss(s);
    iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (iss.fail()) return false;

    std::time_t t = std::mktime(&tm); // local time
    if (t == (std::time_t)-1) return false;

    out = std::chrono::system_clock::from_time_t(t);
    return true;
}

TP floor_to_hour(const TP& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    auto tm = local_tm(t);
    tm.tm_min = 0;
    tm.tm_sec = 0;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

TP floor_to_day(const TP& tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    auto tm = local_tm(t);
    tm.tm_hour = 0;
    tm.tm_min  = 0;
    tm.tm_sec  = 0;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

TP start_of_current_year(const TP& now) {
    auto t = std::chrono::system_clock::to_time_t(now);
    auto tm = local_tm(t);
    tm.tm_mon  = 0;
    tm.tm_mday = 1;
    tm.tm_hour = 0;
    tm.tm_min  = 0;
    tm.tm_sec  = 0;
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

} // namespace timeutil

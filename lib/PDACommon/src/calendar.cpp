#include "pda/calendar.hpp"
#include <cstdio>

namespace pda {
bool leapYear(int y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
int daysInMonth(int y, int m) {
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m >= 1 && m <= 12 ? days[m - 1] + (m == 2 && leapYear(y)) : 0;
}
bool parseCivilMinute(const std::string &s, CivilMinute &out) {
    if (s.size() != 16 || s[4] != '-' || s[7] != '-' || s[10] != ' ' || s[13] != ':')
        return false;
    for (std::size_t i = 0; i < s.size(); ++i)
        if (i != 4 && i != 7 && i != 10 && i != 13 && (s[i] < '0' || s[i] > '9'))
            return false;
    CivilMinute t;
    t.year = std::stoi(s.substr(0, 4));
    t.month = std::stoi(s.substr(5, 2));
    t.day = std::stoi(s.substr(8, 2));
    t.hour = std::stoi(s.substr(11, 2));
    t.minute = std::stoi(s.substr(14, 2));
    if (t.year < 2000 || t.year > 2099 || t.month < 1 || t.month > 12 || t.day < 1 ||
        t.day > daysInMonth(t.year, t.month) || t.hour > 23 || t.minute > 59)
        return false;
    out = t;
    return true;
}

// Gregorian civil-date conversion with a March-based year and 400-year eras.
int64_t civilToUnix(const CivilMinute &t, int offset) {
    int y = t.year - (t.month <= 2);
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = static_cast<unsigned>(y - era * 400);
    unsigned doy = (153 * (t.month + (t.month > 2 ? -3 : 9)) + 2) / 5 + t.day - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;
    return days * 86400 + t.hour * 3600 + t.minute * 60 - offset * 60;
}
CivilMinute unixToCivil(int64_t seconds, int offset) {
    seconds += offset * 60;
    int64_t days = seconds / 86400;
    int64_t remainder = seconds % 86400;
    if (remainder < 0) {
        remainder += 86400;
        --days;
    }
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = static_cast<unsigned>(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = static_cast<int>(yoe + era * 400);
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    int day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    int month = static_cast<int>(mp) + (mp < 10 ? 3 : -9);
    return {y + (month <= 2), month, day, static_cast<int>(remainder / 3600),
            static_cast<int>(remainder % 3600 / 60)};
}
std::string formatCivilMinute(const CivilMinute &t) {
    char b[32];
    std::snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d", t.year, t.month, t.day, t.hour,
                  t.minute);
    return b;
}
} // namespace pda

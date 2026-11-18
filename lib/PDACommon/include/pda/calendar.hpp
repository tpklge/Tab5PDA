#pragma once

#include <cstdint>
#include <string>

namespace pda {

struct CivilMinute {
    int year = 2000;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
};

bool leapYear(int year);
int daysInMonth(int year, int month);
bool parseCivilMinute(const std::string &text, CivilMinute &out);
int64_t civilToUnix(const CivilMinute &time, int utcOffsetMinutes);
CivilMinute unixToCivil(int64_t seconds, int utcOffsetMinutes);
std::string formatCivilMinute(const CivilMinute &time);

} // namespace pda

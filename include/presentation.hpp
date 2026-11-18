#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tab5_pda {
enum class TextCase { None, Sentence, Words, Upper };
std::string capitalize(const std::string &, TextCase);
struct Palette {
    const char *id, *name;
    uint32_t background, surface, ink, header, nav, accent, accentText, muted, border;
};
const Palette &palette(const std::string &id);
struct UnitGroup {
    const char *name;
    std::vector<std::string> units;
};
const std::vector<UnitGroup> &unitGroups();
bool scientificCalculate(const std::string &, bool degrees, double &, std::string &);
struct TimeValue {
    double periods = 12, ratePercent = 1, present = 10000, payment = 0, future = 0;
    bool beginning = false;
};
enum class FinancialUnknown { Periods, Rate, Present, Payment, Future };
bool financialCalculate(const TimeValue &, FinancialUnknown, double &, std::string &);
} // namespace tab5_pda

#pragma once
#include <string>
namespace tab5_pda {
enum class InputMask { Date, DateTime, DateBR, DateTimeBR, Period, Money, Unsigned, Signed };
inline std::string dateToBR(const std::string &s) {
    if (s.size() >= 10 && s[4] == '-' && s[7] == '-')
        return s.substr(8, 2) + "/" + s.substr(5, 2) + "/" + s.substr(0, 4) + s.substr(10);
    return s;
}
inline std::string dateToISO(const std::string &s) {
    if (s.size() >= 10 && s[2] == '/' && s[5] == '/')
        return s.substr(6, 4) + "-" + s.substr(3, 2) + "-" + s.substr(0, 2) + s.substr(10);
    return s;
}
// Prefix validation allows an unfinished input; calendar/range validation stays in the service.
inline bool maskedText(InputMask mask, const std::string &raw, std::string &out) {
    out.clear();
    if (mask == InputMask::Date || mask == InputMask::DateTime || mask == InputMask::DateBR ||
        mask == InputMask::DateTimeBR || mask == InputMask::Period) {
        std::string digits;
        for (char c : raw) {
            if (c >= '0' && c <= '9')
                digits += c;
            else if (c != '-' && c != '/' && c != ':' && c != ' ')
                return false;
        }
        const bool br = mask == InputMask::DateBR || mask == InputMask::DateTimeBR;
        const size_t max = mask == InputMask::DateTime || mask == InputMask::DateTimeBR ? 12
                           : mask == InputMask::Period                                  ? 6
                                                                                        : 8;
        if (digits.size() > max)
            return false;
        for (size_t i = 0; i < digits.size(); ++i) {
            if (br && (i == 2 || i == 4))
                out += '/';
            if (!br && (i == 4 || i == 6))
                out += '-';
            if (i == 8)
                out += ' ';
            if (i == 10)
                out += ':';
            out += digits[i];
        }
        return true;
    }
    bool decimal = false;
    int fraction = 0;
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c >= '0' && c <= '9') {
            if (decimal && ++fraction > 2)
                return false;
        } else if (c == '-' && i == 0 && mask != InputMask::Unsigned) {
        } else if (mask == InputMask::Money && (c == '.' || c == ',') && !decimal && i > 0 &&
                   raw[i - 1] >= '0' && raw[i - 1] <= '9') {
            decimal = true;
        } else
            return false;
        out += c;
    }
    return true;
}
} // namespace tab5_pda

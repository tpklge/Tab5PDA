#pragma once
#include "pda/calendar.hpp"
#include "pda/model.hpp"

namespace pda {
struct Field {
    const char *key;
    const char *label;
    std::size_t limit;
};
const std::vector<Field> &recordFields(RecordKind kind);
std::string value(const std::map<std::string, std::string> &fields, const std::string &key,
                  const std::string &fallback = "");
std::string recordValue(const Record &r, const std::string &key);
void setRecordValue(Record &r, const std::string &key, const std::string &text);
bool validateFields(const Record &r, bool draft, std::string &error);
void normalizeRecord(Record &r);
std::size_t fieldBytes(const std::map<std::string, std::string> &fields);
std::size_t estimatedStateBytes(const State &state);
bool parseMoney(const std::string &text, int64_t &cents);
std::string formatMoney(int64_t cents);
std::string displayMinute(const std::string &minute, const Settings &settings);
bool occurrenceOn(const Record &r, const std::string &day, std::string &occurrence);
bool localOccurrenceOn(const Record &r, const std::string &day, int offset,
                       std::string &occurrence);
std::string nextOccurrence(const Record &r, const std::string &from);
struct Alarm {
    uint32_t id;
    std::string occurrence, key;
};
std::vector<Alarm> dueAlarms(const State &state, const std::string &now);
std::map<std::string, int64_t> expenseTotals(const State &state, const std::string &period);
bool convert(const std::string &expression, double &result, std::string &error);
std::string exportJsonl(const State &state, std::string &error);
bool importJsonl(const std::string &text, State &state, std::string &error);
} // namespace pda

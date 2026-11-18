#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace pda {

#ifdef PDA_TARGET_TAB5
constexpr std::size_t kMaxRecords = 1024;
constexpr std::size_t kMaxDetailBytes = 16 * 1024;
constexpr std::size_t kMaxTextBytes = 1024 * 1024;
constexpr std::size_t kMaxSnapshotBytes = 4 * 1024 * 1024;
constexpr std::size_t kMaxResidentBytes = 4 * 1024 * 1024;
constexpr std::size_t kMaxAlarms = 8192;
constexpr std::size_t kMaxPreferences = 20;
#else
constexpr std::size_t kMaxRecords = 128;
constexpr std::size_t kMaxDetailBytes = 768;
constexpr std::size_t kMaxTextBytes = 24 * 1024;
constexpr std::size_t kMaxSnapshotBytes = 32 * 1024;
constexpr std::size_t kMaxResidentBytes = 64 * 1024;
constexpr std::size_t kMaxAlarms = 512;
constexpr std::size_t kMaxPreferences = 12;
#endif
constexpr std::size_t kMaxTitleBytes = 80;

enum class RecordKind : uint8_t {
    Contact = 1,
    Appointment = 2,
    Task = 3,
    Note = 4,
    Anniversary = 5,
    Expense = 6
};

struct Record {
    uint32_t id = 0;
    RecordKind kind = RecordKind::Note;
    std::string title;
    std::string detail;
    std::string when; // Local civil minute: YYYY-MM-DD HH:MM, optional on tasks.
    bool completed = false;
    std::map<std::string, std::string> fields;
};

struct Settings {
    uint8_t brightness = 160;
    uint8_t volume = 100;
    uint16_t screenTimeoutSeconds = 60;
    int16_t utcOffsetMinutes = -240;
    std::string lastClockSetting; // Informational, never restored as current time.
    std::map<std::string, std::string> fields;
};

struct Draft {
    bool active = false;
    uint8_t field = 0;
    Record record;
};

struct State {
    uint32_t nextId = 1;
    Settings settings;
    std::vector<Record> records;
    Draft draft;
    std::map<std::string, std::string>
        alarms; // occurrence key -> dismissed or snoozed civil minute
};

bool trashed(const Record &record);
bool validUtf8(const std::string &text);
bool validRecord(const Record &record, bool draft, std::string &error);
bool validState(const State &state, std::string &error);
std::string searchKey(const std::string &text);
bool matches(const Record &record, const std::string &query);
const Record *findRecord(const State &state, uint32_t id);

} // namespace pda

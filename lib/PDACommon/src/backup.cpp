#include "pda/organizer.hpp"
#include "pda/snapshot_store.hpp"
#include <cstdio>
#include <limits>

namespace pda {
namespace {
using Object = std::map<std::string, std::string>;
std::string quote(const std::string &s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c == '\n')
            out += "\\n";
        else if (c == '\t')
            out += "\\t";
        else
            out += char(c);
    }
    return out + '"';
}
std::string line(const Object &obj) {
    std::string out = "{";
    bool first = true;
    for (const auto &p : obj) {
        if (!first)
            out += ',';
        first = false;
        out += quote(p.first) + ":" + quote(p.second);
    }
    return out + "}\n";
}
bool string(const std::string &s, std::size_t &i, std::string &out) {
    if (i >= s.size() || s[i++] != '"')
        return false;
    out.clear();
    while (i < s.size()) {
        char c = s[i++];
        if (c == '"')
            return validUtf8(out);
        if (c == '\\') {
            if (i >= s.size())
                return false;
            c = s[i++];
            if (c == 'n')
                c = '\n';
            else if (c == 't')
                c = '\t';
            else if (c != '"' && c != '\\' && c != '/')
                return false;
        } else if (static_cast<unsigned char>(c) < 32)
            return false;
        out += c;
    }
    return false;
}
bool object(const std::string &s, Object &out) {
    out.clear();
    std::size_t i = 0;
    auto spaces = [&] {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r'))
            ++i;
    };
    spaces();
    if (i >= s.size() || s[i++] != '{')
        return false;
    spaces();
    while (i < s.size() && s[i] != '}') {
        std::string key, val;
        if (!string(s, i, key))
            return false;
        spaces();
        if (i >= s.size() || s[i++] != ':')
            return false;
        spaces();
        if (!string(s, i, val) || !out.emplace(key, val).second || out.size() > 32)
            return false;
        spaces();
        if (i < s.size() && s[i] == ',') {
            ++i;
            spaces();
            if (i >= s.size() || s[i] == '}')
                return false;
        } else
            break;
    }
    if (i >= s.size() || s[i++] != '}')
        return false;
    spaces();
    return i == s.size();
}
bool number(const Object &o, const char *key, uint64_t max, uint64_t &n) {
    auto it = o.find(key);
    if (it == o.end() || it->second.empty() || it->second.size() > 20)
        return false;
    n = 0;
    for (char c : it->second) {
        if (c < '0' || c > '9' || n > max / 10 ||
            (n == max / 10 && static_cast<uint64_t>(c - '0') > max % 10))
            return false;
        n = n * 10 + c - '0';
    }
    return n <= max;
}
Object record(const Record &r, const char *type) {
    Object out = r.fields;
    out["type"] = type;
    out["id"] = std::to_string(r.id);
    out["kind"] = std::to_string(static_cast<int>(r.kind));
    out["title"] = r.title;
    out["detail"] = r.detail;
    out["when"] = r.when;
    out["completed"] = r.completed ? "1" : "0";
    return out;
}
bool record(Object o, Record &r) {
    uint64_t n;
    if (!number(o, "id", UINT32_MAX, n))
        return false;
    r.id = n;
    if (!number(o, "kind", 6, n) || !n)
        return false;
    r.kind = static_cast<RecordKind>(n);
    if (!number(o, "completed", 1, n))
        return false;
    r.completed = n;
    r.title = value(o, "title");
    r.detail = value(o, "detail");
    r.when = value(o, "when");
    for (const char *k : {"type", "id", "kind", "title", "detail", "when", "completed", "field"})
        o.erase(k);
    r.fields = std::move(o);
    return true;
}
} // namespace
std::string exportJsonl(const State &s, std::string &error) {
    if (!validState(s, error))
        return {};
    std::string out = line({{"format", "pda-backup-v1"},
                            {"schema", "2"},
                            {"records", std::to_string(s.records.size())},
                            {"nextId", std::to_string(s.nextId)}});
    Object settings = s.settings.fields;
    settings["type"] = "settings";
    settings["brightness"] = std::to_string(s.settings.brightness);
    settings["volume"] = std::to_string(s.settings.volume);
    settings["timeout"] = std::to_string(s.settings.screenTimeoutSeconds);
    settings["offset"] = std::to_string(s.settings.utcOffsetMinutes + 720);
    settings["lastClock"] = s.settings.lastClockSetting;
    out += line(settings);
    for (const auto &r : s.records)
        out += line(record(r, "record"));
    if (s.draft.active) {
        auto o = record(s.draft.record, "draft");
        o["field"] = std::to_string(s.draft.field);
        out += line(o);
    }
    for (const auto &p : s.alarms)
        out += line({{"type", "alarm"}, {"key", p.first}, {"until", p.second}});
    char crc[12];
    std::snprintf(crc, sizeof(crc), "%08x",
                  crc32(reinterpret_cast<const uint8_t *>(out.data()), out.size()));
    out += line({{"type", "commit"}, {"crc32", crc}});
    if (out.size() > kMaxSnapshotBytes) {
        error = "Backup excede a capacidade deste aparelho";
        return {};
    }
    return out;
}
bool importJsonl(const std::string &text, State &out, std::string &error) {
    auto fail = [&] {
        error = "Backup inválido, incompleto ou incompatível";
        return false;
    };
    if (text.empty() || text.size() > kMaxSnapshotBytes || text.back() != '\n')
        return fail();
    State s;
    bool header = false, settings = false, committed = false;
    uint64_t expected = 0;
    for (std::size_t i = 0; i < text.size();) {
        auto end = text.find('\n', i);
        if (end == std::string::npos)
            return fail();
        Object o;
        if (!object(text.substr(i, end - i), o))
            return fail();
        uint64_t n;
        if (!header) {
            if (value(o, "format") != "pda-backup-v1" || value(o, "schema") != "2" ||
                o.size() != 4 || !number(o, "nextId", UINT32_MAX, n) || !n)
                return fail();
            s.nextId = n;
            if (!number(o, "records", kMaxRecords, expected))
                return fail();
            header = true;
        } else {
            auto type = value(o, "type");
            if (type == "settings") {
                if (settings || !s.records.empty())
                    return fail();
                settings = true;
                if (!number(o, "brightness", 255, n))
                    return fail();
                s.settings.brightness = n;
                if (!number(o, "volume", 255, n))
                    return fail();
                s.settings.volume = n;
                if (!number(o, "timeout", 3600, n))
                    return fail();
                s.settings.screenTimeoutSeconds = n;
                if (!number(o, "offset", 1560, n))
                    return fail();
                s.settings.utcOffsetMinutes = int(n) - 720;
                s.settings.lastClockSetting = value(o, "lastClock");
                for (const char *k :
                     {"type", "brightness", "volume", "timeout", "offset", "lastClock"})
                    o.erase(k);
                s.settings.fields = std::move(o);
            } else if (type == "record") {
                if (!settings || s.records.size() >= expected)
                    return fail();
                Record r;
                if (!record(o, r) || !validRecord(r, false, error))
                    return fail();
                s.records.push_back(std::move(r));
            } else if (type == "draft") {
                if (!settings || s.draft.active || !number(o, "field", 255, n))
                    return fail();
                s.draft.active = true;
                s.draft.field = n;
                if (!record(o, s.draft.record))
                    return fail();
            } else if (type == "alarm") {
                if (!settings || o.size() != 3 ||
                    !s.alarms.emplace(value(o, "key"), value(o, "until")).second ||
                    s.alarms.size() > kMaxAlarms)
                    return fail();
            } else if (type == "commit") {
                char crc[12];
                std::snprintf(crc, sizeof(crc), "%08x",
                              crc32(reinterpret_cast<const uint8_t *>(text.data()), i));
                if (o.size() != 2 || value(o, "crc32") != crc || end + 1 != text.size())
                    return fail();
                committed = true;
            } else
                return fail();
        }
        if (estimatedStateBytes(s) > kMaxResidentBytes)
            return fail();
        i = end + 1;
    }
    if (!header || !settings || !committed || s.records.size() != expected || !validState(s, error))
        return fail();
    out = std::move(s);
    error.clear();
    return true;
}
} // namespace pda

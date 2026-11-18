#include "pda/snapshot_store.hpp"
#include "pda/organizer.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace pda {
namespace {
constexpr uint8_t magic[] = {'C', 'P', 'D', 'A', 2, 0, 0, 0};
constexpr uint8_t committed[] = {'P', 'D', 'A', 'C', 'O', 'M', 'I', 'T'};
void put(std::vector<uint8_t> &b, uint64_t v, int size) {
    while (size--) {
        b.push_back(v & 255);
        v >>= 8;
    }
}
void text(std::vector<uint8_t> &b, const std::string &s) {
    put(b, s.size(), 2);
    b.insert(b.end(), s.begin(), s.end());
}
void record(std::vector<uint8_t> &b, const Record &r) {
    put(b, r.id, 4);
    put(b, static_cast<uint8_t>(r.kind), 1);
    put(b, r.completed, 1);
    text(b, r.title);
    text(b, r.detail);
    text(b, r.when);
}
void fields(std::vector<uint8_t> &b, const std::map<std::string, std::string> &f) {
    put(b, f.size(), 2);
    for (const auto &p : f) {
        text(b, p.first);
        text(b, p.second);
    }
}
struct Reader {
    const std::vector<uint8_t> &b;
    std::size_t pos, end;
    bool ok = true;
    uint64_t get(int n) {
        if (pos + static_cast<std::size_t>(n) > end) {
            ok = false;
            return 0;
        }
        uint64_t v = 0;
        for (int i = 0; i < n; ++i)
            v |= static_cast<uint64_t>(b[pos++]) << (8 * i);
        return v;
    }
    std::string str(std::size_t max) {
        auto n = get(2);
        if (!ok || n > max || pos + n > end) {
            ok = false;
            return {};
        }
        std::string s(reinterpret_cast<const char *>(b.data() + pos), n);
        pos += n;
        return s;
    }
    std::map<std::string, std::string> fields(std::size_t max,
                                              std::size_t budget = kMaxResidentBytes) {
        std::map<std::string, std::string> out;
        std::size_t used = 0;
        auto n = get(2);
        if (n > max) {
            ok = false;
            return out;
        }
        while (n-- && ok) {
            auto key = str(40), value = str(768);
            used += 96 + key.size() + value.size();
            if (used > budget) {
                ok = false;
                break;
            }
            if (key.empty() || !out.emplace(key, value).second)
                ok = false;
        }
        return out;
    }
    Record rec() {
        Record r;
        r.id = get(4);
        r.kind = static_cast<RecordKind>(get(1));
        auto done = get(1);
        if (done > 1)
            ok = false;
        r.completed = done;
        r.title = str(kMaxTitleBytes);
        r.detail = str(kMaxDetailBytes);
        r.when = str(16);
        return r;
    }
};
} // namespace
uint32_t crc32(const uint8_t *data, std::size_t size) {
    uint32_t crc = 0xffffffffu;
    while (size--) {
        crc ^= *data++;
        for (int i = 0; i < 8; ++i)
            crc =
                (crc >> 1) ^ (0xedb88320u & static_cast<uint32_t>(-static_cast<int32_t>(crc & 1)));
    }
    return ~crc;
}
bool encodeSnapshot(const State &s, uint64_t gen, std::vector<uint8_t> &b, std::string &error) {
    if (!gen || !validState(s, error))
        return false;
    b.clear();
    b.insert(b.end(), std::begin(magic), std::end(magic));
    put(b, gen, 8);
    put(b, 0, 4);
    put(b, s.nextId, 4);
    put(b, s.settings.brightness, 1);
    put(b, s.settings.volume, 1);
    put(b, s.settings.screenTimeoutSeconds, 2);
    put(b, static_cast<uint16_t>(s.settings.utcOffsetMinutes), 2);
    text(b, s.settings.lastClockSetting);
    put(b, s.records.size(), 2);
    for (const auto &r : s.records)
        record(b, r);
    put(b, s.draft.active, 1);
    if (s.draft.active) {
        put(b, s.draft.field, 1);
        record(b, s.draft.record);
    }
    fields(b, s.settings.fields);
    for (const auto &r : s.records)
        fields(b, r.fields);
    if (s.draft.active)
        fields(b, s.draft.record.fields);
    fields(b, s.alarms);
    uint32_t payload = b.size() - 20;
    for (int i = 0; i < 4; ++i)
        b[16 + i] = (payload >> (8 * i)) & 255;
    put(b, crc32(b.data(), b.size()), 4);
    b.insert(b.end(), std::begin(committed), std::end(committed));
    if (b.size() > kMaxSnapshotBytes) {
        error = "Arquivo excede o limite";
        return false;
    }
    return true;
}
DecodeResult decodeSnapshot(const std::vector<uint8_t> &b, State &out, uint64_t &gen) {
    if (b.size() < 32 || b.size() > kMaxSnapshotBytes || std::memcmp(b.data(), magic, 4) != 0)
        return DecodeResult::Corrupt;
    if ((b[4] != 1 && b[4] != 2) || b[5] || b[6] || b[7])
        return DecodeResult::Unsupported;
    Reader header{b, 8, 20};
    auto generation = header.get(8);
    auto size = header.get(4);
    if (!generation || size != b.size() - 32 ||
        std::memcmp(b.data() + b.size() - 8, committed, 8) != 0)
        return DecodeResult::Corrupt;
    Reader checksum{b, b.size() - 12, b.size() - 8};
    if (checksum.get(4) != crc32(b.data(), b.size() - 12))
        return DecodeResult::Corrupt;
    Reader r{b, 20, b.size() - 12};
    State s;
    s.nextId = r.get(4);
    s.settings.brightness = r.get(1);
    s.settings.volume = r.get(1);
    s.settings.screenTimeoutSeconds = r.get(2);
    s.settings.utcOffsetMinutes = static_cast<int16_t>(r.get(2));
    s.settings.lastClockSetting = r.str(16);
    auto count = r.get(2);
    if (count > kMaxRecords)
        return DecodeResult::Corrupt;
    s.records.reserve(count);
    while (count-- && r.ok)
        s.records.push_back(r.rec());
    auto active = r.get(1);
    if (active > 1)
        r.ok = false;
    s.draft.active = active;
    if (s.draft.active) {
        s.draft.field = r.get(1);
        s.draft.record = r.rec();
    }
    if (b[4] == 2 && r.ok) {
        s.settings.fields = r.fields(kMaxPreferences);
        for (auto &record : s.records) {
            record.fields = r.fields(16);
            std::string error;
            if (!validRecord(record, false, error) || estimatedStateBytes(s) > kMaxResidentBytes) {
                r.ok = false;
                break;
            }
        }
        if (s.draft.active)
            s.draft.record.fields = r.fields(16);
        auto used = estimatedStateBytes(s);
        if (used > kMaxResidentBytes)
            r.ok = false;
        else
            s.alarms = r.fields(kMaxAlarms, kMaxResidentBytes - used);
    }
    std::string error;
    if (!r.ok || r.pos != r.end || !validState(s, error))
        return DecodeResult::Corrupt;
    out = std::move(s);
    gen = generation;
    return DecodeResult::Ok;
}
bool SnapshotStore::backup(const State &s) {
    if (!writable_)
        return false;
    std::string error;
    auto json = exportJsonl(s, error);
    if (json.empty()) {
        message_ = error;
        return false;
    }
    std::vector<uint8_t> data(json.begin(), json.end()), verify;
    char archive[80];
    std::snprintf(archive, sizeof(archive), "/CardputerPDA/backup-%016llu.jsonl",
                  static_cast<unsigned long long>(generation_));
    if (!files_.write(archive, data) || files_.read(archive, verify) != ReadResult::Ok ||
        verify != data) {
        message_ = "Cópia versionada não confirmada";
        return false;
    }
    verify.clear();
    const char *path = "/CardputerPDA/backup.jsonl";
    if (!files_.write(path, data) || files_.read(path, verify) != ReadResult::Ok ||
        verify != data) {
        message_ = "Backup não confirmado";
        return false;
    }
    message_ = "Backup verificado no microSD";
    return true;
}
namespace {
bool safeFileName(const std::string &name) {
    return !name.empty() && name.size() <= 80 && name.front() != '.' &&
           name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRST"
                                  "UVWXYZ0123456789-_.") == std::string::npos &&
           name != "state-a.pda" && name != "state-b.pda";
}
} // namespace
bool SnapshotStore::readFile(const std::string &name, std::string &out, ReadResult *result) {
    if (result)
        *result = ReadResult::Error;
    if (!safeFileName(name)) {
        message_ = "Nome de arquivo inválido";
        return false;
    }
    std::vector<uint8_t> data;
    const auto status = files_.read(("/CardputerPDA/" + name).c_str(), data);
    if (result)
        *result = status;
    if (status != ReadResult::Ok) {
        message_ = "Arquivo ausente ou ilegível: " + name;
        return false;
    }
    out.assign(data.begin(), data.end());
    return true;
}
bool SnapshotStore::writeFile(const std::string &name, const std::string &text) {
    if (!writable_ || !safeFileName(name) || text.size() > kMaxSnapshotBytes) {
        message_ = "Arquivo inválido, grande demais ou microSD indisponível";
        return false;
    }
    const auto path = "/CardputerPDA/" + name;
    std::vector<uint8_t> data(text.begin(), text.end()), verify;
    if (!files_.write(path.c_str(), data) || files_.read(path.c_str(), verify) != ReadResult::Ok ||
        data != verify) {
        message_ = "Arquivo não confirmado no microSD";
        return false;
    }
    message_ = "Arquivo verificado: " + name;
    return true;
}
bool SnapshotStore::dailyBackup(const State &state, const std::string &day) {
    CivilMinute date;
    if (!parseCivilMinute(day + " 00:00", date)) {
        message_ = "Backup diário exige data válida";
        return false;
    }
    const auto name = "daily-" + day + ".jsonl";
    std::vector<uint8_t> existing;
    auto result = files_.read(("/CardputerPDA/" + name).c_str(), existing);
    if (result == ReadResult::Error) {
        message_ = "Falha ao consultar backup diário";
        return false;
    }
    if (result == ReadResult::Ok) {
        State checked;
        std::string error;
        if (!importJsonl(std::string(existing.begin(), existing.end()), checked, error)) {
            message_ = "Backup diário existente inválido; preserve o arquivo";
            return false;
        }
        return true;
    }
    std::string error;
    auto text = exportJsonl(state, error);
    if (text.empty()) {
        message_ = error;
        return false;
    }
    return writeFile(name, text);
}
bool SnapshotStore::readBackup(State &s, bool incoming) {
    std::vector<uint8_t> data;
    if (files_.read(incoming ? "/CardputerPDA/incoming.jsonl" : "/CardputerPDA/backup.jsonl",
                    data) != ReadResult::Ok) {
        message_ = "Backup ausente ou ilegível";
        return false;
    }
    std::string error;
    std::string json(data.begin(), data.end());
    std::vector<uint8_t>().swap(data);
    if (!importJsonl(json, s, error)) {
        message_ = error;
        return false;
    }
    return true;
}
const char *SnapshotStore::path(int bank) {
    return bank == 0 ? "/CardputerPDA/state-a.pda" : "/CardputerPDA/state-b.pda";
}
void SnapshotStore::unavailable(const std::string &reason) {
    writable_ = false;
    message_ = reason;
}
bool SnapshotStore::load(State &state) {
    active_ = -1;
    generation_ = 0;
    writable_ = false;
    bool any = false, bad = false, io = false, unsupported = false;
    State chosen;
    for (int bank = 0; bank < 2; ++bank) {
        std::vector<uint8_t> data;
        auto read = files_.read(path(bank), data);
        if (read == ReadResult::Error) {
            io = true;
            continue;
        }
        if (read == ReadResult::Missing)
            continue;
        any = true;
        State candidate;
        uint64_t gen = 0;
        auto decoded = decodeSnapshot(data, candidate, gen);
        if (decoded == DecodeResult::Unsupported) {
            unsupported = true;
            continue;
        }
        if (decoded != DecodeResult::Ok) {
            bad = true;
            continue;
        }
        if (active_ < 0 || gen > generation_) {
            chosen = std::move(candidate);
            generation_ = gen;
            active_ = bank;
        }
    }
    if (active_ >= 0)
        state = std::move(chosen);
    else if (!any && !io)
        state = State{};
    if (unsupported) {
        message_ = "Versão de dados não suportada";
        return false;
    }
    if (io) {
        message_ = "Falha de leitura do microSD";
        return false;
    }
    if (any && active_ < 0) {
        message_ = "Dados inválidos; preserve o cartão";
        return false;
    }
    writable_ = true;
    message_ = bad ? "Cópia anterior recuperada" : "microSD pronto";
    return true;
}
bool SnapshotStore::save(const State &s) {
    if (!writable_) {
        if (message_.empty())
            message_ = "microSD indisponível";
        return false;
    }
    if (generation_ == std::numeric_limits<uint64_t>::max()) {
        message_ = "Limite de versões atingido";
        return false;
    }
    std::vector<uint8_t> data;
    std::string error;
    if (!encodeSnapshot(s, generation_ + 1, data, error)) {
        message_ = error;
        return false;
    }
    int bank = active_ == 0 ? 1 : 0;
    if (!files_.write(path(bank), data)) {
        unavailable("Falha ao gravar no microSD");
        return false;
    }
    const auto expectedSize = data.size();
    const auto expectedCrc = crc32(data.data(), data.size() - 12);
    std::vector<uint8_t>().swap(data); // Avoid retaining two full snapshots on the S3.
    std::vector<uint8_t> verify;
    bool confirmed = files_.read(path(bank), verify) == ReadResult::Ok &&
                     verify.size() == expectedSize &&
                     crc32(verify.data(), verify.size() - 12) == expectedCrc &&
                     std::memcmp(verify.data() + verify.size() - 8, committed, 8) == 0;
    if (confirmed) {
        Reader checksum{verify, verify.size() - 12, verify.size() - 8};
        confirmed = checksum.get(4) == expectedCrc;
    }
    if (!confirmed) {
        unavailable("Gravação não confirmada no microSD");
        return false;
    }
    active_ = bank;
    ++generation_;
    message_ = "Salvo no microSD";
    return true;
}
} // namespace pda

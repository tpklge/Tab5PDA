#include "memory_files.hpp"
#include "pda/organizer.hpp"
#include "pda/service.hpp"
#include <cstring>
#include <fstream>

using namespace pda;
namespace {
void migration() {
    // Hand-built schema 1 fixture: never generated with the new encoder.
    std::vector<uint8_t> b = {'C', 'P', 'D', 'A', 1, 0, 0, 0};
    auto put = [&](uint64_t v, int n) {
        while (n--) {
            b.push_back(v & 255);
            v >>= 8;
        }
    };
    auto str = [&](const std::string &s) {
        put(s.size(), 2);
        b.insert(b.end(), s.begin(), s.end());
    };
    put(7, 8);
    put(0, 4);
    put(2, 4);
    put(160, 1);
    put(100, 1);
    put(60, 2);
    put(uint16_t(-240), 2);
    str("");
    put(1, 2);
    put(1, 4);
    put(4, 1);
    put(0, 1);
    str("Nota antiga");
    str("Acentos: João\nPersistência");
    str("");
    put(0, 1);
    auto payload = b.size() - 20;
    for (int i = 0; i < 4; ++i)
        b[16 + i] = (payload >> (8 * i)) & 255;
    put(crc32(b.data(), b.size()), 4);
    for (char c : std::string("PDACOMIT"))
        b.push_back(c);
    State s;
    uint64_t gen = 0;
    CHECK(decodeSnapshot(b, s, gen) == DecodeResult::Ok);
    CHECK(gen == 7);
    CHECK(s.records[0].detail == "Acentos: João\nPersistência");
    MemoryFiles files;
    files.files[SnapshotStore::path(0)] = b;
    SnapshotStore store(files);
    Service service(store);
    CHECK(service.load());
    auto note = service.state().records[0];
    note.fields["category"] = "Pessoal";
    CHECK(service.saveRecord(note));
    CHECK(files.files[SnapshotStore::path(0)] == b);
    CHECK(files.files[SnapshotStore::path(1)][4] == 2);
    SnapshotStore rebootStore(files);
    Service reboot(rebootStore);
    CHECK(reboot.load());
    CHECK(reboot.state().records[0].fields.at("category") == "Pessoal");
}
void calendarAndAlarms() {
    Record r{1, RecordKind::Appointment, "Mensal", "", "2026-01-31 09:00", false};
    r.fields["repeat"] = "monthly";
    std::string occurrence;
    CHECK(!occurrenceOn(r, "2026-02-28", occurrence));
    CHECK(occurrenceOn(r, "2026-03-31", occurrence));
    CHECK(nextOccurrence(r, "2026-02-01 00:00") == "2026-03-31 09:00");
    r.fields["exceptions"] = "2026-03-31";
    CHECK(!occurrenceOn(r, "2026-03-31", occurrence));
    r.fields.erase("exceptions");
    r.fields["repeat"] = "weekly";
    CHECK(occurrenceOn(r, "2026-02-07", occurrence));
    r.kind = RecordKind::Anniversary;
    r.when = "2024-02-29 09:00";
    r.fields.clear();
    CHECK(occurrenceOn(r, "2026-02-28", occurrence));
    r.fields["leap"] = "skip";
    CHECK(!occurrenceOn(r, "2026-02-28", occurrence));
    CHECK(occurrenceOn(r, "2028-02-29", occurrence));
    r.kind = RecordKind::Appointment;
    r.when = "2026-10-12 10:00";
    r.fields.clear();
    r.fields["reminder"] = "15";
    r.fields["offset"] = "-240";
    State s;
    s.nextId = 2;
    s.records = {r};
    CHECK(dueAlarms(s, "").empty());
    CHECK(dueAlarms(s, "2026-10-12 09:44").empty());
    auto due = dueAlarms(s, "2026-10-12 09:45");
    CHECK(due.size() == 1);
    s.alarms[due[0].key] = "2026-10-12 13:50";
    CHECK(dueAlarms(s, "2026-10-12 09:49").empty());
    CHECK(dueAlarms(s, "2026-10-12 09:50").size() == 1);
    s.alarms[due[0].key] = "dismissed";
    CHECK(dueAlarms(s, "2026-10-12 10:00").empty());
    CHECK(dueAlarms(s, "2026-10-12 09:45").empty());
    s.alarms.clear();
    CHECK(dueAlarms(s, "2026-10-13 09:45").size() == 1);
    CHECK(dueAlarms(s, "2026-10-13 09:46").empty());
    s.settings.utcOffsetMinutes = 0;
    CHECK(dueAlarms(s, "2026-10-12 13:45").size() == 1);
}
void financesAndConverters() {
    int64_t cents;
    CHECK(parseMoney("0.10", cents) && cents == 10);
    CHECK(parseMoney("-12,3", cents) && cents == -1230);
    CHECK(!parseMoney("1.234", cents));
    CHECK(!parseMoney("NaN", cents));
    CHECK(!parseMoney("1,000.00", cents));
    State s;
    Record r{1, RecordKind::Expense, "Café", "", "2026-10-12 08:00", false};
    r.fields = {{"amount", "0.10"}, {"currency", "BRL"}};
    s.records = {r, r, r};
    s.records[2].fields["currency"] = "USD";
    auto totals = expenseTotals(s, "2026-10");
    CHECK(totals["BRL"] == 20 && totals["USD"] == 10);
    double n;
    std::string error;
    CHECK(convert("1;km;m", n, error) && n == 1000);
    CHECK(convert("32;F;C", n, error) && n == 0);
    CHECK(convert("10;USD;BRL;5.25", n, error) && n == 52.5);
    CHECK(!convert("1;kg;m", n, error));
    CHECK(!convert("-1;K;C", n, error));
    CHECK(calculate("200*10%", n, error) && n == 20);
}
void backupAndOccurrence() {
    MemoryFiles files;
    SnapshotStore store(files);
    Service service(store);
    CHECK(service.load());
    Record r{0,    RecordKind::Appointment, "Reunião", "Teste \"JSON\"\nJoão", "2026-10-12 10:00",
             false};
    r.fields = {{"repeat", "daily"}, {"reminder", "15"}, {"location", "Escritório"}};
    CHECK(service.saveRecord(r));
    auto occurrence = *findRecord(service.state(), 1);
    occurrence.id = 0;
    occurrence.when = "2026-10-13 11:00";
    occurrence.fields["repeat"] = "none";
    occurrence.fields["origin"] = "1";
    occurrence.fields["occurrence"] = "2026-10-13";
    files.writeFailure = true;
    CHECK(!service.saveRecord(occurrence));
    CHECK(value(service.state().records[0].fields, "exceptions").empty());
    files.writeFailure = false;
    CHECK(service.reloadForEdit());
    CHECK(service.saveRecord(occurrence));
    CHECK(service.state().records.size() == 2);
    CHECK(value(service.state().records[0].fields, "exceptions") == "2026-10-13");
    CHECK(!service.state().records[1].fields.count("origin"));
    CHECK(service.acknowledgeAlarm("1@2026-10-12 10:00", "dismissed"));
    CHECK(service.backup());
    std::string error;
    auto json = exportJsonl(service.state(), error);
    CHECK(!json.empty());
    State restored;
    CHECK(importJsonl(json, restored, error));
    CHECK(restored.records[0].detail == r.detail);
    CHECK(restored.alarms == service.state().alarms);
    for (std::size_t i = 0; i < json.size(); ++i) {
        State unchanged;
        unchanged.nextId = 42;
        CHECK(!importJsonl(json.substr(0, i), unchanged, error));
        CHECK(unchanged.nextId == 42);
    }
    auto bad = json;
    bad[20] ^= 1;
    CHECK(!importJsonl(bad, restored, error));
    CHECK(service.deleteRecord(2));
    CHECK(service.restoreBackup());
    CHECK(service.state().records.size() == 2);
    files.shortWrite = true;
    CHECK(!service.restoreBackup());
    CHECK(service.state().records.size() == 2);
    auto badSettings = service.state().settings;
    badSettings.fields["city0Offset"] = "bogus";
    CHECK(!service.saveSettings(badSettings));
}
} // namespace
int main(int argc, char **argv) {
    try {
        migration();
        calendarAndAlarms();
        financesAndConverters();
        backupAndOccurrence();
        if (argc > 1) {
            State s;
            s.nextId = 2;
            s.records = {{1, RecordKind::Note, "Fixture USB", "João\nTeste \"JSON\"", "", false}};
            std::string error;
            auto json = exportJsonl(s, error);
            CHECK(!json.empty());
            std::ofstream file(argv[1], std::ios::binary);
            file << json;
            CHECK(file.good());
        }
        std::cout << "Organizer: migration, recurrence, alarms, money, converters and backup OK\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

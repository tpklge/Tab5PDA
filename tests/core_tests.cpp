#include "memory_files.hpp"
#include "pda/calendar.hpp"
#include "pda/service.hpp"
#include <cmath>
#include <limits>

using namespace pda;

void calendarCases() {
    CivilMinute t;
    CHECK(parseCivilMinute("2024-02-29 23:59", t));
    CHECK(!parseCivilMinute("2025-02-29 12:00", t));
    CHECK(!parseCivilMinute("2026-04-31 12:00", t));
    CHECK(!parseCivilMinute("2026-00-12 12:00", t));
    CHECK(!parseCivilMinute("2026-10-12 24:00", t));
    CHECK(!parseCivilMinute("2026-1x-12 12:00", t));
    CHECK(!parseCivilMinute("2026-10-12 12:00 ", t));
    CHECK(parseCivilMinute("2000-01-01 00:00", t));
    CHECK(civilToUnix(t, 0) == 946684800);
    for (int year : {2000, 2024, 2026, 2038, 2099})
        for (int month = 1; month <= 12; ++month)
            for (int offset : {-720, -240, 0, 330, 840}) {
                CivilMinute original{year, month, daysInMonth(year, month), 23, 59};
                auto seconds = civilToUnix(original, offset);
                CHECK(formatCivilMinute(unixToCivil(seconds, offset)) ==
                      formatCivilMinute(original));
                auto next = unixToCivil(seconds + 60, offset);
                CHECK(next.hour == 0 && next.minute == 0 && next.day == 1);
            }
}
void textAndCalculation() {
    CHECK(validUtf8("João, ação e café"));
    CHECK(matches({1, RecordKind::Contact, "João", "São Paulo", "", false}, "JOAO"));
    CHECK(!validUtf8(std::string("\xc0\xaf", 2)));
    CHECK(!validUtf8(std::string("\xed\xa0\x80", 3)));
    CHECK(!validUtf8(std::string("a\0b", 3)));
    CHECK(!validUtf8(std::string("\xf4\x90\x80\x80", 4)));
    double value;
    std::string error;
    CHECK(calculate("(12.5+7.5)*3 - 10/2", value, error));
    CHECK(value == 55);
    CHECK(calculate("-2 * (3 + 4)", value, error));
    CHECK(value == -14);
    CHECK(!calculate("1/0", value, error));
    CHECK(!calculate("nan", value, error));
    CHECK(!calculate("1+", value, error));
    CHECK(!calculate("(1+2", value, error));
    CHECK(!calculate(std::string(40, '(') + "1" + std::string(40, ')'), value, error));
}
void roundtripAndDamage() {
    State s;
    s.nextId = 3;
    s.records = {{1, RecordKind::Note, "Reunião", "Ação\npróxima", "", false},
                 {2, RecordKind::Appointment, "Consulta", "Dentista", "2038-01-19 03:14", false}};
    s.draft = {true, 1, {0, RecordKind::Note, "Rascunho", "Edição interrompida", "", false}};
    std::vector<uint8_t> bytes;
    std::string error;
    CHECK(encodeSnapshot(s, 8, bytes, error));
    State loaded;
    uint64_t gen = 0;
    CHECK(decodeSnapshot(bytes, loaded, gen) == DecodeResult::Ok);
    CHECK(gen == 8);
    CHECK(loaded.records[0].detail == s.records[0].detail);
    CHECK(loaded.draft.record.detail == s.draft.record.detail);
    for (std::size_t len = 0; len < bytes.size(); ++len) {
        std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + len);
        CHECK(decodeSnapshot(truncated, loaded, gen) != DecodeResult::Ok);
    }
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        auto damaged = bytes;
        damaged[i] ^= 0x80;
        CHECK(decodeSnapshot(damaged, loaded, gen) != DecodeResult::Ok);
    }
    bytes.push_back(0);
    CHECK(decodeSnapshot(bytes, loaded, gen) == DecodeResult::Corrupt);
    s.records[0].title = std::string(kMaxTitleBytes + 1, 'a');
    CHECK(!encodeSnapshot(s, 8, bytes, error));
}
void recoveryAndTransactions() {
    MemoryFiles files;
    SnapshotStore store(files);
    Service service(store);
    CHECK(service.load());
    CHECK(service.saveRecord({0, RecordKind::Note, "Primeira", "Texto", "", false}));
    auto first = files.files[SnapshotStore::path(0)];
    CHECK(service.generation() == 1);
    CHECK(service.saveRecord({0, RecordKind::Task, "Segunda", "", "", false}));
    auto second = files.files[SnapshotStore::path(1)];
    CHECK(service.generation() == 2);
    for (std::size_t len = 0; len < second.size(); ++len) {
        MemoryFiles interrupted;
        interrupted.files[SnapshotStore::path(0)] = first;
        interrupted.files[SnapshotStore::path(1)] = {second.begin(), second.begin() + len};
        SnapshotStore retry(interrupted);
        Service recovered(retry);
        CHECK(recovered.load());
        CHECK(recovered.state().records.size() == 1);
        CHECK(recovered.generation() == 1);
        CHECK(recovered.saveRecord({0, RecordKind::Note, "Após recuperação", "", "", false}));
        CHECK(recovered.state().records.size() == 2);
    }
    files.shortWrite = true;
    CHECK(!service.saveRecord({0, RecordKind::Note, "Não salvo", "", "", false}));
    CHECK(service.state().records.size() == 2);
    CHECK(service.state().nextId == 3);
    CHECK(!service.writable());
    files.shortWrite = false;
    CHECK(service.load());
    CHECK(service.state().records.size() == 2);
    files.writeFailure = true;
    auto title = service.state().records[0].title;
    CHECK(!service.deleteRecord(1));
    CHECK(service.state().records[0].title == title);
    files.writeFailure = false;
    CHECK(service.reloadForEdit());
    CHECK(service.toggleTask(2));
    CHECK(findRecord(service.state(), 2)->completed);
    CHECK(!service.toggleTask(1));
    auto task = *findRecord(service.state(), 2);
    task.title = "Título ainda em edição";
    CHECK(service.saveDraft({true, 0, task}));
    CHECK(service.toggleTask(2));
    CHECK(service.state().draft.active);
    CHECK(service.state().draft.record.title == task.title);
    CHECK(service.state().draft.record.completed == findRecord(service.state(), 2)->completed);
    CHECK(service.saveDraft({true, 1, {0, RecordKind::Contact, "João", "123", "", false}}));
    CHECK(service.toggleTask(2));
    CHECK(service.state().draft.active);
    CHECK(service.state().draft.record.title == "João");
    CHECK(service.saveRecord({1, RecordKind::Note, "Primeira", "Texto atualizado", "", false}));
    CHECK(service.state().draft.active);
    SnapshotStore rebootStore(files);
    Service reboot(rebootStore);
    CHECK(reboot.load());
    CHECK(reboot.state().draft.active);
    CHECK(reboot.saveRecord(reboot.state().draft.record));
    CHECK(!reboot.state().draft.active);
    CHECK(!service.reloadForEdit());
    CHECK(!service.writable()); // Another writer/card changed the data.
}
void unknownAndLimits() {
    MemoryFiles files;
    files.files[SnapshotStore::path(0)] = {1, 2, 3};
    SnapshotStore broken(files);
    State state;
    CHECK(!broken.load(state));
    CHECK(!broken.writable());
    CHECK(!broken.save(state));
    CHECK(files.writes == 0);
    MemoryFiles newer;
    std::vector<uint8_t> data;
    std::string error;
    CHECK(encodeSnapshot(State{}, 1, data, error));
    newer.files[SnapshotStore::path(0)] = data;
    data[4] = 3;
    newer.files[SnapshotStore::path(1)] = data;
    SnapshotStore oldFirmware(newer);
    CHECK(!oldFirmware.load(state));
    CHECK(!oldFirmware.writable());
    CHECK(newer.writes == 0);
    newer.readFailure = true;
    SnapshotStore ioFailure(newer);
    CHECK(!ioFailure.load(state));
    CHECK(!ioFailure.writable());
    State full;
    full.nextId = kMaxRecords + 1;
    for (uint32_t id = 1; id <= kMaxRecords; ++id)
        full.records.push_back({id, RecordKind::Note, "N", "", "", false});
    CHECK(validState(full, error));
    full.records.push_back({full.nextId++, RecordKind::Note, "N", "", "", false});
    CHECK(!validState(full, error));
    full = State{};
    full.nextId = 2;
    full.records = {{1, RecordKind::Appointment, "Inválida", "", "2026-02-29 09:00", false}};
    CHECK(!validState(full, error));
}
int main() {
    try {
        calendarCases();
        textAndCalculation();
        roundtripAndDamage();
        recoveryAndTransactions();
        unknownAndLimits();
        std::cout
            << "Core: calendar, Unicode, CRUD, interrupted writes, CRC, schema and limits OK\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

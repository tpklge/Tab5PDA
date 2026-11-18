#include "exchange.hpp"
#include "memory_files.hpp"
#include "schedule.hpp"
#include <iostream>
using namespace pda;
using namespace tab5_pda;
namespace {
void capacity() {
    CHECK(kMaxRecords == 1024 && kMaxDetailBytes == 16384 && kMaxTextBytes == 1024 * 1024);
    State state;
    state.nextId = 1025;
    for (uint32_t i = 1; i <= 1024; ++i)
        state.records.push_back({i, RecordKind::Note, "Nota " + std::to_string(i), "", "", false});
    state.records[0].detail = std::string(kMaxDetailBytes, 'a');
    std::string error;
    CHECK(validState(state, error));
    std::vector<uint8_t> bytes;
    CHECK(encodeSnapshot(state, 13, bytes, error));
    State decoded;
    uint64_t gen = 0;
    CHECK(decodeSnapshot(bytes, decoded, gen) == DecodeResult::Ok);
    CHECK(gen == 13 && decoded.records.size() == 1024 && decoded.records[0].detail.size() == 16384);
    auto json = exportJsonl(state, error);
    CHECK(!json.empty());
    CHECK(importJsonl(json, decoded, error));
    CHECK(decoded.records.size() == 1024);
    decoded.records[0].detail += 'b';
    CHECK(!validState(decoded, error));
    state.records.push_back({state.nextId++, RecordKind::Note, "Excesso", "", "", false});
    CHECK(!validState(state, error));
    state = {};
    state.nextId = 65;
    for (uint32_t i = 1; i <= 64; ++i)
        state.records.push_back({i, RecordKind::Note, "N", std::string(16384, 'x'), "", false});
    CHECK(!validState(state, error)); // Titles also count toward aggregate text.
    state.records.back().detail.resize(15000);
    CHECK(validState(state, error));
}
void tasksTrashAndFiles() {
    MemoryFiles files;
    SnapshotStore store(files);
    Service service(store);
    CHECK(service.load());
    Record task{0, RecordKind::Task, "Compras", "", "2028-01-31 10:00", false};
    task.fields = {{"repeat", "monthly"}, {"project", "Casa"}, {"priority", "1"}};
    CHECK(service.saveRecord(task));
    Record child{0, RecordKind::Task, "Café", "", "", false};
    child.fields["parent"] = "1";
    CHECK(service.saveRecord(child));
    CHECK(!service.toggleTask(1, "2028-01-31 10:00"));
    CHECK(service.toggleTask(2, "2028-01-31 10:00"));
    CHECK(service.toggleTask(1, "2028-01-31 10:00"));
    CHECK(service.state().records[0].when == "2028-03-31 10:00");
    CHECK(!service.state().records[0].completed && !service.state().records[1].completed);
    auto cyclic = service.state();
    cyclic.records[0].fields["parent"] = "2";
    std::string error;
    CHECK(!validState(cyclic, error));
    CHECK(service.trashRecord(2));
    CHECK(service.trashRecord(1));
    CHECK(service.trashRecord(1, true));
    CHECK(!trashed(service.state().records[0]) && trashed(service.state().records[1]));
    CHECK(service.trashRecord(2, true));
    CHECK(service.trashRecord(1));
    CHECK(trashed(service.state().records[1]));
    SnapshotStore rebootStore(files);
    Service reboot(rebootStore);
    CHECK(reboot.load());
    CHECK(reboot.trashRecord(2, true));
    CHECK(!trashed(reboot.state().records[0]));
    CHECK(reboot.trashRecord(1));
    CHECK(reboot.deleteRecord(1));
    CHECK(value(reboot.state().records[0].fields, "parent").empty());
    CHECK(reboot.dailyBackup("2028-03-31"));
    auto backup = files.files.at("/CardputerPDA/daily-2028-03-31.jsonl");
    CHECK(reboot.trashRecord(2, true));
    CHECK(reboot.dailyBackup("2028-03-31"));
    CHECK(files.files.at("/CardputerPDA/daily-2028-03-31.jsonl") == backup);
    CHECK(!reboot.dailyBackup("bad"));
    CHECK(reboot.writeFile("notes.txt", "Título\nTexto"));
    CHECK(!reboot.writeFile("../state-a.pda", "bad"));
    CHECK(!reboot.writeFile("state-a.pda", "bad"));
    files.shortWrite = true;
    CHECK(!reboot.writeFile("broken.txt", "Texto"));
    files.shortWrite = false;
    files.writeFailure = true;
    auto before = reboot.state().records[0];
    CHECK(!reboot.trashRecord(2));
    CHECK(trashed(before) == trashed(reboot.state().records[0]));
}
void exchanges() {
    State state;
    state.settings.utcOffsetMinutes = -240;
    state.nextId = 4;
    Record event{
        1,    RecordKind::Appointment, "Reunião, equipe", "Texto\ncom acentos", "2028-02-29 09:30",
        false};
    event.fields = {
        {"until", "2028-02-29 10:45"}, {"repeat", "weekly"}, {"repeatEnd", "2028-04-30"},
        {"exceptions", "2028-03-07"},  {"reminder", "15"},   {"offset", "-240"}};
    Record contact{2, RecordKind::Contact, "José", "Anotação", "", false};
    contact.fields = {{"phone", "123"}, {"email", "jose@example.com"}, {"address", "Rua, 12"}};
    Record expense{
        3,    RecordKind::Expense, "Café, \"especial\"", "Uma linha\ne outra", "2028-02-29 12:00",
        false};
    expense.fields = {{"amount", "12.30"}, {"currency", "BRL"}, {"category", "Casa"}};
    state.records = {event, contact, expense};
    std::string error;
    std::vector<Record> parsed;
    for (auto format :
         {ExchangeFormat::Calendar, ExchangeFormat::Contacts, ExchangeFormat::Expenses}) {
        auto text = exportRecords(state, format, error);
        CHECK(error.empty() && !text.empty());
        CHECK(parseRecords(text, format, state.settings, parsed, error));
        CHECK(parsed.size() == 1);
        auto expected = format == ExchangeFormat::Calendar   ? event
                        : format == ExchangeFormat::Contacts ? contact
                                                             : expense;
        CHECK(parsed[0].title == expected.title && parsed[0].detail == expected.detail);
        if (format == ExchangeFormat::Calendar) {
            CHECK(parsed[0].when == event.when);
            CHECK(value(parsed[0].fields, "repeat") == "weekly");
            CHECK(value(parsed[0].fields, "exceptions") == "2028-03-07");
            CHECK(value(parsed[0].fields, "reminder") == "15");
        }
    }
    auto all = event;
    all.fields["allDay"] = "1";
    all.fields["repeat"] = "none";
    normalizeRecord(all);
    state.records = {all};
    state.nextId = 2;
    auto text = exportRecords(state, ExchangeFormat::Calendar, error);
    CHECK(parseRecords(text, ExchangeFormat::Calendar, state.settings, parsed, error));
    CHECK(parsed[0].when == all.when &&
          value(parsed[0].fields, "until") == value(all.fields, "until"));
    text = "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nSUMMARY:X\r\nDTSTART;TZID=America/"
           "Cuiaba:20280229T100000\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
    CHECK(!parseRecords(text, ExchangeFormat::Calendar, state.settings, parsed, error));
    CHECK(!parseRecords("BEGIN:VCARD\nFN:incompleto\n", ExchangeFormat::Contacts, state.settings,
                        parsed, error));
    CHECK(!parseRecords("title,when,amount,currency,category,detail\n\"bad\"x,a,b,c,d,e",
                        ExchangeFormat::Expenses, state.settings, parsed, error));
    CHECK(parseRecords("# Nota\nTexto em português", ExchangeFormat::Markdown, state.settings,
                       parsed, error));
    CHECK(parsed[0].title == "Nota");
    MemoryFiles files;
    SnapshotStore store(files);
    Service service(store);
    CHECK(service.load());
    CHECK(mergeRecords(service, parsed, error));
    auto size = service.state().records.size();
    files.writeFailure = true;
    CHECK(!mergeRecords(service, parsed, error));
    CHECK(service.state().records.size() == size);
}
void schedule() {
    State state;
    state.settings.utcOffsetMinutes = -240;
    state.nextId = 2;
    Record event{1, RecordKind::Appointment, "Noite", "", "2028-02-28 23:30", false};
    event.fields = {{"until", "2028-02-29 01:30"}, {"offset", "0"}, {"repeat", "daily"}};
    state.records = {event};
    bool truncated = false;
    auto entries = scheduleEntries(state, "2028-02-28", 1, truncated);
    CHECK(!truncated && entries.size() == 1);
    CHECK(entries[0].start == "2028-02-28 19:30");
    MemoryFiles files;
    SnapshotStore store(files);
    Service service(store);
    CHECK(service.load());
    CHECK(service.restore(state));
    std::string error;
    CHECK(moveAppointment(service, 1, entries[0].occurrence, "2028-03-01 08:00", error));
    CHECK(service.state().records.size() == 2);
    CHECK(value(service.state().records[0].fields, "exceptions") == "2028-02-28");
    CHECK(service.state().records[1].when == "2028-03-01 12:00");
    CHECK(value(service.state().records[1].fields, "until") == "2028-03-01 14:00");
    CHECK(service.trashRecord(2));
    entries = scheduleEntries(service.state(), "2028-03-01", 1, truncated);
    CHECK(entries.size() == 1);
    CHECK(entries[0].id == 1);
}
} // namespace
int main() {
    try {
        capacity();
        tasksTrashAndFiles();
        exchanges();
        schedule();
        std::cout << "Capacity, trash, recurring subtasks, daily backups, interchange and "
                     "scheduling OK\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

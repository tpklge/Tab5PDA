#include "exchange.hpp"
#include "memory_files.hpp"
#include "pda/organizer.hpp"
#include "workspace.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace tab5_pda;
using namespace pda;
int main(int argc, char **argv) {
    try {
        auto wav = pcmHeader(32000);
        uint32_t payload = 0;
        CHECK(pcmSize(wav.data(), 32044, payload) && payload == 32000);
        CHECK(!pcmSize(wav.data(), 32043, payload));
        wav[22] = 2;
        CHECK(!pcmSize(wav.data(), 32044, payload));
        CHECK(safeName("nota-1.txt"));
        CHECK(!safeName("../private"));
        CHECK(!safeName("bad\nfile"));
        CHECK(!publicFile("assistant.cfg") && !publicFile("state-a.pda") &&
              !publicFile("sync-base.jsonl") && !publicFile("draft-1.pda"));
        CHECK(publicFile("sync-export.jsonl") && publicFile("audio-1.wav") &&
              !publicFile("upload.csv.part"));
        MemoryFiles files;
        SnapshotStore store(files);
        Service service(store);
        CHECK(service.load());
        Record note{0, RecordKind::Note, "Caderno", "Texto", "", false};
        note.fields = {{"notebook", "Trabalho"}, {"tags", "reunião,ideia"}, {"favorite", "1"}};
        CHECK(service.saveRecord(note));
        Draft draft;
        draft.active = true;
        draft.record = service.state().records[0];
        draft.record.detail = "Rascunho independente";
        CHECK(service.saveDraft(draft));
        std::string error;
        CHECK(patchRecord(service, 1, "drawing", "drawing-1.draw", error));
        CHECK(service.state().draft.active &&
              value(service.state().draft.record.fields, "drawing") == "drawing-1.draw");
        std::string data;
        CHECK(encodeDraft(service.state().draft, service.state(), data, error));
        Draft recovered;
        CHECK(decodeDraft(data, service.state(), recovered, error));
        CHECK(recovered.record.detail == "Rascunho independente");
        CHECK(patchRecord(service, 1, "drawing", "drawing-new.draw", error));
        CHECK(decodeDraft(data, service.state(), recovered, error));
        CHECK(value(recovered.record.fields, "drawing") == "drawing-new.draw");
        CHECK(recovered.record.detail == "Rascunho independente");
        for (const std::string link : {"abc", "4294967296", "9999999999", ""}) {
            Draft partial;
            partial.active = true;
            partial.record.kind = RecordKind::Task;
            partial.record.fields["parent"] = link;
            std::string saved;
            CHECK(encodeDraft(partial, service.state(), saved, error));
            CHECK(decodeDraft(saved, service.state(), recovered, error));
            CHECK(value(recovered.record.fields, "parent") == link);
        }
        State tasks;
        tasks.nextId = 2;
        tasks.records = {{1, RecordKind::Task, "Recorrente", "", "2026-11-17 09:00", false}};
        Draft taskDraft;
        taskDraft.active = true;
        taskDraft.record = tasks.records[0];
        std::string taskData;
        CHECK(encodeDraft(taskDraft, tasks, taskData, error));
        tasks.records[0].when = "2026-11-18 09:00";
        CHECK(decodeDraft(taskData, tasks, recovered, error));
        CHECK(recovered.record.when == "2026-11-18 09:00");
        data[20] ^= 1;
        CHECK(!decodeDraft(data, service.state(), recovered, error));
        auto settings = service.state().settings;
        settings.fields["quietStart"] = "1320";
        settings.fields["quietEnd"] = "420";
        settings.fields["snooze"] = "15";
        settings.fields["alarmTone"] = "0";
        CHECK(service.saveSettings(settings));
        CHECK(quietNow(settings, "2026-11-17 23:00"));
        CHECK(quietNow(settings, "2026-11-18 06:59"));
        CHECK(!quietNow(settings, "2026-11-18 07:00"));
        CHECK(!quietNow(settings, ""));
        settings.fields["snooze"] = "0";
        CHECK(!service.saveSettings(settings));
        std::vector<InkStroke> strokes{{0x2563eb, 3, {{0, 0}, {899, 419}}}};
        auto ink = encodeInk(strokes);
        std::vector<InkStroke> parsed;
        CHECK(decodeInk(ink, parsed, error));
        CHECK(parsed[0].points[1].x == 899);
        CHECK(!decodeInk("PDA-DRAW-1\n3 0 1 900 10\n", parsed, error));
        CHECK(!decodeInk("PDA-DRAW-1\n3 0 20001\n", parsed, error));
        State base;
        base.records = {{1, RecordKind::Note, "Base", "Texto", "", false}};
        base.nextId = 2;
        State local = base, remote = base;
        local.settings.fields["wifiPass"] = "local-password";
        remote.records[0].detail = "Editado no computador";
        SyncPlan plan;
        CHECK(planSync(local, remote, base, plan, error));
        CHECK(plan.conflicts.empty());
        CHECK(plan.candidate.records[0].detail == "Editado no computador");
        CHECK(value(plan.candidate.settings.fields, "wifiPass") == "local-password");
        local.records[0].detail = "Editado no PDA";
        CHECK(planSync(local, remote, base, plan, error));
        CHECK(plan.conflicts.size() == 1);
        CHECK(resolveSync(plan, 0, false, error));
        CHECK(plan.candidate.records[0].detail == "Editado no PDA");
        CHECK(planSync(local, remote, base, plan, error));
        CHECK(resolveSync(plan, 0, true, error));
        CHECK(plan.candidate.records[0].detail == "Editado no computador");
        State empty;
        CHECK(planSync(local, remote, empty, plan, error));
        CHECK(plan.conflicts.size() == 1); // Same ID created independently.
        CHECK(planSync(base, empty, base, plan, error));
        CHECK(trashed(plan.candidate.records[0]));
        CHECK(planSync(local, empty, base, plan, error));
        CHECK(plan.conflicts.size() == 1 && plan.conflicts[0].deletion);
        State family;
        family.nextId = 2;
        family.records = {{1, RecordKind::Task, "Projeto", "", "", false}};
        State changedFamily = family;
        changedFamily.nextId = 3;
        Record child{2, RecordKind::Task, "Subtarefa nova", "", "", false};
        child.fields["parent"] = "1";
        changedFamily.records.push_back(child);
        CHECK(planSync(changedFamily, empty, family, plan, error));
        CHECK(plan.conflicts.size() == 1);
        CHECK(resolveSync(plan, 0, true, error));
        CHECK(trashed(plan.candidate.records[0]) && trashed(plan.candidate.records[1]));
        CHECK(validState(plan.candidate, error));
        State allocated;
        allocated.nextId = 500;
        CHECK(planSync(local, allocated, empty, plan, error));
        CHECK(plan.candidate.nextId == 500);
        auto exported = syncExport(local, error);
        CHECK(exported.find("local-password") == std::string::npos);
        State checked;
        CHECK(importJsonl(exported, checked, error));
        CHECK(checked.settings.fields.empty());
        if (argc > 1) {
            std::ifstream input(argv[1], std::ios::binary);
            std::string proposal((std::istreambuf_iterator<char>(input)), {});
            CHECK(importJsonl(proposal, checked, error));
            CHECK(checked.records.size() == 1 && checked.records[0].title == "Comprar café");
            CHECK(mergeRecords(service, checked.records, error));
            CHECK(value(service.state().records.back().fields, "offset") ==
                  std::to_string(service.state().settings.utcOffsetMinutes));
        }
        std::cout << "Workspace: metadata, drafts, drawings, quiet hours, sync conflicts and "
                     "assistant framing OK\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

#include "schedule.hpp"
#include <algorithm>
namespace tab5_pda {
using namespace pda;
std::vector<ScheduleEntry> scheduleEntries(const State &state, const std::string &day, int days,
                                           bool &truncated) {
    std::vector<ScheduleEntry> out;
    truncated = false;
    CivilMinute t;
    if (!parseCivilMinute(day + " 00:00", t) || days < 1 || days > 7)
        return out;
    const auto begin = civilToUnix(t, state.settings.utcOffsetMinutes),
               end = begin + days * 86400LL;
    for (const auto &r : state.records) {
        if (trashed(r) || r.kind != RecordKind::Appointment)
            continue;
        CivilMinute a, b;
        if (!parseCivilMinute(r.when, a) || !parseCivilMinute(value(r.fields, "until", r.when), b))
            continue;
        bool allDay = value(r.fields, "allDay") == "1";
        int offset = allDay ? state.settings.utcOffsetMinutes
                            : std::stoi(value(r.fields, "offset",
                                              std::to_string(state.settings.utcOffsetMinutes)));
        auto duration = civilToUnix(b, 0) - civilToUnix(a, 0) + (allDay ? 60 : 0);
        auto from = formatCivilMinute(unixToCivil(begin - duration + (duration ? 60 : 0), offset));
        if (from < "2000-01-01 00:00")
            from = "2000-01-01 00:00";
        for (int count = 0; count < 4096; ++count) {
            auto at = nextOccurrence(r, from);
            if (!parseCivilMinute(at, t))
                break;
            auto start = civilToUnix(t, offset), finish = start + duration;
            if (start >= end)
                break;
            if ((duration && finish > begin) || (!duration && start >= begin))
                out.push_back(
                    {r.id, formatCivilMinute(unixToCivil(start, state.settings.utcOffsetMinutes)),
                     formatCivilMinute(unixToCivil(finish, state.settings.utcOffsetMinutes)), at});
            if (out.size() >= 4096 || count == 4095) {
                truncated = true;
                return out;
            }
            from = formatCivilMinute(unixToCivil(civilToUnix(t, 0) + 60, 0));
        }
    }
    std::sort(out.begin(), out.end(),
              [](const auto &a, const auto &b) { return a.start < b.start; });
    return out;
}
bool moveAppointment(Service &service, uint32_t id, const std::string &occurrence,
                     const std::string &localTarget, std::string &error) {
    auto *r = findRecord(service.state(), id);
    CivilMinute target, a, b, occ;
    if (!r || trashed(*r) || r->kind != RecordKind::Appointment ||
        !parseCivilMinute(localTarget, target) || !parseCivilMinute(r->when, a) ||
        !parseCivilMinute(value(r->fields, "until", r->when), b) ||
        !parseCivilMinute(occurrence, occ)) {
        error = "Compromisso ou horário inválido";
        return false;
    }
    if (service.state().draft.active) {
        error = "Salve ou descarte o rascunho antes de reagendar";
        return false;
    }
    auto edited = *r;
    int offset = std::stoi(
        value(r->fields, "offset", std::to_string(service.state().settings.utcOffsetMinutes)));
    if (value(r->fields, "allDay") == "1") {
        error = "Use Editar para reagendar dia inteiro";
        return false;
    }
    edited.when = formatCivilMinute(
        unixToCivil(civilToUnix(target, service.state().settings.utcOffsetMinutes), offset));
    CivilMinute start;
    parseCivilMinute(edited.when, start);
    edited.fields["until"] = formatCivilMinute(
        unixToCivil(civilToUnix(start, 0) + civilToUnix(b, 0) - civilToUnix(a, 0), 0));
    if (value(r->fields, "repeat", "none") != "none") {
        edited.id = 0;
        edited.fields["repeat"] = "none";
        edited.fields.erase("repeatEnd");
        edited.fields.erase("exceptions");
        edited.fields["origin"] = std::to_string(id);
        edited.fields["occurrence"] = occurrence.substr(0, 10);
    }
    if (!service.saveRecord(edited)) {
        error = service.error();
        return false;
    }
    error.clear();
    return true;
}
} // namespace tab5_pda

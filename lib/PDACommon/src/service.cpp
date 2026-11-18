#include "pda/service.hpp"
#include "pda/organizer.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <set>

namespace pda {
bool Service::load() {
    error_.clear();
    auto ok = store_.load(state_);
    if (!ok)
        error_ = store_.message();
    return ok;
}
bool Service::reloadForEdit() {
    State candidate;
    if (!store_.load(candidate)) {
        error_ = store_.message();
        return false;
    }
    std::vector<uint8_t> before, after;
    std::string error;
    if (!encodeSnapshot(state_, 1, before, error) || !encodeSnapshot(candidate, 1, after, error) ||
        before != after) {
        store_.unavailable("Dados mudaram; recoloque o cartão original");
        error_ = store_.message();
        return false;
    }
    error_.clear();
    return true;
}
bool Service::commit(State &&candidate) {
    if (!validState(candidate, error_))
        return false;
    if (!store_.save(candidate)) {
        error_ = store_.message();
        return false;
    }
    state_ = std::move(candidate);
    error_.clear();
    return true;
}
bool Service::saveRecord(Record record) {
    normalizeRecord(record);
    if (!validRecord(record, false, error_))
        return false;
    State next = state_;
    if (record.kind == RecordKind::Appointment || record.kind == RecordKind::Task ||
        record.kind == RecordKind::Anniversary)
        if (!record.fields.count("offset"))
            record.fields["offset"] = std::to_string(state_.settings.utcOffsetMinutes);
    auto origin = value(record.fields, "origin"), day = value(record.fields, "occurrence");
    if (!origin.empty()) {
        uint32_t id = std::stoul(origin);
        auto series = std::find_if(next.records.begin(), next.records.end(),
                                   [&](const Record &r) { return r.id == id; });
        std::string occurrence;
        if (series == next.records.end() || series->kind != RecordKind::Appointment ||
            value(series->fields, "repeat", "none") == "none" ||
            !occurrenceOn(*series, day, occurrence)) {
            error_ = "Série ou ocorrência inválida";
            return false;
        }
        auto &exceptions = series->fields["exceptions"];
        if (!exceptions.empty())
            exceptions += ",";
        exceptions += day;
        record.fields.erase("origin");
        record.fields.erase("occurrence");
    }
    bool completesDraft = next.draft.active && next.draft.record.id == record.id &&
                          next.draft.record.kind == record.kind;
    if (!record.id) {
        if (next.records.size() >= kMaxRecords ||
            next.nextId == std::numeric_limits<uint32_t>::max()) {
            error_ = "Limite de registros atingido";
            return false;
        }
        record.id = next.nextId++;
        next.records.push_back(std::move(record));
    } else {
        auto it = std::find_if(next.records.begin(), next.records.end(),
                               [&](const Record &r) { return r.id == record.id; });
        if (it == next.records.end()) {
            error_ = "Registro não encontrado";
            return false;
        }
        *it = std::move(record);
    }
    if (completesDraft)
        next.draft = Draft{};
    return commit(std::move(next));
}
bool Service::deleteRecord(uint32_t id) {
    if (!findRecord(state_, id)) {
        error_ = "Registro não encontrado";
        return false;
    }
    State next = state_;
    next.records.erase(std::remove_if(next.records.begin(), next.records.end(),
                                      [&](const Record &r) { return r.id == id; }),
                       next.records.end());
    if (next.draft.active && (next.draft.record.id == id ||
                              value(next.draft.record.fields, "origin") == std::to_string(id)))
        next.draft = Draft{};
    for (auto &r : next.records)
        if (value(r.fields, "contact") == std::to_string(id))
            r.fields.erase("contact");
        else if (value(r.fields, "parent") == std::to_string(id))
            r.fields.erase("parent");
    if (next.draft.active && value(next.draft.record.fields, "contact") == std::to_string(id))
        next.draft.record.fields.erase("contact");
    if (next.draft.active && value(next.draft.record.fields, "parent") == std::to_string(id))
        next.draft.record.fields.erase("parent");
    for (auto i = next.alarms.begin(); i != next.alarms.end();)
        if (i->first.compare(0, std::to_string(id).size() + 1, std::to_string(id) + "@") == 0)
            i = next.alarms.erase(i);
        else
            ++i;
    return commit(std::move(next));
}
bool Service::trashRecord(uint32_t id, bool restore) {
    auto *record = findRecord(state_, id);
    if (!record || trashed(*record) != restore) {
        error_ = "Registro indisponível";
        return false;
    }
    State next = state_;
    const auto group = value(record->fields, "trashRoot", std::to_string(id));
    std::set<uint32_t> affected{id};
    if (restore) {
        auto parent = value(record->fields, "parent");
        if (!parent.empty()) {
            auto *p = findRecord(state_, std::stoul(parent));
            if (p && trashed(*p) && value(p->fields, "trashRoot") != group) {
                error_ = "Restaure a tarefa principal primeiro";
                return false;
            }
        }
        for (const auto &r : next.records)
            if (trashed(r) && value(r.fields, "trashRoot", std::to_string(r.id)) == group)
                affected.insert(r.id);
    }
    for (std::size_t pass = 0; !restore && pass < next.records.size(); ++pass) {
        auto before = affected.size();
        for (const auto &r : next.records) {
            auto parent = value(r.fields, "parent");
            if (!trashed(r) && !parent.empty() && affected.count(std::stoul(parent)))
                affected.insert(r.id);
        }
        if (affected.size() == before)
            break;
    }
    for (auto &r : next.records)
        if (affected.count(r.id)) {
            if (restore) {
                r.fields.erase("trashed");
                r.fields.erase("trashRoot");
            } else {
                r.fields["trashed"] = "1";
                r.fields["trashRoot"] = std::to_string(id);
            }
        }
    if (!restore && next.draft.active && affected.count(next.draft.record.id))
        next.draft = {};
    for (auto it = next.alarms.begin(); it != next.alarms.end();) {
        auto at = it->first.find('@');
        if (!restore && affected.count(std::stoul(it->first.substr(0, at))))
            it = next.alarms.erase(it);
        else
            ++it;
    }
    return commit(std::move(next));
}
bool Service::readFile(const std::string &name, std::string &text, ReadResult *result) {
    if (!store_.readFile(name, text, result)) {
        error_ = store_.message();
        return false;
    }
    error_.clear();
    return true;
}
bool Service::writeFile(const std::string &name, const std::string &text) {
    if (!store_.writeFile(name, text)) {
        error_ = store_.message();
        return false;
    }
    error_.clear();
    return true;
}
bool Service::dailyBackup(const std::string &day) {
    if (!store_.dailyBackup(state_, day)) {
        error_ = store_.message();
        return false;
    }
    error_.clear();
    return true;
}
bool Service::toggleTask(uint32_t id, const std::string &completedAt) {
    auto r = findRecord(state_, id);
    if (!r || trashed(*r) || r->kind != RecordKind::Task) {
        error_ = "Selecione uma tarefa";
        return false;
    }
    if (!r->completed)
        for (const auto &child : state_.records)
            if (!trashed(child) && !child.completed &&
                value(child.fields, "parent") == std::to_string(id)) {
                error_ = "Conclua as subtarefas primeiro";
                return false;
            }
    State next = state_;
    auto it = std::find_if(next.records.begin(), next.records.end(),
                           [&](const Record &record) { return record.id == id; });
    it->completed = !it->completed;
    CivilMinute time;
    if (it->completed && parseCivilMinute(completedAt, time))
        it->fields["completedAt"] = completedAt;
    else
        it->fields.erase("completedAt");
    if (it->completed && value(it->fields, "repeat", "none") != "none") {
        CivilMinute previous;
        if (parseCivilMinute(it->when, previous)) {
            auto from = formatCivilMinute(unixToCivil(civilToUnix(previous, 0) + 60, 0));
            auto upcoming = nextOccurrence(*it, from);
            if (!upcoming.empty()) {
                it->when = upcoming;
                it->completed = false;
                std::set<uint32_t> family{id};
                for (std::size_t pass = 0; pass < next.records.size(); ++pass) {
                    auto before = family.size();
                    for (auto &child : next.records) {
                        const auto parent = value(child.fields, "parent");
                        if (!trashed(child) && !parent.empty() &&
                            family.count(std::stoul(parent))) {
                            family.insert(child.id);
                            child.completed = false;
                            child.fields.erase("completedAt");
                        }
                    }
                    if (before == family.size())
                        break;
                }
            }
        }
    }
    if (next.draft.active && next.draft.record.id == id) {
        if (next.draft.record.when == r->when)
            next.draft.record.when = it->when;
        next.draft.record.completed = it->completed;
        if (it->fields.count("completedAt"))
            next.draft.record.fields["completedAt"] = it->fields["completedAt"];
        else
            next.draft.record.fields.erase("completedAt");
    }
    return commit(std::move(next));
}
bool Service::saveSettings(const Settings &settings) {
    State next = state_;
    next.settings = settings;
    return commit(std::move(next));
}
bool Service::saveDraft(const Draft &draft) {
    State next = state_;
    next.draft = draft;
    return commit(std::move(next));
}
bool Service::clearDraft() {
    return saveDraft(Draft{});
}

bool Service::acknowledgeAlarm(const std::string &key, const std::string &until) {
    State next = state_;
    next.alarms[key] = until;
    return commit(std::move(next));
}
bool Service::skipOccurrence(uint32_t id, const std::string &day) {
    auto r = findRecord(state_, id);
    if (!r || r->kind != RecordKind::Appointment) {
        error_ = "Selecione uma série";
        return false;
    }
    auto edited = *r;
    auto &list = edited.fields["exceptions"];
    if (list.find(day) != std::string::npos)
        return true;
    if (!list.empty())
        list += ",";
    list += day;
    return saveRecord(edited);
}
bool Service::restore(const State &s) {
    State copy = s;
    return commit(std::move(copy));
}
bool Service::backup() {
    error_.clear();
    if (!store_.backup(state_)) {
        error_ = store_.message();
        return false;
    }
    return true;
}
bool Service::restoreBackup(bool incoming) {
    State candidate;
    if (!store_.readBackup(candidate, incoming)) {
        error_ = store_.message();
        return false;
    }
    return commit(std::move(candidate));
}
namespace {
class Calculator {
    const char *cur_;
    bool ok_ = true;
    int depth_ = 0;
    void spaces() {
        while (*cur_ == ' ')
            ++cur_;
    }
    double factor() {
        spaces();
        if (++depth_ > 24) {
            ok_ = false;
            return 0;
        }
        double v = 0;
        if (*cur_ == '+' || *cur_ == '-') {
            char sign = *cur_++;
            v = factor();
            if (sign == '-')
                v = -v;
        } else if (*cur_ == '(') {
            ++cur_;
            v = expression();
            spaces();
            if (*cur_ != ')')
                ok_ = false;
            else
                ++cur_;
        } else {
            char *end = nullptr;
            v = std::strtod(cur_, &end);
            if (end == cur_)
                ok_ = false;
            cur_ = end;
        }
        spaces();
        if (*cur_ == '%') {
            ++cur_;
            v /= 100;
        }
        --depth_;
        return v;
    }
    double term() {
        double v = factor();
        spaces();
        while (ok_ && (*cur_ == '*' || *cur_ == '/')) {
            char op = *cur_++;
            double rhs = factor();
            if (op == '/' && rhs == 0) {
                ok_ = false;
                return 0;
            }
            v = op == '*' ? v * rhs : v / rhs;
            spaces();
        }
        return v;
    }
    double expression() {
        double v = term();
        spaces();
        while (ok_ && (*cur_ == '+' || *cur_ == '-')) {
            char op = *cur_++;
            double rhs = term();
            v = op == '+' ? v + rhs : v - rhs;
            spaces();
        }
        return v;
    }

  public:
    explicit Calculator(const char *s) : cur_(s) {}
    bool run(double &out) {
        out = expression();
        spaces();
        return ok_ && !*cur_ && std::isfinite(out);
    }
};
} // namespace
bool calculate(const std::string &s, double &out, std::string &error) {
    if (s.empty() || s.size() > 96 || !Calculator(s.c_str()).run(out)) {
        error = "Expressão inválida ou divisão por zero";
        return false;
    }
    error.clear();
    return true;
}
} // namespace pda

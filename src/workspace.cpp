#include "workspace.hpp"
#include "pda/organizer.hpp"
#include <algorithm>
#include <cstring>
#include <set>
#include <sstream>
namespace tab5_pda {
std::array<uint8_t, 44> pcmHeader(uint32_t bytes) {
    std::array<uint8_t, 44> out{};
    auto put = [&](unsigned at, uint32_t value, unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            out[at + i] = value >> (8 * i);
    };
    std::memcpy(out.data(), "RIFF", 4);
    put(4, bytes + 36, 4);
    std::memcpy(out.data() + 8, "WAVEfmt ", 8);
    put(16, 16, 4);
    put(20, 1, 2);
    put(22, 1, 2);
    put(24, 16000, 4);
    put(28, 32000, 4);
    put(32, 2, 2);
    put(34, 16, 2);
    std::memcpy(out.data() + 36, "data", 4);
    put(40, bytes, 4);
    return out;
}
bool pcmSize(const uint8_t *header, std::size_t fileBytes, uint32_t &payload) {
    if (fileBytes < 44)
        return false;
    auto get = [&](unsigned at) {
        return uint32_t(header[at]) | (uint32_t(header[at + 1]) << 8) |
               (uint32_t(header[at + 2]) << 16) | (uint32_t(header[at + 3]) << 24);
    };
    payload = get(40);
    const auto expected = pcmHeader(payload);
    return payload % 2 == 0 && std::size_t(payload) + 44 == fileBytes &&
           std::memcmp(header, expected.data(), 44) == 0;
}
bool safeName(const std::string &name) {
    return !name.empty() && name.size() <= 80 && name.front() != '.' &&
           name.find_first_not_of(
               "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") ==
               std::string::npos;
}
bool publicFile(const std::string &name) {
    if (!safeName(name))
        return false;
    if (name.rfind("state-", 0) == 0 || name.rfind("draft-", 0) == 0 || name == "assistant.cfg" ||
        name == "sync-base.jsonl")
        return false;
    auto dot = name.find_last_of('.');
    if (dot == std::string::npos)
        return false;
    auto ext = name.substr(dot);
    return ext == ".txt" || ext == ".md" || ext == ".csv" || ext == ".ics" || ext == ".vcf" ||
           ext == ".draw" || ext == ".wav" || ext == ".jsonl" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".png" || ext == ".bmp";
}
bool quietNow(const pda::Settings &s, const std::string &now) {
    pda::CivilMinute t;
    if (!pda::parseCivilMinute(now, t))
        return false;
    auto a = pda::value(s.fields, "quietStart"), b = pda::value(s.fields, "quietEnd");
    if (a.empty() || b.empty() || a == b)
        return false;
    int start = std::stoi(a), end = std::stoi(b), minute = t.hour * 60 + t.minute;
    return start < end ? minute >= start && minute < end : minute >= start || minute < end;
}
bool patchRecord(pda::Service &service, uint32_t id, const std::string &key,
                 const std::string &value, std::string &error) {
    pda::State next = service.state();
    auto it = std::find_if(next.records.begin(), next.records.end(),
                           [&](const auto &r) { return r.id == id; });
    if (it == next.records.end() || pda::trashed(*it)) {
        error = "Registro indisponível";
        return false;
    }
    pda::setRecordValue(*it, key, value);
    if (next.draft.active && next.draft.record.id == id)
        pda::setRecordValue(next.draft.record, key, value);
    if (!service.restore(next)) {
        error = service.error();
        return false;
    }
    error.clear();
    return true;
}
namespace {
bool linkId(const std::string &text, uint32_t &id) {
    if (text.empty() || text.size() > 10)
        return false;
    uint64_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9')
            return false;
        value = value * 10 + unsigned(c - '0');
        if (value > UINT32_MAX)
            return false;
    }
    id = static_cast<uint32_t>(value);
    return id != 0;
}
} // namespace
bool encodeDraft(const pda::Draft &draft, const pda::State &source, std::string &out,
                 std::string &error) {
    if (!draft.active || !pda::validRecord(draft.record, true, error))
        return false;
    pda::State state;
    state.draft = draft;
    std::set<uint32_t> needed;
    if (draft.record.id)
        needed.insert(draft.record.id);
    for (auto key : {"contact", "parent"}) {
        auto link = pda::value(draft.record.fields, key);
        uint32_t id = 0;
        if (linkId(link, id))
            needed.insert(id);
    }
    for (std::size_t pass = 0; pass < source.records.size(); ++pass) {
        auto before = needed.size();
        for (const auto &r : source.records)
            if (needed.count(r.id))
                for (auto key : {"contact", "parent"}) {
                    auto link = pda::value(r.fields, key);
                    uint32_t id = 0;
                    if (linkId(link, id))
                        needed.insert(id);
                }
        if (before == needed.size())
            break;
    }
    for (const auto &r : source.records)
        if (needed.count(r.id)) {
            state.records.push_back(r);
            state.nextId = std::max(state.nextId, r.id + 1);
        }
    std::vector<uint8_t> bytes;
    if (!pda::encodeSnapshot(state, 1, bytes, error))
        return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}
bool decodeDraft(const std::string &data, const pda::State &current, pda::Draft &out,
                 std::string &error) {
    pda::State state;
    uint64_t generation = 0;
    std::vector<uint8_t> bytes(data.begin(), data.end());
    if (pda::decodeSnapshot(bytes, state, generation) != pda::DecodeResult::Ok ||
        !state.draft.active) {
        error = "Rascunho inválido";
        return false;
    }
    out = state.draft;
    if (out.record.id) {
        auto *r = pda::findRecord(current, out.record.id);
        if (!r || pda::trashed(*r) || r->kind != out.record.kind) {
            error = "Registro original não está disponível";
            return false;
        }
        out.record.completed = r->completed;
        for (auto key : {"audio", "drawing", "completedAt"}) {
            auto value = pda::value(r->fields, key);
            pda::setRecordValue(out.record, key, value);
        }
        const auto *original = pda::findRecord(state, out.record.id);
        if (r->kind == pda::RecordKind::Task && original && out.record.when == original->when)
            out.record.when = r->when;
    }
    return true;
}
std::string encodeInk(const std::vector<InkStroke> &strokes) {
    std::ostringstream out;
    out << "PDA-DRAW-1\n";
    for (const auto &stroke : strokes) {
        out << unsigned(stroke.width) << ' ' << stroke.color << ' ' << stroke.points.size();
        for (auto p : stroke.points)
            out << ' ' << p.x << ' ' << p.y;
        out << '\n';
    }
    return out.str();
}
bool decodeInk(const std::string &data, std::vector<InkStroke> &out, std::string &error) {
    std::istringstream in(data);
    std::string magic;
    std::getline(in, magic);
    out.clear();
    if (magic != "PDA-DRAW-1") {
        error = "Desenho inválido";
        return false;
    }
    std::size_t total = 0;
    for (std::string line; std::getline(in, line);) {
        std::istringstream row(line);
        unsigned width = 0, color = 0;
        std::size_t count = 0;
        if (!(row >> width >> color >> count) || width < 1 || width > 16 || color > 0xffffff ||
            count > 20000 - total) {
            error = "Traço inválido";
            out.clear();
            return false;
        }
        InkStroke stroke;
        stroke.width = width;
        stroke.color = color;
        for (std::size_t i = 0; i < count; ++i) {
            int x, y;
            if (!(row >> x >> y) || x < 0 || x >= 900 || y < 0 || y >= 420) {
                error = "Ponto inválido";
                out.clear();
                return false;
            }
            stroke.points.push_back({int16_t(x), int16_t(y)});
        }
        std::string extra;
        if (row >> extra) {
            error = "Dados extras no desenho";
            out.clear();
            return false;
        }
        total += count;
        out.push_back(std::move(stroke));
        if (out.size() > 2048) {
            error = "Muitos traços";
            out.clear();
            return false;
        }
    }
    error.clear();
    return true;
}
namespace {
bool equal(const pda::Record &a, const pda::Record &b) {
    return a.id == b.id && a.kind == b.kind && a.title == b.title && a.detail == b.detail &&
           a.when == b.when && a.completed == b.completed && a.fields == b.fields;
}
bool changedDescendant(const pda::State &local, const pda::State &base, uint32_t id) {
    for (const auto &r : local.records) {
        auto parent = pda::value(r.fields, "parent");
        for (std::size_t depth = 0; !parent.empty() && depth < local.records.size(); ++depth) {
            auto p = static_cast<uint32_t>(std::stoul(parent));
            if (p == id) {
                auto *old = pda::findRecord(base, r.id);
                if (!old || !equal(r, *old))
                    return true;
                break;
            }
            auto *record = pda::findRecord(local, p);
            if (!record)
                break;
            parent = pda::value(record->fields, "parent");
        }
    }
    return false;
}
void trashChildren(pda::State &state) {
    for (std::size_t pass = 0; pass < state.records.size(); ++pass) {
        bool changed = false;
        for (auto &r : state.records) {
            if (pda::trashed(r))
                continue;
            auto parent = pda::value(r.fields, "parent");
            if (parent.empty())
                continue;
            auto *p = pda::findRecord(state, std::stoul(parent));
            if (p && pda::trashed(*p)) {
                r.fields["trashed"] = "1";
                r.fields["trashRoot"] = pda::value(p->fields, "trashRoot", std::to_string(p->id));
                changed = true;
            }
        }
        if (!changed)
            break;
    }
}
void replace(pda::State &s, const pda::Record &r) {
    auto it = std::find_if(s.records.begin(), s.records.end(),
                           [&](const auto &v) { return v.id == r.id; });
    if (it == s.records.end())
        s.records.push_back(r);
    else
        *it = r;
    s.nextId = std::max(s.nextId, r.id + 1);
}
} // namespace
bool planSync(const pda::State &local, const pda::State &remote, const pda::State &base,
              SyncPlan &plan, std::string &error) {
    if (!pda::validState(local, error) || !pda::validState(remote, error) ||
        !pda::validState(base, error))
        return false;
    plan = {};
    plan.candidate = local;
    plan.candidate.nextId = std::max({local.nextId, remote.nextId, base.nextId});
    for (const auto &r : remote.records) {
        auto *l = pda::findRecord(local, r.id), *b = pda::findRecord(base, r.id);
        if (l && equal(*l, r))
            continue;
        if (!l) {
            if (b && !equal(r, *b))
                plan.conflicts.push_back({r.id, r, false});
            else if (!b)
                replace(plan.candidate, r);
            continue;
        }
        if (b && equal(*l, *b) && !(pda::trashed(r) && changedDescendant(local, base, r.id)))
            replace(plan.candidate, r);
        else if (!b || !equal(r, *b))
            plan.conflicts.push_back({r.id, r, false});
    }
    for (const auto &b : base.records)
        if (!pda::findRecord(remote, b.id)) {
            auto *l = pda::findRecord(local, b.id);
            if (!l)
                continue;
            auto deleted = *l;
            deleted.fields["trashed"] = "1";
            deleted.fields["trashRoot"] = std::to_string(l->id);
            if (equal(*l, b) && !changedDescendant(local, base, b.id))
                replace(plan.candidate, deleted);
            else
                plan.conflicts.push_back({l->id, deleted, true});
        }
    trashChildren(plan.candidate);
    if (plan.candidate.records.size() > pda::kMaxRecords) {
        error = "Sincronização excede a capacidade";
        return false;
    }
    error.clear();
    return true;
}
bool resolveSync(SyncPlan &plan, std::size_t index, bool remote, std::string &error) {
    if (index >= plan.conflicts.size()) {
        error = "Conflito indisponível";
        return false;
    }
    if (remote) {
        replace(plan.candidate, plan.conflicts[index].remote);
        trashChildren(plan.candidate);
    }
    plan.conflicts.erase(plan.conflicts.begin() + index);
    error.clear();
    return true;
}
std::string syncExport(const pda::State &source, std::string &error) {
    pda::State state;
    state.records = source.records;
    state.nextId = source.nextId;
    return pda::exportJsonl(state, error);
}
} // namespace tab5_pda

#include "pda/model.hpp"
#include "pda/calendar.hpp"
#include "pda/organizer.hpp"
#include <algorithm>
#include <limits>
#include <set>

namespace pda {
bool trashed(const Record &r) {
    return value(r.fields, "trashed") == "1";
}
bool validUtf8(const std::string &s) {
    for (std::size_t i = 0; i < s.size();) {
        uint8_t c = static_cast<uint8_t>(s[i++]);
        if (c < 0x80) {
            if (c == 0 || (c < 0x20 && c != '\n' && c != '\t'))
                return false;
            continue;
        }
        int count;
        uint32_t value, minimum;
        if (c >= 0xc2 && c <= 0xdf) {
            count = 1;
            value = c & 31;
            minimum = 0x80;
        } else if (c >= 0xe0 && c <= 0xef) {
            count = 2;
            value = c & 15;
            minimum = 0x800;
        } else if (c >= 0xf0 && c <= 0xf4) {
            count = 3;
            value = c & 7;
            minimum = 0x10000;
        } else
            return false;
        while (count--) {
            if (i >= s.size())
                return false;
            c = static_cast<uint8_t>(s[i++]);
            if ((c & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (c & 63);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}
bool validRecord(const Record &r, bool draft, std::string &error) {
    auto fail = [&](const char *e) {
        error = e;
        return false;
    };
    auto kind = static_cast<uint8_t>(r.kind);
    if (kind < 1 || kind > 6)
        return fail("Tipo de registro inválido");
    if (r.title.size() > kMaxTitleBytes || r.detail.size() > kMaxDetailBytes || r.when.size() > 16)
        return fail("Texto excede o limite");
    if (!validUtf8(r.title) || !validUtf8(r.detail) || !validUtf8(r.when))
        return fail("Texto UTF-8 inválido");
    if (r.title.find('\n') != std::string::npos || r.title.find('\t') != std::string::npos)
        return fail("Título deve ter uma linha");
    if (!validateFields(r, draft, error))
        return false;
    if (draft)
        return true;
    if (r.title.find_first_not_of(' ') == std::string::npos)
        return fail("Informe um título");
    if ((r.kind == RecordKind::Appointment || r.kind == RecordKind::Anniversary ||
         r.kind == RecordKind::Expense) &&
        r.when.empty())
        return fail("Informe a data e hora");
    CivilMinute time;
    if (!r.when.empty() && !parseCivilMinute(r.when, time))
        return fail("Use AAAA-MM-DD HH:MM");
    if ((r.kind == RecordKind::Contact || r.kind == RecordKind::Note) && !r.when.empty())
        return fail("Data não se aplica a este registro");
    return true;
}
bool validState(const State &s, std::string &error) {
    auto fail = [&](const char *e) {
        error = e;
        return false;
    };
    if (s.records.size() > kMaxRecords || s.nextId == 0)
        return fail("Limite de registros atingido");
    if (s.settings.brightness < 16 || s.settings.screenTimeoutSeconds > 3600 ||
        s.settings.utcOffsetMinutes < -720 || s.settings.utcOffsetMinutes > 840)
        return fail("Configuração inválida");
    CivilMinute time;
    if (!s.settings.lastClockSetting.empty() &&
        !parseCivilMinute(s.settings.lastClockSetting, time))
        return fail("Ajuste de hora inválido");
    std::size_t bytes = fieldBytes(s.settings.fields) + fieldBytes(s.alarms);
    if (s.settings.fields.size() > kMaxPreferences || s.alarms.size() > kMaxAlarms)
        return fail("Limite de preferências ou lembretes atingido");
    for (const auto &p : s.settings.fields) {
        if (p.first.size() > 24 || p.second.size() > 128 || !validUtf8(p.first) ||
            !validUtf8(p.second))
            return fail("Preferência inválida");
    }
    const std::set<std::string> preferenceKeys = {
        "pin",    "salt",       "lockSeconds", "wifiSSID",    "wifiPass",    "city0",
        "city1",  "city2",      "city0Offset", "city1Offset", "city2Offset", "dateFormat",
        "snooze", "quietStart", "quietEnd",    "alarmTone",   "theme"};
    for (const auto &p : s.settings.fields)
        if (!preferenceKeys.count(p.first))
            return fail("Preferência desconhecida");
    auto theme = value(s.settings.fields, "theme", "sereno");
    if (theme != "sereno" && theme != "areia" && theme != "ardosia")
        return fail("Tema desconhecido");
    if (value(s.settings.fields, "wifiSSID").size() > 32 ||
        value(s.settings.fields, "wifiPass").size() > 63)
        return fail("Credenciais Wi-Fi longas demais");
    for (int i = 0; i < 3; ++i) {
        auto v = value(s.settings.fields, "city" + std::to_string(i) + "Offset", "0");
        std::size_t start = v.size() && v[0] == '-' ? 1 : 0;
        if (v.size() > 4 || v.size() == start ||
            v.substr(start).find_first_not_of("0123456789") != std::string::npos ||
            std::stoi(v) < -720 || std::stoi(v) > 840)
            return fail("Fuso mundial inválido");
        if (value(s.settings.fields, "city" + std::to_string(i)).size() > 40)
            return fail("Cidade longa demais");
    }
    for (auto key : {"snooze", "quietStart", "quietEnd", "alarmTone"}) {
        auto v = value(s.settings.fields, key);
        if (v.empty())
            continue;
        if (v.size() > 4 || v.find_first_not_of("0123456789") != std::string::npos)
            return fail("Preferência de aviso inválida");
        auto n = std::stoul(v);
        if ((std::string(key) == "snooze" && (n < 1 || n > 1440)) ||
            ((std::string(key) == "quietStart" || std::string(key) == "quietEnd") && n > 1439) ||
            (std::string(key) == "alarmTone" && n > 4000))
            return fail("Aviso fora do intervalo");
    }
    auto format = value(s.settings.fields, "dateFormat", "ISO");
    if (format != "ISO" && format != "BR")
        return fail("Formato de data inválido");
    auto pin = value(s.settings.fields, "pin");
    if (!pin.empty() &&
        (pin.size() != 64 || pin.find_first_not_of("0123456789abcdef") != std::string::npos))
        return fail("PIN inválido");
    auto salt = value(s.settings.fields, "salt");
    if (!pin.empty() &&
        (salt.size() != 32 || salt.find_first_not_of("0123456789abcdef") != std::string::npos))
        return fail("Salt inválido");
    auto lock = value(s.settings.fields, "lockSeconds", "120");
    if (lock.empty() || lock.size() > 4 ||
        lock.find_first_not_of("0123456789") != std::string::npos || std::stoul(lock) > 3600)
        return fail("Tempo de bloqueio inválido");
    for (const auto &p : s.alarms) {
        auto at = p.first.find('@');
        if (at == std::string::npos || at == 0 || at > 10 ||
            p.first.substr(0, at).find_first_not_of("0123456789") != std::string::npos ||
            !parseCivilMinute(p.first.substr(at + 1), time) ||
            (p.second != "dismissed" && !parseCivilMinute(p.second, time)))
            return fail("Estado de alarme inválido");
        uint64_t id = 0;
        for (std::size_t i = 0; i < at; ++i)
            id = id * 10 + (p.first[i] - '0');
        if (!id || id > UINT32_MAX || !findRecord(s, static_cast<uint32_t>(id)))
            return fail("Lembrete sem registro original");
    }
    std::size_t resident = estimatedStateBytes(s);
    std::set<uint32_t> ids;
    for (const auto &r : s.records) {
        if (!r.id || r.id >= s.nextId || !ids.insert(r.id).second)
            return fail("Identificador inválido");
        if (!validRecord(r, false, error))
            return false;
        if (r.fields.count("origin") || r.fields.count("occurrence"))
            return fail("Ocorrência ainda não confirmada");
        auto link = value(r.fields, "contact");
        if (!link.empty()) {
            auto contact = findRecord(s, std::stoul(link));
            if (!contact || contact->kind != RecordKind::Contact)
                return fail("Contato vinculado não encontrado");
        }
        auto parent = value(r.fields, "parent");
        std::set<uint32_t> ancestors{r.id};
        while (!parent.empty()) {
            auto id = static_cast<uint32_t>(std::stoul(parent));
            auto *p = findRecord(s, id);
            if (!p || p->kind != RecordKind::Task || (!trashed(r) && trashed(*p)) ||
                !ancestors.insert(id).second)
                return fail("Subtarefa com pai inválido ou ciclo");
            parent = value(p->fields, "parent");
        }
        bytes += r.title.size() + r.detail.size() + r.when.size() + fieldBytes(r.fields);
    }
    if (s.draft.active) {
        if (s.draft.field >= recordFields(s.draft.record.kind).size() ||
            !validRecord(s.draft.record, true, error))
            return fail("Rascunho inválido");
        if (s.draft.record.id && !findRecord(s, s.draft.record.id))
            return fail("Rascunho sem registro original");
        bytes += s.draft.record.title.size() + s.draft.record.detail.size() +
                 s.draft.record.when.size() + fieldBytes(s.draft.record.fields);
    }
    if (resident > kMaxResidentBytes)
        return fail("Limite de memória da agenda: exporte ou exclua registros");
    return bytes <= kMaxTextBytes || fail("Agenda cheia: exporte ou exclua registros");
}
std::string searchKey(const std::string &text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        uint8_t c = static_cast<uint8_t>(text[i]);
        if (c == 0xc3 && i + 1 < text.size()) {
            uint8_t d = static_cast<uint8_t>(text[++i]);
            if ((d >= 0x80 && d <= 0x85) || (d >= 0xa0 && d <= 0xa5))
                out += 'a';
            else if (d == 0x87 || d == 0xa7)
                out += 'c';
            else if ((d >= 0x88 && d <= 0x8b) || (d >= 0xa8 && d <= 0xab))
                out += 'e';
            else if ((d >= 0x8c && d <= 0x8f) || (d >= 0xac && d <= 0xaf))
                out += 'i';
            else if ((d >= 0x92 && d <= 0x96) || (d >= 0xb2 && d <= 0xb6))
                out += 'o';
            else if ((d >= 0x99 && d <= 0x9c) || (d >= 0xb9 && d <= 0xbc))
                out += 'u';
            else {
                out += static_cast<char>(c);
                out += static_cast<char>(d);
            }
        } else
            out += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
    }
    return out;
}
bool matches(const Record &r, const std::string &q) {
    auto key = searchKey(q);
    for (const auto &p : r.fields)
        if (searchKey(p.second).find(key) != std::string::npos)
            return true;
    return key.empty() || searchKey(r.title).find(key) != std::string::npos ||
           searchKey(r.detail).find(key) != std::string::npos ||
           r.when.find(key) != std::string::npos;
}
const Record *findRecord(const State &s, uint32_t id) {
    auto it = std::find_if(s.records.begin(), s.records.end(),
                           [&](const Record &r) { return r.id == id; });
    return it == s.records.end() ? nullptr : &*it;
}
} // namespace pda

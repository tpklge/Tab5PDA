#include "pda/organizer.hpp"
#include "pda/service.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <set>

namespace pda {
const std::vector<Field> &recordFields(RecordKind k) {
    static const std::vector<Field> contact = {
        {"title", "Nome", 80},        {"detail", "Observações", kMaxDetailBytes},
        {"company", "Empresa", 80},   {"phone", "Telefones", 120},
        {"email", "E-mail", 120},     {"address", "Endereço", 240},
        {"category", "Categoria", 40}};
    static const std::vector<Field> appointment = {
        {"title", "Título", 80},
        {"detail", "Observações", kMaxDetailBytes},
        {"when", "Início: AAAA-MM-DD HH:MM", 16},
        {"until", "Fim: AAAA-MM-DD HH:MM", 16},
        {"location", "Local", 120},
        {"category", "Categoria", 40},
        {"reminder", "Aviso: minutos antes (-1 desliga)", 5},
        {"repeat", "Repetir: none/daily/weekly/monthly/yearly", 7},
        {"repeatEnd", "Até: AAAA-MM-DD (opcional)", 10},
        {"allDay", "Dia inteiro: 0 não / 1 sim", 1}};
    static const std::vector<Field> task = {
        {"title", "Título", 80},
        {"detail", "Observações", kMaxDetailBytes},
        {"when", "Prazo: AAAA-MM-DD HH:MM", 16},
        {"category", "Categoria", 40},
        {"priority", "Prioridade: 1 alta / 2 média / 3 baixa", 1},
        {"reminder", "Aviso: minutos antes (-1 desliga)", 5},
        {"project", "Projeto / lista", 40},
        {"parent", "Tarefa principal (ID opcional)", 10},
        {"repeat", "Repetir: none/daily/weekly/monthly/yearly", 7},
        {"repeatEnd", "Até: AAAA-MM-DD (opcional)", 10}};
    static const std::vector<Field> note = {{"title", "Título", 80},
                                            {"detail", "Texto / observações", kMaxDetailBytes},
                                            {"category", "Categoria", 40}
#ifdef PDA_TARGET_TAB5
                                            ,
                                            {"notebook", "Caderno / pasta", 40},
                                            {"tags", "Etiquetas", 120},
                                            {"favorite", "Favorita", 1}
#endif
    };
    static const std::vector<Field> anniversary = {
        {"title", "Nome / aniversário", 80},
        {"detail", "Observações", kMaxDetailBytes},
        {"when", "Data base: AAAA-MM-DD HH:MM", 16},
        {"category", "Categoria", 40},
        {"contact", "ID do contato (opcional)", 10},
        {"reminder", "Aviso: minutos antes (-1 desliga)", 5},
        {"leap", "29/02: feb28 ou skip", 5},
        {"birthYear", "Ano de nascimento (opcional)", 4}};
    static const std::vector<Field> expense = {
        {"title", "Descrição", 80},
        {"detail", "Observações", kMaxDetailBytes},
        {"when", "Data: AAAA-MM-DD HH:MM", 16},
        {"category", "Categoria", 40},
        {"amount", "Valor: 123.45 (sem separador de milhar)", 16},
        {"currency", "Moeda: BRL / USD / EUR", 3}};
    switch (k) {
    case RecordKind::Contact:
        return contact;
    case RecordKind::Appointment:
        return appointment;
    case RecordKind::Task:
        return task;
    case RecordKind::Anniversary:
        return anniversary;
    case RecordKind::Expense:
        return expense;
    default:
        return note;
    }
}
std::string value(const std::map<std::string, std::string> &f, const std::string &k,
                  const std::string &fallback) {
    auto i = f.find(k);
    return i == f.end() || i->second.empty() ? fallback : i->second;
}
std::string recordValue(const Record &r, const std::string &k) {
    if (k == "title")
        return r.title;
    if (k == "detail")
        return r.detail;
    if (k == "when")
        return r.when;
    return value(r.fields, k);
}
void setRecordValue(Record &r, const std::string &k, const std::string &s) {
    if (k == "title")
        r.title = s;
    else if (k == "detail")
        r.detail = s;
    else if (k == "when")
        r.when = s;
    else if (s.empty())
        r.fields.erase(k);
    else
        r.fields[k] = s;
}
std::size_t fieldBytes(const std::map<std::string, std::string> &f) {
    std::size_t n = 0;
    for (const auto &p : f)
        n += p.first.size() + p.second.size();
    return n;
}
std::size_t estimatedStateBytes(const State &s) {
    std::size_t total = 256 + fieldBytes(s.settings.fields) + fieldBytes(s.alarms) +
                        (s.settings.fields.size() + s.alarms.size()) * 96;
    auto record = [&](const Record &r) {
        total += 160 + r.title.size() + r.detail.size() + r.when.size() + fieldBytes(r.fields) +
                 r.fields.size() * 96;
    };
    for (const auto &r : s.records)
        record(r);
    if (s.draft.active)
        record(s.draft.record);
    return total;
}
bool parseMoney(const std::string &s, int64_t &out) {
    if (s.empty() || s.size() > 16)
        return false;
    std::size_t i = 0;
    bool neg = s[0] == '-';
    if (neg)
        ++i;
    int64_t whole = 0;
    int digits = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        whole = whole * 10 + s[i++] - '0';
        ++digits;
    }
    if (!digits)
        return false;
    int fraction = 0, count = 0;
    if (i < s.size() && (s[i] == '.' || s[i] == ',')) {
        ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            if (++count > 2)
                return false;
            fraction = fraction * 10 + s[i++] - '0';
        }
        if (!count)
            return false;
        if (count == 1)
            fraction *= 10;
    }
    if (i != s.size() || whole > 1000000000000LL)
        return false;
    out = (whole * 100 + fraction) * (neg ? -1 : 1);
    return true;
}
std::string formatMoney(int64_t cents) {
    char b[40];
    uint64_t n = cents < 0 ? uint64_t(-(cents + 1)) + 1 : uint64_t(cents);
    std::snprintf(b, sizeof(b), "%s%llu.%02llu", cents < 0 ? "-" : "",
                  (unsigned long long)(n / 100), (unsigned long long)(n % 100));
    return b;
}
std::string displayMinute(const std::string &minute, const Settings &settings) {
    if (minute.size() != 16 || value(settings.fields, "dateFormat", "ISO") != "BR")
        return minute;
    return minute.substr(8, 2) + "/" + minute.substr(5, 2) + "/" + minute.substr(0, 4) +
           minute.substr(10);
}
bool validateFields(const Record &r, bool draft, std::string &error) {
    auto fail = [&](const char *s) {
        error = s;
        return false;
    };
    if (r.fields.size() > 16)
        return fail("Campos demais");
    for (const auto &p : r.fields) {
        auto &fields = recordFields(r.kind);
        auto i = std::find_if(fields.begin(), fields.end(),
                              [&](const Field &f) { return p.first == f.key; });
        if (p.first == "notebook" || p.first == "tags" || p.first == "favorite" ||
            p.first == "audio" || p.first == "drawing") {
            const bool attachment = p.first == "audio" || p.first == "drawing";
            if ((!attachment && r.kind != RecordKind::Note) ||
                (attachment && r.kind != RecordKind::Note && r.kind != RecordKind::Appointment))
                return fail("Metadado não aplicável ao registro");
            auto limit = p.first == "tags" ? 120u : p.first == "notebook" ? 40u : 80u;
            if (p.second.size() > limit || !validUtf8(p.second))
                return fail("Metadado inválido");
            if (p.first == "favorite" && p.second != "0" && p.second != "1")
                return fail("Favorita: 0 ou 1");
            if (attachment &&
                (p.second.empty() || p.second.front() == '.' ||
                 p.second.find_first_not_of(
                     "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_.") !=
                     std::string::npos))
                return fail("Anexo inválido");
            continue;
        }
        if (p.first == "trashRoot") {
            if (p.second.empty() || p.second.size() > 10 ||
                p.second.find_first_not_of("0123456789") != std::string::npos)
                return fail("Grupo de lixeira inválido");
            continue;
        }
        if (p.first == "trashed") {
            if (p.second != "1")
                return fail("Estado da lixeira inválido");
            continue;
        }
        if (p.first == "completedAt") {
            CivilMinute t;
            if (r.kind != RecordKind::Task || !parseCivilMinute(p.second, t))
                return fail("Data de conclusão inválida");
            continue;
        }
        if (p.first == "offset") {
            auto v = p.second;
            std::size_t start = !v.empty() && v[0] == '-' ? 1 : 0;
            if (v.size() > 4 || v.size() == start ||
                v.substr(start).find_first_not_of("0123456789") != std::string::npos ||
                std::stoi(v) < -720 || std::stoi(v) > 840)
                return fail("Fuso do registro inválido");
            continue;
        }
        if (p.first == "origin" || p.first == "occurrence") {
            if (r.kind != RecordKind::Appointment)
                return fail("Origem de ocorrência inválida");
            CivilMinute date;
            if (p.first == "origin" &&
                (p.second.empty() || p.second.size() > 9 ||
                 p.second.find_first_not_of("0123456789") != std::string::npos))
                return fail("Origem inválida");
            if (p.first == "occurrence" && !parseCivilMinute(p.second + " 00:00", date))
                return fail("Ocorrência inválida");
            continue;
        }
        if (p.first == "exceptions") {
            if (r.kind != RecordKind::Appointment || p.second.size() > 660 || !validUtf8(p.second))
                return fail("Exceções inválidas");
            continue;
        }
        if (i == fields.end() || p.second.size() > i->limit || !validUtf8(p.second))
            return fail("Campo inválido ou muito longo");
    }
    if (draft)
        return true;
    CivilMinute t;
    auto allDay = value(r.fields, "allDay", "0");
    if (allDay != "0" && allDay != "1")
        return fail("Dia inteiro: 0 ou 1");
    auto birthYear = value(r.fields, "birthYear");
    if (!birthYear.empty() &&
        (birthYear.size() != 4 || birthYear.find_first_not_of("0123456789") != std::string::npos ||
         std::stoi(birthYear) < 1850 || std::stoi(birthYear) > 2099))
        return fail("Ano de nascimento: 1850 a 2099");
    if (r.fields.count("origin") != r.fields.count("occurrence"))
        return fail("Origem de ocorrência incompleta");
    auto end = value(r.fields, "until");
    if (!end.empty() && (!parseCivilMinute(end, t) || end < r.when))
        return fail("Fim deve ser igual ou posterior ao início");
    auto reminder = value(r.fields, "reminder", "-1");
    if (reminder != "-1") {
        if (reminder.empty() || reminder.find_first_not_of("0123456789") != std::string::npos ||
            std::stoul(reminder) > 10080)
            return fail("Aviso: -1 ou 0 a 10080 minutos");
        if (r.when.empty())
            return fail("Lembrete exige data e hora");
    }
    auto repeat = value(r.fields, "repeat", "none");
    if (repeat != "none" && repeat != "daily" && repeat != "weekly" && repeat != "monthly" &&
        repeat != "yearly")
        return fail("Recorrência inválida");
    auto last = value(r.fields, "repeatEnd");
    if (!last.empty() && (!parseCivilMinute(last + " 00:00", t) || last < r.when.substr(0, 10)))
        return fail("Fim da recorrência inválido");
    auto exceptions = value(r.fields, "exceptions");
    for (std::size_t i = 0; i < exceptions.size(); i += 11) {
        if (i + 10 > exceptions.size() ||
            !parseCivilMinute(exceptions.substr(i, 10) + " 00:00", t) ||
            (i + 10 < exceptions.size() && exceptions[i + 10] != ','))
            return fail("Exceção de data inválida");
    }
    auto priority = value(r.fields, "priority", "2");
    if (priority != "1" && priority != "2" && priority != "3")
        return fail("Prioridade: 1, 2 ou 3");
    auto parent = value(r.fields, "parent");
    if (!parent.empty() &&
        (parent.size() > 9 || parent.find_first_not_of("0123456789") != std::string::npos ||
         parent == "0"))
        return fail("ID da tarefa principal inválido");
    if (repeat != "none" && r.when.empty())
        return fail("Recorrência exige prazo");
    auto link = value(r.fields, "contact");
    if (!link.empty() &&
        (link.find_first_not_of("0123456789") != std::string::npos || link.size() > 9))
        return fail("ID do contato inválido");
    auto leap = value(r.fields, "leap", "feb28");
    if (leap != "feb28" && leap != "skip")
        return fail("29/02: feb28 ou skip");
    if (r.kind == RecordKind::Expense) {
        int64_t cents;
        if (!parseMoney(value(r.fields, "amount"), cents))
            return fail("Informe um valor com até 2 decimais");
        auto currency = value(r.fields, "currency");
        if (currency.size() != 3 ||
            currency.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
            return fail("Moeda: 3 letras maiúsculas");
    }
    return true;
}
void normalizeRecord(Record &r) {
    if (r.kind == RecordKind::Appointment && value(r.fields, "allDay", "0") == "1" &&
        r.when.size() == 16) {
        r.when = r.when.substr(0, 10) + " 00:00";
        auto end = value(r.fields, "until", r.when);
        if (end.size() == 16)
            r.fields["until"] = end.substr(0, 10) + " 23:59";
    }
}
bool occurrenceOn(const Record &r, const std::string &day, std::string &occurrence) {
    CivilMinute base, target;
    if (trashed(r) || !parseCivilMinute(r.when, base) || !parseCivilMinute(day + " 00:00", target))
        return false;
    if (day < r.when.substr(0, 10))
        return false;
    auto end = value(r.fields, "repeatEnd");
    if (!end.empty() && day > end)
        return false;
    auto exceptions = value(r.fields, "exceptions");
    for (std::size_t i = 0; i < exceptions.size(); i += 11)
        if (exceptions.substr(i, 10) == day)
            return false;
    auto repeat = r.kind == RecordKind::Anniversary ? "yearly" : value(r.fields, "repeat", "none");
    int64_t delta =
        (civilToUnix(target, 0) - civilToUnix({base.year, base.month, base.day, 0, 0}, 0)) / 86400;
    bool yes = repeat == "daily" || (repeat == "none" && delta == 0) ||
               (repeat == "weekly" && delta % 7 == 0) ||
               (repeat == "monthly" && target.day == base.day) ||
               (repeat == "yearly" && target.month == base.month && target.day == base.day);
    if (r.kind == RecordKind::Anniversary && base.month == 2 && base.day == 29 &&
        !leapYear(target.year) && value(r.fields, "leap", "feb28") == "feb28" &&
        target.month == 2 && target.day == 28)
        yes = true;
    if (yes)
        occurrence = day + (value(r.fields, "allDay", "0") == "1" ? " 00:00" : r.when.substr(10));
    return yes;
}
std::string nextOccurrence(const Record &r, const std::string &from) {
    CivilMinute t;
    if (!parseCivilMinute(from, t))
        return {};
    auto repeat = value(r.fields, "repeat", "none");
    if (r.kind != RecordKind::Anniversary && repeat == "none") {
        std::string occurrence;
        return occurrenceOn(r, r.when.substr(0, 10), occurrence) && occurrence >= from
                   ? occurrence
                   : std::string{};
    }
    CivilMinute base;
    if (!parseCivilMinute(r.when, base))
        return {};
    if (r.when > from)
        t = base;
    t.hour = 0;
    t.minute = 0;
    auto start = civilToUnix(t, 0);
    for (int i = 0; i < 366 * 8 && t.year <= 2099; ++i) {
        std::string occurrence;
        if (occurrenceOn(r, formatCivilMinute(t).substr(0, 10), occurrence) && occurrence >= from)
            return occurrence;
        t = unixToCivil(start + (i + 1LL) * 86400, 0);
    }
    return {};
}
bool localOccurrenceOn(const Record &r, const std::string &day, int offset,
                       std::string &occurrence) {
    if (r.kind == RecordKind::Anniversary || value(r.fields, "allDay", "0") == "1")
        return occurrenceOn(r, day, occurrence);
    CivilMinute t;
    if (!parseCivilMinute(day + " 00:00", t))
        return false;
    int source = std::stoi(value(r.fields, "offset", std::to_string(offset)));
    auto midnight = civilToUnix(t, offset);
    for (int delta = -1; delta <= 1; ++delta) {
        std::string at;
        auto baseDay =
            formatCivilMinute(unixToCivil(midnight + delta * 86400LL, source)).substr(0, 10);
        if (occurrenceOn(r, baseDay, at) && parseCivilMinute(at, t)) {
            auto local = formatCivilMinute(unixToCivil(civilToUnix(t, source), offset));
            if (local.substr(0, 10) == day) {
                occurrence = local;
                return true;
            }
        }
    }
    return false;
}
std::vector<Alarm> dueAlarms(const State &s, const std::string &now) {
    std::vector<Alarm> out;
    CivilMinute t;
    if (!parseCivilMinute(now, t))
        return out;
    auto seconds = civilToUnix(t, s.settings.utcOffsetMinutes);
    for (const auto &r : s.records) {
        if (trashed(r) || r.completed ||
            (r.kind != RecordKind::Appointment && r.kind != RecordKind::Task &&
             r.kind != RecordKind::Anniversary))
            continue;
        auto text = value(r.fields, "reminder", "-1");
        if (text == "-1")
            continue;
        int before = std::stoi(text);
        int offset =
            r.kind == RecordKind::Anniversary || value(r.fields, "allDay", "0") == "1"
                ? s.settings.utcOffsetMinutes
                : std::stoi(value(r.fields, "offset", std::to_string(s.settings.utcOffsetMinutes)));
        // A reminder can precede its occurrence by seven days and remain overdue
        // for one day.
        for (int day = -1; day <= 8; ++day) {
            auto date =
                formatCivilMinute(unixToCivil(seconds + day * 86400LL, offset)).substr(0, 10);
            std::string occurrence;
            if (!occurrenceOn(r, date, occurrence))
                continue;
            auto key = std::to_string(r.id) + "@" + occurrence;
            auto receipt = value(s.alarms, key);
            if (receipt == "dismissed")
                continue;
            CivilMinute event;
            parseCivilMinute(occurrence, event);
            auto due = civilToUnix(event, offset) - before * 60LL;
            if (!receipt.empty()) {
                if (!parseCivilMinute(receipt, event))
                    continue;
                due = civilToUnix(event, 0);
            }
            if (seconds >= due && seconds - due <= 86400)
                out.push_back({r.id, occurrence, key});
        }
    }
    // Snoozes remain eligible even when their original occurrence leaves the day
    // window.
    for (const auto &receipt : s.alarms) {
        if (receipt.second == "dismissed")
            continue;
        CivilMinute until;
        if (!parseCivilMinute(receipt.second, until))
            continue;
        auto due = civilToUnix(until, 0);
        if (seconds < due || seconds - due > 86400)
            continue;
        auto separator = receipt.first.find('@');
        if (separator == std::string::npos)
            continue;
        auto id = static_cast<uint32_t>(std::stoul(receipt.first.substr(0, separator)));
        auto *r = findRecord(s, id);
        if (!r || r->completed)
            continue;
        auto occurrence = receipt.first.substr(separator + 1);
        std::string checked;
        if (!occurrenceOn(*r, occurrence.substr(0, 10), checked) || checked != occurrence)
            continue;
        if (std::none_of(out.begin(), out.end(),
                         [&](const Alarm &a) { return a.key == receipt.first; }))
            out.push_back({id, occurrence, receipt.first});
    }
    return out;
}
std::map<std::string, int64_t> expenseTotals(const State &s, const std::string &period) {
    std::map<std::string, int64_t> out;
    for (const auto &r : s.records)
        if (!trashed(r) && r.kind == RecordKind::Expense &&
            r.when.compare(0, period.size(), period) == 0) {
            int64_t cents;
            if (parseMoney(value(r.fields, "amount"), cents))
                out[value(r.fields, "currency")] += cents;
        }
    return out;
}
bool convert(const std::string &s, double &out, std::string &error) {
    // Input: numeric expression ; source unit ; target unit [; manual exchange
    // rate]
    std::vector<std::string> parts;
    std::size_t start = 0;
    do {
        auto end = s.find(';', start);
        parts.push_back(s.substr(start, end - start));
        if (end == std::string::npos)
            break;
        start = end + 1;
    } while (parts.size() < 5);
    double n;
    if ((parts.size() != 3 && parts.size() != 4) || !calculate(parts[0], n, error)) {
        error = "Use valor;origem;destino[;taxa]";
        return false;
    }
    for (auto &p : parts) {
        auto a = p.find_first_not_of(' '), b = p.find_last_not_of(' ');
        p = a == std::string::npos ? "" : p.substr(a, b - a + 1);
    }
    struct Unit {
        const char *name;
        int type;
        double factor;
    };
    static const Unit units[] = {{"mm", 1, .001},      {"cm", 1, .01},           {"m", 1, 1},
                                 {"km", 1, 1000},      {"in", 1, .0254},         {"ft", 1, .3048},
                                 {"mi", 1, 1609.344},  {"g", 2, .001},           {"kg", 2, 1},
                                 {"lb", 2, .45359237}, {"oz", 2, .028349523125}, {"ml", 3, .001},
                                 {"l", 3, 1},          {"galUS", 3, 3.785411784}};
    if (parts.size() == 3 && (parts[1] == "C" || parts[1] == "F" || parts[1] == "K") &&
        (parts[2] == "C" || parts[2] == "F" || parts[2] == "K")) {
        double c = parts[1] == "F" ? (n - 32) * 5 / 9 : parts[1] == "K" ? n - 273.15 : n;
        if (c < -273.15) {
            error = "Temperatura abaixo do zero absoluto";
            return false;
        }
        out = parts[2] == "F" ? c * 9 / 5 + 32 : parts[2] == "K" ? c + 273.15 : c;
        if (std::isfinite(out))
            return true;
        error = "Valor convertido fora do limite";
        return false;
    }
    const Unit *a = nullptr, *b = nullptr;
    for (const auto &u : units) {
        if (parts[1] == u.name)
            a = &u;
        if (parts[2] == u.name)
            b = &u;
    }
    if (parts.size() == 3 && a && b && a->type == b->type) {
        out = n * a->factor / b->factor;
        return std::isfinite(out);
    }
    auto currency = [](const std::string &p) {
        return p.size() == 3 &&
               p.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos;
    };
    double rate;
    if (parts.size() == 4 && currency(parts[1]) && currency(parts[2]) &&
        calculate(parts[3], rate, error) && rate > 0 && std::isfinite(n * rate)) {
        out = n * rate;
        return true;
    }
    error = "Unidades incompatíveis ou taxa inválida";
    return false;
}
} // namespace pda

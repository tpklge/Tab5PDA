#include "exchange.hpp"
#include "pda/organizer.hpp"
#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace tab5_pda {
namespace {
using namespace pda;
std::string escape(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c == '\n')
            out += "\\n";
        else {
            if (c == '\\' || c == ';' || c == ',')
                out += '\\';
            out += c;
        }
    }
    return out;
}
std::string unescape(const std::string &s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[++i];
            out += c == 'n' || c == 'N' ? '\n' : c;
        } else
            out += s[i];
    }
    return out;
}
void property(std::string &out, std::string line) {
    // RFC content lines fold at UTF-8 boundaries; CRLF is used on disk.
    while (line.size() > 72) {
        std::size_t n = 72;
        while (n && (static_cast<unsigned char>(line[n]) & 0xc0) == 0x80)
            --n;
        out += line.substr(0, n) + "\r\n ";
        line.erase(0, n);
    }
    out += line + "\r\n";
}
std::string compact(const std::string &s) {
    std::string out;
    for (char c : s)
        if (c >= '0' && c <= '9')
            out += c;
    if (s.size() == 16)
        out.insert(8, "T");
    return out + (s.size() == 16 ? "00" : "");
}
bool date(const std::string &s, int offset, std::string &out) {
    bool utc = !s.empty() && s.back() == 'Z';
    if (s.size() != 8 && s.size() != 15 && !(s.size() == 16 && utc))
        return false;
    if (s.size() > 8 && (s[8] != 'T' || s.substr(13, 2) != "00"))
        return false;
    out = s.substr(0, 4) + "-" + s.substr(4, 2) + "-" + s.substr(6, 2) + " " +
          (s.size() == 8 ? "00:00" : s.substr(9, 2) + ":" + s.substr(11, 2));
    CivilMinute t;
    if (!parseCivilMinute(out, t))
        return false;
    if (utc)
        out = formatCivilMinute(unixToCivil(civilToUnix(t, 0), offset));
    return parseCivilMinute(out, t);
}
std::string csv(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"')
            out += '"';
        out += c;
    }
    return out + '"';
}
bool rows(const std::string &text, std::vector<std::vector<std::string>> &out) {
    std::vector<std::string> row;
    std::string cell;
    bool quoted = false, closed = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (cell.size() > kMaxDetailBytes + 128)
            return false;
        if (quoted) {
            if (c == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    cell += '"';
                    ++i;
                } else {
                    quoted = false;
                    closed = true;
                }
            } else
                cell += c;
        } else if (c == '"' && cell.empty() && !closed)
            quoted = true;
        else if (c == ',' || c == '\n' || c == '\r') {
            if (row.size() >= 64 || cell.size() > kMaxDetailBytes + 128)
                return false;
            row.push_back(cell);
            cell.clear();
            closed = false;
            if (c != ',') {
                if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                    ++i;
                if (out.size() >= kMaxRecords + 1)
                    return false;
                out.push_back(row);
                row.clear();
            }
        } else {
            if (closed || c == '"')
                return false;
            cell += c;
        }
    }
    if (quoted)
        return false;
    if (!cell.empty() || !row.empty() || closed) {
        if (row.size() >= 64 || cell.size() > kMaxDetailBytes + 128 ||
            out.size() >= kMaxRecords + 1)
            return false;
        row.push_back(cell);
        out.push_back(row);
    }
    return true;
}
} // namespace
std::string exportRecords(const pda::State &state, ExchangeFormat format, std::string &error) {
    using namespace pda;
    error.clear();
    std::string out;
    if (format == ExchangeFormat::Calendar)
        out = "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Tab5PDA//PT-BR\r\n";
    if (format == ExchangeFormat::Expenses)
        out = "title,when,amount,currency,category,detail\r\n";
    for (const auto &r : state.records) {
        if (trashed(r))
            continue;
        if (format == ExchangeFormat::Calendar && r.kind == RecordKind::Appointment) {
            out += "BEGIN:VEVENT\r\n";
            property(out, "UID:tab5-" + std::to_string(r.id) + "@pda.local");
            property(out, "SUMMARY:" + escape(r.title));
            bool allDay = value(r.fields, "allDay") == "1";
            auto stamp = [&](std::string at) {
                CivilMinute t;
                parseCivilMinute(at, t);
                if (!allDay) {
                    int offset = std::stoi(
                        value(r.fields, "offset", std::to_string(state.settings.utcOffsetMinutes)));
                    return compact(formatCivilMinute(unixToCivil(civilToUnix(t, offset), 0))) + "Z";
                }
                return compact(at.substr(0, 10));
            };
            property(out, std::string(allDay ? "DTSTART;VALUE=DATE:" : "DTSTART:") + stamp(r.when));
            auto end = value(r.fields, "until", r.when);
            if (allDay) {
                CivilMinute t;
                parseCivilMinute(end, t);
                end = formatCivilMinute(
                    unixToCivil(civilToUnix({t.year, t.month, t.day, 0, 0}, 0) + 86400, 0));
            }
            property(out, std::string(allDay ? "DTEND;VALUE=DATE:" : "DTEND:") + stamp(end));
            for (auto pair : {std::make_pair("location", "LOCATION"), {"category", "CATEGORIES"}})
                if (!value(r.fields, pair.first).empty())
                    property(out,
                             std::string(pair.second) + ":" + escape(value(r.fields, pair.first)));
            property(out, "DESCRIPTION:" + escape(r.detail));
            auto repeat = value(r.fields, "repeat", "none");
            if (repeat != "none") {
                std::transform(repeat.begin(), repeat.end(), repeat.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                auto rule = "RRULE:FREQ=" + repeat;
                auto until = value(r.fields, "repeatEnd");
                if (!until.empty())
                    rule += ";UNTIL=" + (allDay ? compact(until) : stamp(until + " 23:59"));
                property(out, rule);
            }
            auto exceptions = value(r.fields, "exceptions");
            for (std::size_t i = 0; i < exceptions.size(); i += 11) {
                auto at = exceptions.substr(i, 10) + r.when.substr(10);
                property(out, std::string(allDay ? "EXDATE;VALUE=DATE:" : "EXDATE:") + stamp(at));
            }
            auto reminder = value(r.fields, "reminder", "-1");
            if (reminder != "-1")
                out += "BEGIN:VALARM\r\nACTION:DISPLAY\r\nDESCRIPTION:Lembrete\r\nTRIGGER:-PT" +
                       reminder + "M\r\nEND:VALARM\r\n";
            out += "END:VEVENT\r\n";
        }
        if (format == ExchangeFormat::Contacts && r.kind == RecordKind::Contact) {
            out += "BEGIN:VCARD\r\nVERSION:3.0\r\n";
            property(out, "FN:" + escape(r.title));
            property(out, "N:;" + escape(r.title) + ";;;");
            property(out, "NOTE:" + escape(r.detail));
            for (auto pair : {std::make_pair("phone", "TEL"),
                              {"email", "EMAIL"},
                              {"company", "ORG"},
                              {"address", "LABEL"},
                              {"category", "CATEGORIES"}})
                if (!value(r.fields, pair.first).empty())
                    property(out,
                             std::string(pair.second) + ":" + escape(value(r.fields, pair.first)));
            out += "END:VCARD\r\n";
        }
        if (format == ExchangeFormat::Expenses && r.kind == RecordKind::Expense)
            out += csv(r.title) + "," + csv(r.when) + "," + csv(value(r.fields, "amount")) + "," +
                   csv(value(r.fields, "currency")) + "," + csv(value(r.fields, "category")) + "," +
                   csv(r.detail) + "\r\n";
        if ((format == ExchangeFormat::Text || format == ExchangeFormat::Markdown) &&
            r.kind == RecordKind::Note) {
            out += (format == ExchangeFormat::Markdown ? "# " : "") + r.title + "\n" + r.detail +
                   "\n\n";
        }
    }
    if (format == ExchangeFormat::Calendar)
        out += "END:VCALENDAR\r\n";
    if (out.size() > kMaxSnapshotBytes) {
        error = "Exportação excede o limite de arquivo";
        return {};
    }
    return out;
}

bool parseRecords(const std::string &text, ExchangeFormat format, const pda::Settings &settings,
                  std::vector<pda::Record> &out, std::string &error) {
    using namespace pda;
    out.clear();
    auto fail = [&](const std::string &why) {
        error = why;
        out.clear();
        return false;
    };
    if (text.empty() || text.size() > kMaxSnapshotBytes)
        return fail("Arquivo vazio ou grande demais");
    if (format == ExchangeFormat::Text || format == ExchangeFormat::Markdown) {
        auto newline = text.find('\n');
        auto title = text.substr(0, newline);
        if (format == ExchangeFormat::Markdown && title.rfind("# ", 0) == 0)
            title.erase(0, 2);
        if (!title.empty() && title.back() == '\r')
            title.pop_back();
        Record r;
        r.kind = RecordKind::Note;
        r.title = title;
        r.detail = newline == std::string::npos ? "" : text.substr(newline + 1);
        out.push_back(r);
    } else if (format == ExchangeFormat::Expenses) {
        std::vector<std::vector<std::string>> data;
        if (!rows(text, data) || data.empty() ||
            data[0] != std::vector<std::string>{"title", "when", "amount", "currency", "category",
                                                "detail"})
            return fail("CSV exige cabeçalho title,when,amount,currency,category,detail");
        for (std::size_t i = 1; i < data.size(); ++i) {
            if (data[i].size() != 6)
                return fail("Linha CSV com quantidade de colunas inválida");
            Record r;
            r.kind = RecordKind::Expense;
            r.title = data[i][0];
            r.when = data[i][1];
            r.detail = data[i][5];
            int64_t cents;
            if (!parseMoney(data[i][2], cents))
                return fail("Valor CSV inválido");
            r.fields = {
                {"amount", formatMoney(cents)}, {"currency", data[i][3]}, {"category", data[i][4]}};
            out.push_back(r);
        }
    } else {
        std::vector<std::string> lines;
        std::istringstream input(text);
        for (std::string line; std::getline(input, line);) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.empty() && (line[0] == ' ' || line[0] == '\t')) {
                if (lines.empty())
                    return fail("Continuação de linha inválida");
                lines.back() += line.substr(1);
            } else
                lines.push_back(line);
        }
        bool active = false, alarm = false, envelope = false, closed = false;
        Record r;
        for (const auto &line : lines) {
            auto colon = line.find(':');
            if (colon == std::string::npos) {
                if (line.empty())
                    continue;
                return fail("Linha sem propriedade");
            }
            auto key = line.substr(0, colon), val = line.substr(colon + 1);
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            auto semi = key.find(';');
            auto name = key.substr(0, semi);
            auto group = name.find_last_of('.');
            if (group != std::string::npos)
                name.erase(0, group + 1);
            if (key.find("TZID=") != std::string::npos ||
                key.find("ENCODING=") != std::string::npos)
                return fail("Fuso nomeado ou codificação não suportados; use UTC/UTF-8");
            if (line == "BEGIN:VCALENDAR") {
                if (envelope || format != ExchangeFormat::Calendar)
                    return fail("Calendário inválido");
                envelope = true;
                continue;
            }
            if (line == "END:VCALENDAR") {
                if (active)
                    return fail("Evento incompleto");
                closed = true;
                continue;
            }
            bool begin =
                line == (format == ExchangeFormat::Calendar ? "BEGIN:VEVENT" : "BEGIN:VCARD");
            bool end = line == (format == ExchangeFormat::Calendar ? "END:VEVENT" : "END:VCARD");
            if (begin) {
                if (active || closed)
                    return fail("Registro aninhado");
                active = true;
                r = {};
                r.kind = format == ExchangeFormat::Calendar ? RecordKind::Appointment
                                                            : RecordKind::Contact;
                continue;
            }
            if (end) {
                if (!active || alarm)
                    return fail("Registro incompleto");
                normalizeRecord(r);
                out.push_back(r);
                active = false;
                continue;
            }
            if (line == "BEGIN:VALARM" && active) {
                alarm = true;
                continue;
            }
            if (line == "END:VALARM") {
                if (!alarm)
                    return fail("Aviso inválido");
                alarm = false;
                continue;
            }
            if (!active) {
                if (name == "BEGIN" || name == "END")
                    return fail("Componente não suportado");
                continue;
            }
            if (alarm) {
                if (name == "TRIGGER") {
                    if (key.find("RELATED=END") != std::string::npos)
                        return fail("Aviso relativo ao fim não suportado");
                    if (val.rfind("-PT", 0) != 0 || val.back() != 'M')
                        return fail("Aviso exige duração em minutos");
                    r.fields["reminder"] = val.substr(3, val.size() - 4);
                }
                continue;
            }
            if (name == "BEGIN" || name == "END")
                return fail("Componente não suportado");
            if (name == "SUMMARY" || name == "FN")
                r.title = unescape(val);
            else if (name == "DESCRIPTION" || name == "NOTE")
                r.detail = unescape(val);
            else if (name == "LOCATION")
                r.fields["location"] = unescape(val);
            else if (name == "CATEGORIES")
                r.fields["category"] = unescape(val);
            else if (name == "TEL") {
                if (!r.fields["phone"].empty())
                    r.fields["phone"] += "; ";
                r.fields["phone"] += unescape(val);
            } else if (name == "EMAIL")
                r.fields["email"] = unescape(val);
            else if (name == "ORG")
                r.fields["company"] = unescape(val);
            else if (name == "LABEL" || name == "ADR")
                r.fields["address"] = unescape(val);
            else if (name == "DTSTART" || name == "DTEND" || name == "EXDATE") {
                std::string at;
                if (!date(val, settings.utcOffsetMinutes, at))
                    return fail("Data externa inválida ou precisão não suportada");
                if (name == "DTSTART") {
                    r.when = at;
                    if (val.size() == 8)
                        r.fields["allDay"] = "1";
                } else if (name == "DTEND") {
                    if (val.size() == 8) {
                        CivilMinute t;
                        parseCivilMinute(at, t);
                        at = formatCivilMinute(unixToCivil(civilToUnix(t, 0) - 60, 0));
                    }
                    r.fields["until"] = at;
                } else {
                    auto &v = r.fields["exceptions"];
                    if (!v.empty())
                        v += ",";
                    v += at.substr(0, 10);
                }
            } else if (name == "DURATION" || name == "RECURRENCE-ID" || name == "RDATE" ||
                       name == "EXRULE")
                return fail("Use eventos simples; duração ou exceção externa não suportada");
            else if (name == "RRULE") {
                std::istringstream rule(val);
                for (std::string part; std::getline(rule, part, ';');) {
                    if (part.rfind("FREQ=", 0) == 0) {
                        auto freq = part.substr(5);
                        std::transform(freq.begin(), freq.end(), freq.begin(), [](unsigned char c) {
                            return static_cast<char>(std::tolower(c));
                        });
                        r.fields["repeat"] = freq;
                    } else if (part.rfind("UNTIL=", 0) == 0) {
                        std::string at;
                        if (!date(part.substr(6), settings.utcOffsetMinutes, at))
                            return fail("Limite de recorrência inválido");
                        r.fields["repeatEnd"] = at.substr(0, 10);
                    } else if (part != "INTERVAL=1")
                        return fail("Regra de recorrência não suportada");
                }
            }
        }
        if (active || alarm || (format == ExchangeFormat::Calendar && (!envelope || !closed)))
            return fail("Arquivo incompleto");
    }
    if (out.empty() || out.size() > kMaxRecords)
        return fail("Nenhum registro ou capacidade excedida");
    for (auto &r : out) {
        if (!validRecord(r, false, error)) {
            out.clear();
            return false;
        }
    }
    error.clear();
    return true;
}
bool parseCsv(const std::string &text, std::vector<std::vector<std::string>> &out) {
    out.clear();
    return rows(text, out);
}
bool previewTable(const std::string &text, std::vector<std::vector<std::string>> &out,
                  std::string &error) {
    out.clear();
    if (!rows(text, out) || out.empty()) {
        error = "Tabela CSV inválida";
        return false;
    }
    if (out.size() > 101)
        out.resize(101);
    for (auto &row : out) {
        if (row.size() > 12)
            row.resize(12);
        for (auto &cell : row)
            if (cell.size() > 256) {
                std::size_t length = 256;
                while (length && (static_cast<unsigned char>(cell[length]) & 0xc0) == 0x80)
                    --length;
                cell.resize(length);
            }
    }
    error.clear();
    return true;
}
bool mergeRecords(pda::Service &service, std::vector<pda::Record> records, std::string &error) {
    pda::State next = service.state();
    if (records.size() > pda::kMaxRecords - next.records.size()) {
        error = "Capacidade de registros excedida";
        return false;
    }
    for (auto &r : records) {
        if (next.nextId == std::numeric_limits<uint32_t>::max()) {
            error = "IDs esgotados";
            return false;
        }
        r.id = next.nextId++;
        if (r.kind == pda::RecordKind::Appointment || r.kind == pda::RecordKind::Task ||
            r.kind == pda::RecordKind::Anniversary)
            r.fields["offset"] = std::to_string(next.settings.utcOffsetMinutes);
        next.records.push_back(std::move(r));
    }
    if (!service.restore(next)) {
        error = service.error();
        return false;
    }
    error.clear();
    return true;
}
} // namespace tab5_pda

#include "productivity.hpp"
#include "exchange.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#include <sstream>
namespace tab5_pda {
namespace {
std::string quote(const std::string &s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"')
            out += '"';
        out += c;
    }
    return out + '"';
}
bool idNumber(const std::string &s, uint32_t &v) {
    if (s.empty() || s.size() > 10)
        return false;
    uint64_t n = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
        n = n * 10 + c - '0';
        if (n >= UINT32_MAX)
            return false;
    }
    v = n;
    return n != 0;
}
} // namespace
std::string normalizeDocumentText(const std::string &text) {
    std::string out;
    out.reserve(text.size());
    std::size_t begin = text.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    for (std::size_t i = begin; i < text.size(); ++i) {
        if (text[i] == '\r') {
            out += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
        } else
            out += text[i];
    }
    return out;
}
bool imageDimensions(const std::string &data, unsigned &width, unsigned &height) {
    const auto *b = reinterpret_cast<const unsigned char *>(data.data());
    auto n = data.size();
    width = height = 0;
    auto be = [&](std::size_t at) {
        return (unsigned(b[at]) << 24) | (unsigned(b[at + 1]) << 16) | (unsigned(b[at + 2]) << 8) |
               b[at + 3];
    };
    auto le = [&](std::size_t at) {
        return unsigned(b[at]) | (unsigned(b[at + 1]) << 8) | (unsigned(b[at + 2]) << 16) |
               (unsigned(b[at + 3]) << 24);
    };
    if (n >= 24 && data.compare(0, 8, std::string("\x89PNG\r\n\x1a\n", 8)) == 0 &&
        data.compare(12, 4, "IHDR") == 0) {
        width = be(16);
        height = be(20);
    } else if (n >= 54 && data.substr(0, 2) == "BM" && le(14) >= 40) {
        width = le(18);
        auto signedHeight = static_cast<int32_t>(le(22));
        if (signedHeight == INT32_MIN)
            return false;
        height = signedHeight < 0 ? unsigned(-signedHeight) : unsigned(signedHeight);
    } else if (n >= 4 && b[0] == 0xff && b[1] == 0xd8) {
        for (std::size_t p = 2; p + 3 < n;) {
            if (b[p++] != 0xff)
                return false;
            while (p < n && b[p] == 0xff)
                ++p;
            if (p >= n)
                return false;
            auto marker = b[p++];
            if (marker == 0xd9 || marker == 0xda)
                break;
            if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd7))
                continue;
            if (p + 2 > n)
                return false;
            unsigned length = (unsigned(b[p]) << 8) | b[p + 1];
            if (length < 2 || length > n - p)
                return false;
            if ((marker >= 0xc0 && marker <= 0xcf) && marker != 0xc4 && marker != 0xc8 &&
                marker != 0xcc) {
                if (length < 8)
                    return false;
                height = (unsigned(b[p + 3]) << 8) | b[p + 4];
                width = (unsigned(b[p + 5]) << 8) | b[p + 6];
                break;
            }
            p += length;
        }
    }
    return width && height && width <= 4096 && height <= 4096;
}
bool Sheet::value(int cell, double &out, std::string &error) const {
    std::array<bool, rows * cols> visiting{};
    unsigned operations = 0;
    unsigned depth = 0; // Shared across referenced cells as well as parentheses.
    std::function<double(int)> eval;
    bool okay = true;
    eval = [&](int index) -> double {
        if (!okay || index < 0 || index >= rows * cols || visiting[index] ||
            std::count(visiting.begin(), visiting.end(), true) >= 24 || ++operations > 10000) {
            okay = false;
            return 0;
        }
        visiting[index] = true;
        const auto &text = cells[index];
        if (text.empty()) {
            visiting[index] = false;
            return 0;
        }
        if (text[0] != '=') {
            char *end = nullptr;
            double n = std::strtod(text.c_str(), &end);
            if (end == text.c_str() || *end || !std::isfinite(n))
                okay = false;
            visiting[index] = false;
            return n;
        }
        std::size_t pos = 1;
        auto spaces = [&] {
            while (pos < text.size() && text[pos] == ' ')
                ++pos;
        };
        auto reference = [&]() -> int {
            spaces();
            if (pos >= text.size() || text[pos] < 'A' || text[pos] > 'H') {
                okay = false;
                return -1;
            }
            int col = text[pos++] - 'A', row = 0;
            std::size_t begin = pos;
            while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
                row = row * 10 + text[pos++] - '0';
                if (row > rows) {
                    okay = false;
                    return -1;
                }
            }
            if (pos == begin || row < 1) {
                okay = false;
                return -1;
            }
            return (row - 1) * cols + col;
        };
        std::function<double()> expression, term, factor;
        factor = [&]() -> double {
            spaces();
            if (++depth > 24) {
                okay = false;
                --depth;
                return 0;
            }
            double v = 0;
            if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
                bool minus = text[pos++] == '-';
                v = factor();
                if (minus)
                    v = -v;
            } else if (pos < text.size() && text[pos] == '(') {
                ++pos;
                v = expression();
                spaces();
                if (pos >= text.size() || text[pos++] != ')')
                    okay = false;
            } else if (text.compare(pos, 4, "SUM(") == 0 || text.compare(pos, 5, "SOMA(") == 0) {
                pos += text.compare(pos, 4, "SUM(") == 0 ? 4 : 5;
                int a = reference();
                spaces();
                if (pos >= text.size() || text[pos++] != ':') {
                    okay = false;
                }
                int b = reference();
                spaces();
                if (pos >= text.size() || text[pos++] != ')')
                    okay = false;
                if (okay && a >= 0 && b >= 0) {
                    for (int r = std::min(a / cols, b / cols); r <= std::max(a / cols, b / cols);
                         ++r)
                        for (int c = std::min(a % cols, b % cols);
                             c <= std::max(a % cols, b % cols); ++c)
                            v += eval(r * cols + c);
                }
            } else if (pos < text.size() && text[pos] >= 'A' && text[pos] <= 'H') {
                auto ref = reference();
                if (ref >= 0)
                    v = eval(ref);
            } else {
                char *end = nullptr;
                v = std::strtod(text.c_str() + pos, &end);
                if (end == text.c_str() + pos)
                    okay = false;
                else
                    pos = end - text.c_str();
            }
            --depth;
            return v;
        };
        term = [&] {
            double v = factor();
            spaces();
            while (okay && pos < text.size() && (text[pos] == '*' || text[pos] == '/')) {
                char op = text[pos++];
                double n = factor();
                if (op == '/' && n == 0) {
                    okay = false;
                    return 0.0;
                }
                v = op == '*' ? v * n : v / n;
                spaces();
            }
            return v;
        };
        expression = [&] {
            double v = term();
            spaces();
            while (okay && pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
                char op = text[pos++];
                double n = term();
                v = op == '+' ? v + n : v - n;
                spaces();
            }
            return v;
        };
        double result = expression();
        spaces();
        if (pos != text.size() || !std::isfinite(result))
            okay = false;
        visiting[index] = false;
        return result;
    };
    out = eval(cell);
    error = okay ? "" : "Fórmula inválida, referência circular ou divisão por zero";
    return okay;
}
std::string Sheet::csv() const {
    std::string out;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (c)
                out += ',';
            out += quote(cells[r * cols + c]);
        }
        out += '\n';
    }
    return out;
}
bool Sheet::load(const std::string &text, std::string &error) {
    std::vector<std::vector<std::string>> data;
    if (!parseCsv(normalizeDocumentText(text), data) || data.size() > rows) {
        error = "CSV: máximo de 20 linhas e 8 colunas";
        return false;
    }
    Sheet next;
    for (std::size_t r = 0; r < data.size(); ++r) {
        if (data[r].size() > cols) {
            error = "CSV: máximo de 8 colunas";
            return false;
        }
        for (std::size_t c = 0; c < data[r].size(); ++c) {
            if (data[r][c].size() > 128 || !pda::validUtf8(data[r][c])) {
                error = "Célula inválida ou maior que 128 bytes";
                return false;
            }
            next.cells[r * cols + c] = data[r][c];
        }
    }
    *this = next;
    error.clear();
    return true;
}
namespace {
std::size_t boundary(const std::string &text, std::size_t at) {
    at = std::min(at, text.size());
    while (at < text.size() && (static_cast<unsigned char>(text[at]) & 0xc0) == 0x80)
        ++at;
    return at;
}
} // namespace
unsigned readingPages(const std::string &s, unsigned size) {
    if (!size)
        return 1;
    auto pages = std::max<std::size_t>(1, (s.size() + size - 1) / size);
    while (pages > 1 && boundary(s, (pages - 1) * size) == s.size())
        --pages;
    return pages;
}
std::string readingPage(const std::string &s, unsigned page, unsigned size) {
    if (!size)
        return {};
    auto begin = boundary(s, std::size_t(page) * size),
         end = boundary(s, (std::size_t(page) + 1) * size);
    return s.substr(begin, end - begin);
}
bool validLedger(const std::vector<LedgerEntry> &entries, std::string &error) {
    auto fail = [&](const char *why) {
        error = why;
        return false;
    };
    if (entries.size() > 1000)
        return fail("Limite de 1000 lançamentos");
    std::set<uint32_t> ids;
    std::set<std::string> origins;
    for (const auto &e : entries) {
        pda::CivilMinute t;
        if (!e.id || e.id == UINT32_MAX || !ids.insert(e.id).second ||
            !pda::parseCivilMinute(e.day + " 00:00", t))
            return fail("ID ou data inválida");
        if (e.account.empty() || e.title.empty() || e.account.size() > 40 || e.title.size() > 80 ||
            e.category.size() > 40 || e.currency.size() != 3 ||
            e.currency.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos ||
            e.cents > 100000000000000LL || e.cents < -100000000000000LL)
            return fail("Lançamento inválido");
        for (auto *s : {&e.account, &e.title, &e.category, &e.origin})
            if (!pda::validUtf8(*s) || s->find('\n') != std::string::npos)
                return fail("Campo inválido");
        if (e.budget && (e.cents < 0 || e.monthly))
            return fail("Orçamento inválido");
        if (e.origin.size() > 40 || (!e.origin.empty() && !origins.insert(e.origin).second))
            return fail("Origem recorrente duplicada");
    }
    error.clear();
    return true;
}
std::string ledgerCsv(const std::vector<LedgerEntry> &entries) {
    std::string out = "id,date,account,title,amount,currency,category,monthly,origin,budget\n";
    for (const auto &e : entries)
        out += std::to_string(e.id) + "," + quote(e.day) + "," + quote(e.account) + "," +
               quote(e.title) + "," + pda::formatMoney(e.cents) + "," + e.currency + "," +
               quote(e.category) + "," + (e.monthly ? "1" : "0") + "," + quote(e.origin) + "," +
               (e.budget ? "1" : "0") + "\n";
    return out;
}
bool loadLedger(const std::string &text, std::vector<LedgerEntry> &out, std::string &error) {
    std::vector<std::vector<std::string>> data;
    std::vector<LedgerEntry> next;
    if (!parseCsv(text, data) || data.empty() ||
        data[0] != std::vector<std::string>{"id", "date", "account", "title", "amount", "currency",
                                            "category", "monthly", "origin", "budget"}) {
        error = "Formato do livro financeiro inválido";
        return false;
    }
    for (std::size_t i = 1; i < data.size(); ++i) {
        const auto &r = data[i];
        LedgerEntry e;
        if (r.size() != 10 || !idNumber(r[0], e.id) || !pda::parseMoney(r[4], e.cents) ||
            (r[7] != "0" && r[7] != "1") || (r[9] != "0" && r[9] != "1")) {
            error = "Linha financeira inválida";
            return false;
        }
        e.day = r[1];
        e.account = r[2];
        e.title = r[3];
        e.currency = r[5];
        e.category = r[6];
        e.monthly = r[7] == "1";
        e.origin = r[8];
        e.budget = r[9] == "1";
        next.push_back(e);
    }
    if (!validLedger(next, error))
        return false;
    out = std::move(next);
    return true;
}
bool postMonthly(std::vector<LedgerEntry> &entries, const std::string &month, std::string &error) {
    if (!validLedger(entries, error))
        return false;
    pda::CivilMinute date;
    if (!pda::parseCivilMinute(month + "-01 00:00", date)) {
        error = "Mês inválido";
        return false;
    }
    auto next = entries;
    uint32_t id = 1;
    for (const auto &e : entries)
        id = std::max(id, e.id + 1);
    for (const auto &e : entries)
        if (e.monthly && !e.budget && e.day.substr(0, 7) < month) {
            auto key = std::to_string(e.id) + "@" + month;
            if (std::any_of(next.begin(), next.end(),
                            [&](const auto &r) { return r.origin == key; }))
                continue;
            auto copy = e;
            copy.id = id++;
            copy.monthly = false;
            copy.origin = key;
            int day =
                std::min(std::stoi(e.day.substr(8, 2)), pda::daysInMonth(date.year, date.month));
            char formatted[12];
            std::snprintf(formatted, sizeof(formatted), "%04d-%02d-%02d", date.year, date.month,
                          day);
            copy.day = formatted;
            next.push_back(copy);
        }
    if (!validLedger(next, error))
        return false;
    entries = std::move(next);
    return true;
}
std::map<std::string, int64_t> ledgerTotals(const std::vector<LedgerEntry> &entries,
                                            const std::string &month, bool expenses,
                                            const std::string &category) {
    std::map<std::string, int64_t> out;
    for (const auto &e : entries)
        if (!e.budget && (month.empty() || e.day.substr(0, 7) == month) &&
            (category.empty() || e.category == category)) {
            auto key = e.account + " / " + e.currency;
            out[key] += expenses ? std::max<int64_t>(0, -e.cents) : e.cents;
        }
    return out;
}
} // namespace tab5_pda

#include "exchange.hpp"
#include "memory_files.hpp"
#include "productivity.hpp"
#include <cmath>
#include <iostream>
#include <random>

using namespace tab5_pda;
int main() {
    try {
        std::string error;
        std::vector<std::vector<std::string>> table;
        std::string tooManyColumns(64, ',');
        CHECK(!parseCsv(tooManyColumns, table)); // The final row has no newline.
        CHECK(!parseCsv(std::string(17000, 'a'), table));
        Sheet windows;
        CHECK(windows.load("\xef\xbb\xbf\"10\",\"linha 1\r\nlinha 2\"\r\n", error));
        CHECK(windows.cells[0] == "10" && windows.cells[1] == "linha 1\nlinha 2");
        std::string accent(255, 'a');
        accent += "ç";
        CHECK(previewTable(accent, table, error));
        CHECK(pda::validUtf8(table[0][0]));

        std::mt19937 rng(0x5DA2026);
        const std::string tokens = "=0123456789.ABCDEFGH()+-*/:SUMO ,e\"";
        for (unsigned run = 0; run < 5000; ++run) {
            std::string bytes(rng() % 160, '\0');
            for (auto &b : bytes)
                b = static_cast<char>(rng());
            unsigned width, height;
            if (imageDimensions(bytes, width, height))
                CHECK(width && height && width <= 4096 && height <= 4096);
            Sheet sheet;
            for (unsigned cell = 0; cell < 4; ++cell) {
                auto &text = sheet.cells[cell];
                text.resize(rng() % 128);
                for (auto &ch : text)
                    ch = tokens[rng() % tokens.size()];
            }
            double value;
            if (sheet.value(0, value, error))
                CHECK(std::isfinite(value));
            Sheet restored;
            CHECK(restored.load(sheet.csv(), error));
            CHECK(restored.cells == sheet.cells);
            std::vector<std::vector<std::string>> arbitrary;
            parseCsv(bytes, arbitrary); // ASan/UBSan check malformed binary CSV too.
        }
        const char *characters[] = {"a", "ç", "á", "€", "🍀", "\n"};
        for (unsigned run = 0; run < 200; ++run) {
            std::string text, combined;
            for (unsigned c = 0; c < 1000; ++c)
                text += characters[rng() % 6];
            unsigned chunk = 4 + rng() % 100;
            for (unsigned page = 0; page < readingPages(text, chunk); ++page) {
                auto part = readingPage(text, page, chunk);
                CHECK(!part.empty() && pda::validUtf8(part));
                combined += part;
            }
            CHECK(combined == text);
        }
        std::vector<LedgerEntry> ledger;
        for (unsigned id = 1; id <= 1000; ++id)
            ledger.push_back({id, "2026-01-31", "Conta", "Mensal", "BRL", "", "", -100, true});
        auto original = ledgerCsv(ledger);
        CHECK(!postMonthly(ledger, "2026-02", error));
        CHECK(ledgerCsv(ledger) == original);
        ledger[0].id = UINT32_MAX - 1;
        ledger.resize(1);
        CHECK(!postMonthly(ledger, "2026-02", error));
        CHECK(ledger.size() == 1 && ledger[0].day == "2026-01-31");
        std::cout
            << "5000 malformed input/formula/CSV cases, 200 UTF-8 books and ledger limits OK\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

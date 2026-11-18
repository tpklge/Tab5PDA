#include "memory_files.hpp"
#include "productivity.hpp"
#include <iostream>
using namespace tab5_pda;
int main() {
    try {
        Sheet sheet;
        std::string error;
        double result;
        sheet.cells[0] = "10";
        sheet.cells[1] = "20";
        sheet.cells[2] = "=SUM(A1:B1)*2";
        CHECK(sheet.value(2, result, error) && result == 60);
        sheet.cells[3] = "=C1/0";
        CHECK(!sheet.value(3, result, error));
        sheet.cells[3] = "=D1";
        CHECK(!sheet.value(3, result, error));
        sheet.cells[3] = "=A21";
        CHECK(!sheet.value(3, result, error));
        sheet.cells[3] = "=SOMA(A1:B1)+(-2)";
        CHECK(sheet.value(3, result, error) && result == 28);
        sheet.cells[4] = "Texto, com aspas \"a\"";
        Sheet restored;
        CHECK(restored.load(sheet.csv(), error));
        CHECK(restored.cells == sheet.cells);
        CHECK(!restored.load(std::string(129, 'x'), error));
        Sheet nested;
        nested.cells[0] = "=" + std::string(12, '(') + "B1" + std::string(12, ')');
        nested.cells[1] = "=" + std::string(12, '(') + "C1" + std::string(12, ')');
        nested.cells[2] = "1";
        CHECK(!nested.value(0, result, error));
        std::string text(2199, 'a');
        text += "ç";
        CHECK(readingPages(text) == 1);
        text += "á";
        std::string combined;
        for (unsigned p = 0; p < readingPages(text); ++p) {
            auto page = readingPage(text, p);
            CHECK(!page.empty() && pda::validUtf8(page));
            combined += page;
        }
        CHECK(combined == text);
        std::vector<LedgerEntry> ledger{
            {1, "2026-01-31", "Banco", "Salário", "BRL", "Trabalho", "", 100000, true, false},
            {2, "2026-01-31", "Carteira", "Aluguel", "BRL", "Casa", "", -25000, true, false}};
        CHECK(validLedger(ledger, error));
        CHECK(postMonthly(ledger, "2026-02", error));
        CHECK(ledger.size() == 4);
        CHECK(ledger.back().day == "2026-02-28");
        CHECK(postMonthly(ledger, "2026-02", error) && ledger.size() == 4);
        CHECK(ledgerTotals(ledger, "2026-02")["Banco / BRL"] == 100000);
        CHECK(ledgerTotals(ledger, "2026-02", true)["Carteira / BRL"] == 25000);
        std::vector<LedgerEntry> loaded;
        CHECK(loadLedger(ledgerCsv(ledger), loaded, error));
        CHECK(loaded.size() == 4 && loaded[0].monthly);
        auto bad = ledger;
        bad[1].id = bad[0].id;
        CHECK(!validLedger(bad, error));
        CHECK(!loadLedger("id,date\n1,nope", loaded, error));
        CHECK(loaded.size() == 4);
        std::string png("\x89PNG\r\n\x1a\n", 8);
        png += std::string(4, '\0');
        png += "IHDR";
        png += std::string("\0\0\0\1\0\0\0\1", 8);
        unsigned w, h;
        CHECK(imageDimensions(png, w, h) && w == 1 && h == 1);
        png[16] = 1;
        CHECK(!imageDimensions(png, w, h));
        std::cout << "Sheet formulas, CSV, UTF-8 pages, finance recurrence and image bounds OK\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}

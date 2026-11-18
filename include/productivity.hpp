#pragma once
#include "pda/organizer.hpp"
#include <array>
namespace tab5_pda {
std::string normalizeDocumentText(const std::string &);
bool imageDimensions(const std::string &, unsigned &width, unsigned &height);
struct Sheet {
    static constexpr int rows = 20, cols = 8;
    std::array<std::string, rows * cols> cells{};
    bool value(int cell, double &out, std::string &error) const;
    std::string csv() const;
    bool load(const std::string &, std::string &error);
};
std::string readingPage(const std::string &, unsigned page, unsigned pageBytes = 2200);
unsigned readingPages(const std::string &, unsigned pageBytes = 2200);
struct LedgerEntry {
    uint32_t id = 0;
    std::string day, account, title, currency = "BRL", category, origin;
    int64_t cents = 0;
    bool monthly = false, budget = false;
};
bool validLedger(const std::vector<LedgerEntry> &, std::string &error);
std::string ledgerCsv(const std::vector<LedgerEntry> &);
bool loadLedger(const std::string &, std::vector<LedgerEntry> &, std::string &error);
bool postMonthly(std::vector<LedgerEntry> &, const std::string &month, std::string &error);
std::map<std::string, int64_t> ledgerTotals(const std::vector<LedgerEntry> &,
                                            const std::string &month, bool expenses = false,
                                            const std::string &category = "");
} // namespace tab5_pda

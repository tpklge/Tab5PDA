#pragma once
#include "pda/service.hpp"

namespace tab5_pda {
enum class ExchangeFormat { Calendar, Contacts, Expenses, Text, Markdown };
std::string exportRecords(const pda::State &, ExchangeFormat, std::string &error);
bool parseRecords(const std::string &, ExchangeFormat, const pda::Settings &,
                  std::vector<pda::Record> &, std::string &error);
bool parseCsv(const std::string &, std::vector<std::vector<std::string>> &);
bool previewTable(const std::string &, std::vector<std::vector<std::string>> &, std::string &error);
bool mergeRecords(pda::Service &, std::vector<pda::Record>, std::string &error);
} // namespace tab5_pda

#pragma once
#include "pda/service.hpp"
#include <array>
#include <vector>
namespace tab5_pda {
struct FileEntry {
    std::string name;
    uint32_t bytes = 0;
};
std::array<uint8_t, 44> pcmHeader(uint32_t bytes);
bool pcmSize(const uint8_t *header, std::size_t fileBytes, uint32_t &payload);
bool safeName(const std::string &name);
bool publicFile(const std::string &name);
bool quietNow(const pda::Settings &, const std::string &now);
bool patchRecord(pda::Service &, uint32_t, const std::string &, const std::string &,
                 std::string &error);
bool encodeDraft(const pda::Draft &, const pda::State &, std::string &, std::string &error);
bool decodeDraft(const std::string &, const pda::State &, pda::Draft &, std::string &error);
struct InkPoint {
    int16_t x = 0, y = 0;
};
struct InkStroke {
    uint32_t color = 0;
    uint8_t width = 3;
    std::vector<InkPoint> points;
};
std::string encodeInk(const std::vector<InkStroke> &);
bool decodeInk(const std::string &, std::vector<InkStroke> &, std::string &error);
struct SyncConflict {
    uint32_t id;
    pda::Record remote;
    bool deletion = false;
};
struct SyncPlan {
    pda::State candidate;
    std::vector<SyncConflict> conflicts;
};
bool planSync(const pda::State &local, const pda::State &remote, const pda::State &base, SyncPlan &,
              std::string &error);
bool resolveSync(SyncPlan &, std::size_t conflict, bool useRemote, std::string &error);
std::string syncExport(const pda::State &, std::string &error);
} // namespace tab5_pda

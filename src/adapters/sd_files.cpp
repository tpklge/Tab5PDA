#include "sd_files.hpp"
#include <algorithm>

namespace tab5_pda {
namespace {
std::string physical(const char *path) {
    std::string value(path), prefix = "/CardputerPDA";
    return value.compare(0, prefix.size(), prefix) == 0 ? "/Tab5PDA" + value.substr(prefix.size())
                                                        : value;
}
} // namespace
bool SdFiles::begin() {
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    SD.end();
    mounted_ = false;
    // Tab5 microSD on SPI, separate from the C6 SDIO host.
    spi_.begin(43, 39, 44, 42);
    if (!SD.begin(42, spi_, 20000000, "/sd", 5, false) || SD.cardType() == CARD_NONE)
        return false;
    if (!SD.exists("/Tab5PDA") && !SD.mkdir("/Tab5PDA")) {
        SD.end();
        return false;
    }
    mounted_ = true;
    return true;
}
pda::ReadResult SdFiles::read(const char *path, std::vector<uint8_t> &data) {
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    data.clear();
    if (!mounted_ || SD.cardType() == CARD_NONE)
        return pda::ReadResult::Error;
    // Opening the directory distinguishes an unavailable card from a missing file.
    auto dir = SD.open("/Tab5PDA");
    if (!dir || !dir.isDirectory())
        return pda::ReadResult::Error;
    dir.close();
    if (!SD.exists(physical(path).c_str()))
        return pda::ReadResult::Missing;
    auto file = SD.open(physical(path).c_str(), FILE_READ);
    if (!file || file.isDirectory())
        return pda::ReadResult::Error;
    auto size = file.size();
    if (size > pda::kMaxSnapshotBytes) {
        file.close();
        return pda::ReadResult::Error;
    }
    data.resize(size);
    auto count = file.read(data.data(), size);
    file.close();
    if (count != size) {
        data.clear();
        return pda::ReadResult::Error;
    }
    return pda::ReadResult::Ok;
}
bool SdFiles::write(const char *path, const std::vector<uint8_t> &data) {
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    if (!mounted_ || SD.cardType() == CARD_NONE)
        return false;
    // "w" truncates only the inactive bank. Never remove/rename the active bank.
    auto file = SD.open(physical(path).c_str(), "w");
    if (!file)
        return false;
    std::size_t done = 0;
    while (done < data.size()) {
        auto block = std::min<std::size_t>(512, data.size() - done);
        auto wrote = file.write(data.data() + done, block);
        if (wrote != block) {
            file.close();
            return false;
        }
        done += wrote;
    }
    file.flush();
    file.close();
    return true;
}
std::vector<FileEntry> SdFiles::list() {
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    std::vector<FileEntry> out;
    if (!mounted_)
        return out;
    auto dir = SD.open("/Tab5PDA");
    for (auto f = dir.openNextFile(); f; f = dir.openNextFile()) {
        std::string name = f.name();
        auto slash = name.find_last_of('/');
        if (slash != std::string::npos)
            name.erase(0, slash + 1);
        if (!f.isDirectory() && safeName(name))
            out.push_back({name, static_cast<uint32_t>(f.size())});
        f.close();
        if (out.size() >= 4096)
            break;
    }
    dir.close();
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
    return out;
}
bool SdFiles::remove(const std::string &name) {
    std::lock_guard<std::recursive_mutex> guard(mutex_);
    if (!mounted_ || !safeName(name) || name.rfind("state-", 0) == 0 || name == "assistant.cfg" ||
        name == "sync-base.jsonl")
        return false;
    return SD.remove(("/Tab5PDA/" + name).c_str());
}
} // namespace tab5_pda

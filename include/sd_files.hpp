#pragma once
#include "pda/snapshot_store.hpp"
#include "workspace.hpp"
#include <SD.h>
#include <SPI.h>
#include <mutex>

namespace tab5_pda {
class SdFiles final : public pda::IFiles {
  public:
    bool begin();
    std::vector<FileEntry> list();
    bool remove(const std::string &);
    std::recursive_mutex &mutex() {
        return mutex_;
    }
    pda::ReadResult read(const char *path, std::vector<uint8_t> &data) override;
    bool write(const char *path, const std::vector<uint8_t> &data) override;
    bool mounted() const {
        return mounted_;
    }
    uint64_t cardBytes() const {
        return mounted_ ? SD.cardSize() : 0;
    }

  private:
    std::recursive_mutex mutex_;
    SPIClass spi_{FSPI};
    bool mounted_ = false;
};
} // namespace tab5_pda

#pragma once

#include "pda/model.hpp"

namespace pda {

enum class ReadResult { Missing, Ok, Error };
class IFiles {
  public:
    virtual ~IFiles() = default;
    virtual ReadResult read(const char *path, std::vector<uint8_t> &data) = 0;
    virtual bool write(const char *path, const std::vector<uint8_t> &data) = 0;
};

enum class DecodeResult { Ok, Corrupt, Unsupported };
uint32_t crc32(const uint8_t *data, std::size_t size);
bool encodeSnapshot(const State &state, uint64_t generation, std::vector<uint8_t> &data,
                    std::string &error);
DecodeResult decodeSnapshot(const std::vector<uint8_t> &data, State &state, uint64_t &generation);

class SnapshotStore {
  public:
    explicit SnapshotStore(IFiles &files) : files_(files) {}
    bool load(State &state);
    bool save(const State &state);
    bool backup(const State &state);
    bool dailyBackup(const State &state, const std::string &day);
    bool readFile(const std::string &name, std::string &text, ReadResult *result = nullptr);
    bool writeFile(const std::string &name, const std::string &text);
    bool readBackup(State &state, bool incoming = false);
    void unavailable(const std::string &reason);
    bool writable() const {
        return writable_;
    }
    uint64_t generation() const {
        return generation_;
    }
    const std::string &message() const {
        return message_;
    }
    static const char *path(int bank);

  private:
    IFiles &files_;
    int active_ = -1;
    uint64_t generation_ = 0;
    bool writable_ = false;
    std::string message_;
};

} // namespace pda

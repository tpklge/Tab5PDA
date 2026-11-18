#pragma once

#include "pda/snapshot_store.hpp"

namespace pda {

class Service {
  public:
    explicit Service(SnapshotStore &store) : store_(store) {}
    bool load();
    bool reloadForEdit();
    bool saveRecord(Record record);
    bool deleteRecord(uint32_t id);
    bool trashRecord(uint32_t id, bool restore = false);
    bool readFile(const std::string &name, std::string &text, ReadResult *result = nullptr);
    bool writeFile(const std::string &name, const std::string &text);
    bool dailyBackup(const std::string &day);
    bool toggleTask(uint32_t id, const std::string &completedAt = "");
    bool saveSettings(const Settings &settings);
    bool saveDraft(const Draft &draft);
    bool clearDraft();
    bool acknowledgeAlarm(const std::string &key, const std::string &until);
    bool skipOccurrence(uint32_t id, const std::string &day);
    bool restore(const State &state);
    bool backup();
    bool restoreBackup(bool incoming = false);
    const State &state() const {
        return state_;
    }
    bool writable() const {
        return store_.writable();
    }
    const std::string &error() const {
        return error_;
    }
    const std::string &storageMessage() const {
        return store_.message();
    }
    uint64_t generation() const {
        return store_.generation();
    }

  private:
    bool commit(State &&candidate);
    SnapshotStore &store_;
    State state_;
    std::string error_;
};

bool calculate(const std::string &expression, double &result, std::string &error);

} // namespace pda

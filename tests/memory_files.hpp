#pragma once
#include "pda/snapshot_store.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr))                                                                               \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) +      \
                                     ": " #expr);                                                  \
    } while (false)

struct MemoryFiles final : pda::IFiles {
    std::map<std::string, std::vector<uint8_t>> files;
    bool readFailure = false, writeFailure = false, shortWrite = false;
    int writes = 0;
    pda::ReadResult read(const char *path, std::vector<uint8_t> &data) override {
        if (readFailure)
            return pda::ReadResult::Error;
        auto it = files.find(path);
        if (it == files.end())
            return pda::ReadResult::Missing;
        data = it->second;
        return pda::ReadResult::Ok;
    }
    bool write(const char *path, const std::vector<uint8_t> &data) override {
        ++writes;
        if (writeFailure)
            return false;
        files[path] = data;
        if (shortWrite)
            files[path].resize(data.size() / 2);
        return true;
    }
};

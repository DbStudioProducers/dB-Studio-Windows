#pragma once

#include <array>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <mutex>
#include <string>
#include <utility>
#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

class AuxAssignmentRegistry final {
public:
    static constexpr std::size_t kMixCount = 16;
    enum class Result { success, invalidDeviceId, invalidMix, deviceAlreadyAssigned,
                        mixAlreadyAssigned, deviceNotFound, persistenceError, invalidData };
    struct Assignment {
        std::string deviceId;
        std::string displayName;
        std::size_t mixIndex = 0;
    };

    Result assign(const std::string& deviceId, const std::string& displayName, std::size_t mixIndex) {
        if (deviceId.empty()) return Result::invalidDeviceId;
        if (mixIndex >= kMixCount) return Result::invalidMix;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& item : assignments_)
            if (item.deviceId == deviceId) return Result::deviceAlreadyAssigned;
        if (!assignments_[mixIndex].deviceId.empty()) return Result::mixAlreadyAssigned;
        assignments_[mixIndex] = Assignment{deviceId, displayName, mixIndex};
        return Result::success;
    }

    Result renameDevice(const std::string& deviceId, const std::string& displayName) {
        if (deviceId.empty()) return Result::invalidDeviceId;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& item : assignments_) if (item.deviceId == deviceId) {
            item.displayName = displayName;
            return Result::success;
        }
        return Result::deviceNotFound;
    }

    Result unassignDevice(const std::string& deviceId) {
        if (deviceId.empty()) return Result::invalidDeviceId;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& item : assignments_) if (item.deviceId == deviceId) {
            item = Assignment{};
            return Result::success;
        }
        return Result::deviceNotFound;
    }

    bool findByDevice(const std::string& deviceId, Assignment& result) const {
        if (deviceId.empty()) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& item : assignments_) if (item.deviceId == deviceId) {
            result = item; return true;
        }
        return false;
    }

    bool findByMix(std::size_t mixIndex, Assignment& result) const {
        if (mixIndex >= kMixCount) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto& item = assignments_[mixIndex];
        if (item.deviceId.empty()) return false;
        result = item; return true;
    }

    std::size_t assignedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (const auto& item : assignments_) if (!item.deviceId.empty()) ++count;
        return count;
    }

    // Versioned, local-only format. Text fields are hex encoded to preserve all UTF-8 bytes.
    Result saveToFile(const std::string& path) const {
        if (path.empty()) return Result::persistenceError;
        std::array<Assignment, kMixCount> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = assignments_;
        }
        // Interpret incoming paths as UTF-8 on every platform. On Windows,
        // filesystem::path stores UTF-16 so non-ASCII account names remain valid.
        const auto destination = std::filesystem::u8path(path);
        auto temporary = destination;
        temporary += std::filesystem::path::string_type{
#ifdef _WIN32
            L".tmp"
#else
            ".tmp"
#endif
        };
        std::error_code filesystemError;
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            if (!out) return Result::persistenceError;
            out << "DBSAUX1\n";
            for (std::size_t i = 0; i < kMixCount; ++i) {
                const auto& a = snapshot[i];
                if (a.deviceId.empty()) continue;
                out << i << '\t' << encodeHex(a.deviceId) << '\t' << encodeHex(a.displayName) << '\n';
            }
            out.flush();
            if (!out.good()) {
                out.close();
                std::filesystem::remove(temporary, filesystemError);
                return Result::persistenceError;
            }
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, filesystemError);
            return Result::persistenceError;
        }
#else
        std::filesystem::rename(temporary, destination, filesystemError);
        if (filesystemError) {
            std::filesystem::remove(temporary, filesystemError);
            return Result::persistenceError;
        }
#endif
        return Result::success;
    }

    // Loads transactionally: invalid files never partially modify the active registry.
    Result loadFromFile(const std::string& path) {
        std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
        if (!in) return Result::persistenceError;
        std::string line;
        if (!std::getline(in, line) || line != "DBSAUX1") return Result::invalidData;
        std::array<Assignment, kMixCount> loaded{};
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            const auto first = line.find('\t');
            const auto second = first == std::string::npos ? first : line.find('\t', first + 1);
            if (first == std::string::npos || second == std::string::npos ||
                line.find('\t', second + 1) != std::string::npos) return Result::invalidData;
            std::size_t index = 0;
            try {
                std::size_t used = 0;
                index = static_cast<std::size_t>(std::stoul(line.substr(0, first), &used));
                if (used != first) return Result::invalidData;
            } catch (...) { return Result::invalidData; }
            if (index >= kMixCount || !loaded[index].deviceId.empty()) return Result::invalidData;
            std::string id, name;
            if (!decodeHex(line.substr(first + 1, second - first - 1), id) ||
                !decodeHex(line.substr(second + 1), name) || id.empty()) return Result::invalidData;
            for (const auto& existing : loaded)
                if (existing.deviceId == id) return Result::invalidData;
            loaded[index] = Assignment{std::move(id), std::move(name), index};
        }
        if (in.bad()) return Result::persistenceError;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            assignments_ = std::move(loaded);
        }
        return Result::success;
    }

private:
    static std::string encodeHex(const std::string& value) {
        static const char digits[] = "0123456789ABCDEF";
        std::string out; out.reserve(value.size() * 2);
        for (unsigned char c : value) { out.push_back(digits[c >> 4]); out.push_back(digits[c & 15]); }
        return out;
    }
    static bool decodeHex(const std::string& value, std::string& out) {
        if (value.size() % 2 != 0) return false;
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1;
        };
        out.clear(); out.reserve(value.size() / 2);
        for (std::size_t i = 0; i < value.size(); i += 2) {
            const int hi = nibble(value[i]), lo = nibble(value[i + 1]);
            if (hi < 0 || lo < 0) return false;
            out.push_back(static_cast<char>((hi << 4) | lo));
        }
        return true;
    }

    mutable std::mutex mutex_;
    std::array<Assignment, kMixCount> assignments_{};
};

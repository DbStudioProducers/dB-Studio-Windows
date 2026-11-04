#pragma once

#include <array>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>

class AuxAssignmentRegistry final {
public:
    static constexpr std::size_t kMixCount = 16;

    enum class Result {
        success,
        invalidDeviceId,
        invalidMix,
        deviceAlreadyAssigned,
        mixAlreadyAssigned,
        deviceNotFound
    };

    struct Assignment {
        std::string deviceId;
        std::string displayName;
        std::size_t mixIndex = 0;
    };

    Result assign(const std::string& deviceId, const std::string& displayName,
                  std::size_t mixIndex) {
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
        for (auto& item : assignments_) {
            if (item.deviceId == deviceId) {
                item.displayName = displayName;
                return Result::success;
            }
        }
        return Result::deviceNotFound;
    }

    Result unassignDevice(const std::string& deviceId) {
        if (deviceId.empty()) return Result::invalidDeviceId;
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& item : assignments_) {
            if (item.deviceId == deviceId) {
                item = Assignment{};
                return Result::success;
            }
        }
        return Result::deviceNotFound;
    }

    bool findByDevice(const std::string& deviceId, Assignment& result) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& item : assignments_) {
            if (item.deviceId == deviceId) {
                result = item;
                return true;
            }
        }
        return false;
    }

    bool findByMix(std::size_t mixIndex, Assignment& result) const {
        if (mixIndex >= kMixCount) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto& item = assignments_[mixIndex];
        if (item.deviceId.empty()) return false;
        result = item;
        return true;
    }

    std::size_t assignedCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (const auto& item : assignments_)
            if (!item.deviceId.empty()) ++count;
        return count;
    }

private:
    mutable std::mutex mutex_;
    std::array<Assignment, kMixCount> assignments_{};
};

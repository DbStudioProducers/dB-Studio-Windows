#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <algorithm>

namespace DbStudioAudio {

// Fixed-capacity SPSC handoff. Exactly one producer may call tryPush and
// exactly one consumer may call tryAcquire/release. No locks or allocations.
template <std::size_t Channels, std::size_t MaxFrames, std::size_t Slots>
class SpscAudioBlockQueue {
    static_assert(Channels > 0 && MaxFrames > 0 && Slots > 1, "Invalid queue dimensions");
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "Real-time queue requires lock-free 64-bit atomics on the target architecture.");
public:
    struct BlockView {
        const float* samples = nullptr; // channel-major, MaxFrames stride
        int frames = 0;
        std::uint64_t sequence = 0;
        std::uint64_t timestampFrames = 0;
    };

    static constexpr std::size_t kChannels = Channels;
    static constexpr std::size_t kMaxFrames = MaxFrames;

    bool tryPush(const float* const* channels, int frames,
                 std::uint64_t timestampFrames = 0) noexcept {
        if (channels == nullptr || frames <= 0 || static_cast<std::size_t>(frames) > MaxFrames)
            return false;
        for (std::size_t ch = 0; ch < Channels; ++ch)
            if (channels[ch] == nullptr) return false;
        const auto write = write_.load(std::memory_order_relaxed);
        const auto read = read_.load(std::memory_order_acquire);
        if (write - read >= Slots) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        auto& block = blocks_[write % Slots];
        block.frames = frames;
        block.sequence = nextSequence_++;
        block.timestampFrames = timestampFrames;
        for (std::size_t ch = 0; ch < Channels; ++ch) {
            std::copy_n(channels[ch], frames, block.samples.data() + ch * MaxFrames);
        }
        write_.store(write + 1, std::memory_order_release);
        return true;
    }

    bool tryAcquire(BlockView& view) noexcept {
        if (acquired_) return false;
        const auto read = read_.load(std::memory_order_relaxed);
        const auto write = write_.load(std::memory_order_acquire);
        if (read == write) return false;
        const auto& block = blocks_[read % Slots];
        view = {block.samples.data(), block.frames, block.sequence, block.timestampFrames};
        acquired_ = true;
        return true;
    }

    void release() noexcept {
        if (!acquired_) return;
        read_.fetch_add(1, std::memory_order_release);
        acquired_ = false;
    }

    std::uint64_t droppedBlocks() const noexcept {
        return dropped_.load(std::memory_order_relaxed);
    }

private:
    struct Block {
        std::array<float, Channels * MaxFrames> samples{};
        int frames = 0;
        std::uint64_t sequence = 0;
        std::uint64_t timestampFrames = 0;
    };
    std::array<Block, Slots> blocks_{};
    alignas(64) std::atomic<std::uint64_t> write_{0};
    alignas(64) std::atomic<std::uint64_t> read_{0};
    std::uint64_t nextSequence_ = 0; // producer-owned
    std::atomic<std::uint64_t> dropped_{0};
    bool acquired_ = false; // consumer-owned
};

} // namespace DbStudioAudio


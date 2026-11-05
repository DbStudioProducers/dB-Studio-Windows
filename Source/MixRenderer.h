#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace DbStudioMix {
constexpr int kInputs = 32;
constexpr int kMixes = 16;
constexpr int kBuses = kMixes * 2;
constexpr int kMaxFrames = 8192;
using BusBuffers = std::array<std::array<float, kMaxFrames>, kBuses>;
using Gains = std::array<std::array<float, kInputs>, kMixes>;
using Enabled = std::array<std::array<bool, kInputs>, kMixes>;
using Peaks = std::array<float, kMixes>;

// Pure, allocation-free renderer shared by the real-time engine and tests.
// Input pointers may be null; invalid samples are treated as silence.
inline void render(const float* const* inputs, int availableInputs, int frames,
                   const Gains& leftGain, const Gains& rightGain,
                   const Enabled& enabled, BusBuffers& buses, Peaks& peaks) noexcept {
    const int count = std::clamp(availableInputs, 0, kInputs);
    if (frames <= 0 || frames > kMaxFrames) return;
    for (int mix = 0; mix < kMixes; ++mix) {
        float* const left = buses[static_cast<std::size_t>(mix * 2)].data();
        float* const right = buses[static_cast<std::size_t>(mix * 2 + 1)].data();
        float peak = 0.0f;
        for (int frame = 0; frame < frames; ++frame) {
            float l = 0.0f, r = 0.0f;
            for (int input = 0; input < count; ++input) {
                if (!enabled[static_cast<std::size_t>(mix)][static_cast<std::size_t>(input)] ||
                    inputs == nullptr || inputs[input] == nullptr) continue;
                const float raw = inputs[input][frame];
                const float sample = std::isfinite(raw) ? raw : 0.0f;
                l += sample * leftGain[static_cast<std::size_t>(mix)][static_cast<std::size_t>(input)];
                r += sample * rightGain[static_cast<std::size_t>(mix)][static_cast<std::size_t>(input)];
            }
            l = std::clamp(l, -0.98f, 0.98f);
            r = std::clamp(r, -0.98f, 0.98f);
            left[frame] = l;
            right[frame] = r;
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
        }
        peaks[static_cast<std::size_t>(mix)] = peak;
    }
}
}

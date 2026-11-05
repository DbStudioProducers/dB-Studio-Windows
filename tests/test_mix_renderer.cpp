#include "../Source/MixRenderer.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::cerr << "FAIL: " << #condition << " at line " << __LINE__ << std::endl; \
        return 1; \
    } \
} while (false)

namespace {
bool near(float actual, float expected, float tolerance = 0.0001f) {
    return std::abs(actual - expected) <= tolerance;
}
}

int main() {
    using namespace DbStudioMix;
    constexpr int frames = 4;
    std::array<std::array<float, frames>, kInputs> inputData{};
    std::array<const float*, kInputs> inputs{};
    for (int i = 0; i < kInputs; ++i)
        inputs[static_cast<std::size_t>(i)] = inputData[static_cast<std::size_t>(i)].data();

    inputData[0] = {0.5f, -0.5f, 2.0f, std::numeric_limits<float>::quiet_NaN()};
    inputData[1] = {0.25f, 0.25f, 0.25f, std::numeric_limits<float>::infinity()};

    Gains left{}, right{};
    Enabled enabled{};
    for (int mix = 0; mix < kMixes; ++mix) {
        for (int ch = 0; ch < kInputs; ++ch) {
            left[static_cast<std::size_t>(mix)][static_cast<std::size_t>(ch)] = 1.0f;
            right[static_cast<std::size_t>(mix)][static_cast<std::size_t>(ch)] = 1.0f;
        }
    }

    // Keep the 1 MiB bus matrix off the Windows 1 MiB default thread stack.
    static BusBuffers buses{};
    Peaks peaks{};

    // Independent routing: each mix only receives explicitly enabled inputs.
    enabled[0][0] = true;
    enabled[1][1] = true;
    enabled[2][0] = true;
    enabled[2][1] = true;
    render(inputs.data(), kInputs, frames, left, right, enabled, buses, peaks);
    CHECK(near(buses[0][0], 0.5f) && near(buses[1][0], 0.5f));
    CHECK(near(buses[2][0], 0.25f) && near(buses[3][0], 0.25f));
    CHECK(near(buses[4][0], 0.75f) && near(buses[5][0], 0.75f));
    CHECK(near(buses[6][0], 0.0f) && near(buses[7][0], 0.0f));
    // Mix 0 contains a 2.0 sample that is clipped to 0.98 in a later frame.
    CHECK(near(peaks[0], 0.98f) && near(peaks[1], 0.25f));

    // Per-side gain and pan law are supplied by the engine and applied independently.
    enabled[3][0] = true;
    left[3][0] = 0.5f;
    right[3][0] = 0.25f;
    render(inputs.data(), kInputs, frames, left, right, enabled, buses, peaks);
    CHECK(near(buses[6][0], 0.25f));
    CHECK(near(buses[7][0], 0.125f));

    // Multiple inputs sum; hard ceiling protects the bus, and NaN/Inf become silence.
    enabled[4][0] = true;
    enabled[4][1] = true;
    render(inputs.data(), kInputs, frames, left, right, enabled, buses, peaks);
    CHECK(near(buses[8][0], 0.75f));
    CHECK(near(buses[8][2], 0.98f));
    CHECK(near(buses[8][3], 0.0f));
    CHECK(near(buses[9][3], 0.0f));
    CHECK(near(peaks[4], 0.98f));

    // Disabled channels and null input pointers must contribute silence.
    enabled[5][0] = true;
    enabled[5][1] = false;
    std::array<const float*, kInputs> partialInputs = inputs;
    partialInputs[0] = nullptr;
    render(partialInputs.data(), kInputs, frames, left, right, enabled, buses, peaks);
    CHECK(near(buses[10][0], 0.0f));
    render(nullptr, kInputs, frames, left, right, enabled, buses, peaks);
    CHECK(near(buses[10][0], 0.0f));

    // Invalid input counts are bounded safely.
    render(inputs.data(), -5, frames, left, right, enabled, buses, peaks);
    CHECK(near(buses[10][0], 0.0f));
    render(inputs.data(), kInputs + 10, frames, left, right, enabled, buses, peaks);
    CHECK(near(buses[10][0], 0.5f));

    // Invalid frame counts leave existing output untouched.
    buses[10][0] = 0.42f;
    render(inputs.data(), kInputs, 0, left, right, enabled, buses, peaks);
    CHECK(near(buses[10][0], 0.42f));
    render(inputs.data(), kInputs, kMaxFrames + 1, left, right, enabled, buses, peaks);
    CHECK(near(buses[10][0], 0.42f));
    return 0;
}

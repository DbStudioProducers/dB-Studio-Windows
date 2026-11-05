#include "../Source/MixRenderer.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "FAIL: " << #condition << " at line " << __LINE__ << std::endl; return 1; } } while (false)

int main() {
    using namespace DbStudioMix;
    constexpr int frames = 4;
    std::array<std::array<float, frames>, kInputs> inputData{};
    std::array<const float*, kInputs> inputs{};
    for (int i = 0; i < kInputs; ++i) inputs[static_cast<std::size_t>(i)] = inputData[static_cast<std::size_t>(i)].data();
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
    enabled[0][0] = true;
    enabled[1][1] = true;
    enabled[2][0] = true;
    enabled[2][1] = true;
    enabled[3][0] = true;
    left[3][0] = 0.5f;
    right[3][0] = 0.25f;

    // Keep the 1 MiB bus matrix off the Windows 1 MiB default thread stack.
    static BusBuffers buses{};
    Peaks peaks{};
    render(inputs.data(), kInputs, frames, left, right, enabled, buses, peaks);
    CHECK(buses[0][0] == 0.5f && buses[1][0] == 0.5f);
    CHECK(buses[0][1] == -0.5f);
    CHECK(buses[0][2] == 0.98f); // hard safety ceiling
    CHECK(buses[0][3] == 0.0f);  // NaN becomes silence
    CHECK(buses[1][3] == 0.0f);
    CHECK(buses[2][0] == 0.25f && buses[3][0] == 0.25f);
    CHECK(buses[4][0] == 0.75f && buses[5][0] == 0.75f);
    CHECK(buses[6][0] == 0.25f && buses[7][0] == 0.125f);
    CHECK(buses[8][0] == 0.0f && buses[9][0] == 0.0f); // independent buses
    CHECK(std::abs(peaks[0] - 0.98f) < 0.0001f);
    CHECK(std::abs(peaks[1] - 0.25f) < 0.0001f);

    // Null inputs, no inputs, and oversized blocks must not crash or write.
    std::array<const float*, kInputs> nullInputs{};
    render(nullInputs.data(), kInputs, frames, left, right, enabled, buses, peaks);
    CHECK(buses[0][0] == 0.0f);
    buses[0][0] = 0.42f;
    render(inputs.data(), kInputs, kMaxFrames + 1, left, right, enabled, buses, peaks);
    CHECK(buses[0][0] == 0.42f);
    return 0;
}

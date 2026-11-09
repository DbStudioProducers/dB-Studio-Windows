#include "../Source/AudioPacketCodec.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL: " << #x << " line " << __LINE__ << std::endl; return 1; } } while (false)

int main() {
    using namespace DbStudioNet;
    PacketMetadata meta{};
    for (std::size_t i = 0; i < meta.sessionId.size(); ++i)
        meta.sessionId[i] = static_cast<std::uint8_t>(i + 1);
    meta.streamId = 15;
    meta.sequence = 0x10203040u;
    meta.timestampFrames = 0x0102030405060708ull;
    meta.sampleRate = 48000;
    meta.frames = 128;

    std::array<float, kMaxFramesPerPacket> left{}, right{}, decodedLeft{}, decodedRight{};
    for (std::size_t i = 0; i < kMaxFramesPerPacket; ++i) {
        left[i] = static_cast<float>(i) / 128.0f - 0.5f;
        right[i] = 0.5f - static_cast<float>(i) / 256.0f;
    }
    std::array<std::uint8_t, kMaxDatagramBytes> packet{};
    std::size_t written = 0;
    CHECK(encodeAudioPacket(meta, left.data(), right.data(), packet.data(), packet.size(), written) == PacketResult::success);
    CHECK(written == kHeaderBytes + 128u * 8u);
    CHECK(written <= kMaxDatagramBytes);
    CHECK(packet[0] == 'D' && packet[1] == 'B' && packet[2] == 'S' && packet[3] == 'A');
    CHECK(packet[4] == 1 && packet[24] == 15);

    PacketMetadata restored{};
    CHECK(decodeAudioPacket(packet.data(), written, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::success);
    CHECK(restored.sessionId == meta.sessionId);
    CHECK(restored.streamId == meta.streamId && restored.sequence == meta.sequence);
    CHECK(restored.timestampFrames == meta.timestampFrames && restored.sampleRate == 48000 && restored.frames == 128);
    for (std::size_t i = 0; i < 128; ++i) {
        CHECK(std::abs(decodedLeft[i] - left[i]) < 1.0e-7f);
        CHECK(std::abs(decodedRight[i] - right[i]) < 1.0e-7f);
    }

    CHECK(encodeAudioPacket(meta, nullptr, right.data(), packet.data(), packet.size(), written) == PacketResult::invalidArgument);
    CHECK(encodeAudioPacket(meta, left.data(), right.data(), packet.data(), 100, written) == PacketResult::insufficientCapacity);
    auto invalid = meta; invalid.streamId = 16;
    CHECK(encodeAudioPacket(invalid, left.data(), right.data(), packet.data(), packet.size(), written) == PacketResult::invalidMetadata);
    invalid = meta; invalid.sampleRate = 384000;
    CHECK(encodeAudioPacket(invalid, left.data(), right.data(), packet.data(), packet.size(), written) == PacketResult::invalidMetadata);
    invalid = meta; invalid.frames = 129;
    CHECK(encodeAudioPacket(invalid, left.data(), right.data(), packet.data(), packet.size(), written) == PacketResult::invalidFrameCount);
    invalid = meta; invalid.sessionId.fill(0);
    CHECK(encodeAudioPacket(invalid, left.data(), right.data(), packet.data(), packet.size(), written) == PacketResult::invalidMetadata);

    auto malformed = packet;
    malformed[4] = 2;
    CHECK(decodeAudioPacket(malformed.data(), written, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::invalidPacket);
    malformed = packet; malformed[5] = 1;
    CHECK(decodeAudioPacket(malformed.data(), written, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::invalidPacket);
    malformed = packet; malformed[24] = 16;
    CHECK(decodeAudioPacket(malformed.data(), written, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::invalidMetadata);
    CHECK(decodeAudioPacket(packet.data(), written - 1, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::invalidPacket);
    CHECK(decodeAudioPacket(packet.data(), written, restored, decodedLeft.data(), decodedRight.data(), 127) == PacketResult::insufficientCapacity);

    left[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(encodeAudioPacket(meta, left.data(), right.data(), packet.data(), packet.size(), written) == PacketResult::nonFiniteSample);
    left[0] = 0.0f;
    CHECK(encodeAudioPacket(meta, left.data(), right.data(), packet.data(), packet.size(), written) == PacketResult::success);
    auto nanPacket = packet;
    nanPacket[kHeaderBytes] = 0; nanPacket[kHeaderBytes + 1] = 0;
    nanPacket[kHeaderBytes + 2] = 0xc0; nanPacket[kHeaderBytes + 3] = 0x7f;
    CHECK(decodeAudioPacket(nanPacket.data(), written, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::nonFiniteSample);

    CHECK(decodeAudioPacket(nullptr, written, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::invalidArgument);
    CHECK(decodeAudioPacket(packet.data(), kMaxDatagramBytes + 1, restored, decodedLeft.data(), decodedRight.data(), decodedLeft.size()) == PacketResult::invalidPacket);
    return 0;
}

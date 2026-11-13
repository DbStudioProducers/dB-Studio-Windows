#include "../Source/IemAudioPacketPump.h"
#include "../Source/SpscAudioBlockQueue.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL: " << #x << " line " << __LINE__ << std::endl; return 1; } } while (false)

namespace {
using Queue = DbStudioAudio::SpscAudioBlockQueue<32, 256, 3>;

struct DatagramCapture {
    std::vector<std::vector<std::uint8_t>> packets;
    std::size_t failAfter = static_cast<std::size_t>(-1);

    bool sendDatagram(const std::uint8_t* data, std::size_t size) noexcept {
        if (packets.size() >= failAfter)
            return false;
        packets.emplace_back(data, data + size);
        return true;
    }
};

void authorize(DbStudioSecurity::TransportSecurityGate& gate) {
    gate.beginHandshake();
    gate.markSecureTransportEstablished(true);
    gate.markDeviceAuthenticated(true);
    gate.markAuthorized(true);
}

void fillAndPush(Queue& queue, std::array<std::array<float, 256>, 32>& source,
                 int frames, std::uint64_t timestamp) {
    std::array<const float*, 32> channels{};
    for (std::size_t channel = 0; channel < source.size(); ++channel) {
        channels[channel] = source[channel].data();
        for (int frame = 0; frame < frames; ++frame)
            source[channel][static_cast<std::size_t>(frame)] =
                static_cast<float>((channel + 1) * 0.01 + frame * 0.0001);
    }
    queue.tryPush(channels.data(), frames, timestamp);
}
}

int main() {
    std::array<std::uint8_t, 16> sessionId{};
    sessionId[0] = 0x42;
    Queue queue;
    std::array<std::array<float, 256>, 32> source{};
    DatagramCapture sink;
    DbStudioNet::IemAudioPacketPump pump(sessionId, 48000);
    DbStudioSecurity::TransportSecurityGate gate;

    CHECK(pump.pumpOne(queue, sink, gate, 0xffffu) == DbStudioNet::AudioPumpResult::noBlock);
    fillAndPush(queue, source, 16, 4096);
    CHECK(pump.pumpOne(queue, sink, gate, 0xffffu) == DbStudioNet::AudioPumpResult::denied);
    CHECK(sink.packets.empty()); // unauthorized audio is consumed and discarded

    authorize(gate);
    Queue noAUXQueue;
    fillAndPush(noAUXQueue, source, 16, 6144);
    CHECK(pump.pumpOne(noAUXQueue, sink, gate, 0) == DbStudioNet::AudioPumpResult::denied);
    CHECK(sink.packets.empty()); // no server-authorized AUX means no IEM payloads

    fillAndPush(queue, source, 256, 8192);
    CHECK(pump.pumpOne(queue, sink, gate, 0xffffu) == DbStudioNet::AudioPumpResult::sent);
    CHECK(sink.packets.size() == 32); // two <=128-frame datagrams for each AUX

    for (std::size_t i = 0; i < sink.packets.size(); ++i) {
        DbStudioNet::PacketMetadata metadata{};
        std::array<float, DbStudioNet::kMaxFramesPerPacket> left{}, right{};
        const auto& packet = sink.packets[i];
        CHECK(DbStudioNet::decodeAudioPacket(packet.data(), packet.size(), metadata,
                                               left.data(), right.data(), left.size())
              == DbStudioNet::PacketResult::success);
        CHECK(metadata.streamId == i / 2);
        CHECK(metadata.sequence == i % 2);
        CHECK(metadata.timestampFrames == 8192 + (i % 2) * 128);
        CHECK(metadata.sampleRate == 48000);
        CHECK(metadata.frames == 128);
        const auto channel = static_cast<std::size_t>(metadata.streamId) * 2;
        const auto sourceFrame = static_cast<int>(i % 2) * 128;
        CHECK(std::abs(left[0] - static_cast<float>((channel + 1) * 0.01 + sourceFrame * 0.0001)) < 1.0e-6f);
        CHECK(std::abs(right[0] - static_cast<float>((channel + 2) * 0.01 + sourceFrame * 0.0001)) < 1.0e-6f);
    }

    Queue restrictedQueue;
    DatagramCapture restrictedSink;
    DbStudioNet::IemAudioPacketPump restrictedPump(sessionId, 48000);
    fillAndPush(restrictedQueue, source, 256, 12288);
    CHECK(restrictedPump.pumpOne(restrictedQueue, restrictedSink, gate,
                                 std::uint16_t{1} << 7)
          == DbStudioNet::AudioPumpResult::sent);
    CHECK(restrictedSink.packets.size() == 2);
    for (const auto& packet : restrictedSink.packets) {
        DbStudioNet::PacketMetadata metadata{};
        std::array<float, DbStudioNet::kMaxFramesPerPacket> left{}, right{};
        CHECK(DbStudioNet::decodeAudioPacket(packet.data(), packet.size(), metadata,
                                               left.data(), right.data(), left.size())
              == DbStudioNet::PacketResult::success);
        CHECK(metadata.streamId == 7); // an AUX grant cannot leak any other bus
    }

    Queue failingQueue;
    DatagramCapture failingSink;
    failingSink.failAfter = 2;
    DbStudioNet::IemAudioPacketPump failingPump(sessionId, 48000);
    fillAndPush(failingQueue, source, 16, 16384);
    CHECK(failingPump.pumpOne(failingQueue, failingSink, gate, 0xffffu)
          == DbStudioNet::AudioPumpResult::sinkFailure);
    CHECK(failingSink.packets.size() == 2);
    Queue::BlockView view{};
    CHECK(!failingQueue.tryAcquire(view)); // failed sends cannot pin the audio slot

    return 0;
}


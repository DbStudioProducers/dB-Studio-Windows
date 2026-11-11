#pragma once

#include "AudioPacketCodec.h"
#include "TransportSecurityPolicy.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace DbStudioNet {

enum class AudioPumpResult {
    noBlock,
    sent,
    denied,
    invalidBlock,
    sinkFailure,
    encodeFailure,
};

// Runs on the network worker, never on the JUCE audio callback. The sink must
// synchronously copy each datagram into transport-owned bounded storage.
// Queue release is guaranteed on every return path.
class IemAudioPacketPump final {
public:
    explicit IemAudioPacketPump(const std::array<std::uint8_t, 16>& sessionId,
                                std::uint32_t sampleRate) noexcept
        : encoder_(sessionId, sampleRate) {}

    template <typename BlockSource, typename DatagramSink>
    AudioPumpResult pumpOne(BlockSource& source, DatagramSink& sink,
                            const DbStudioSecurity::TransportSecurityGate& gate,
                            std::uint16_t authorizedStreamMask) noexcept {
        static_assert(BlockSource::kChannels == 32,
                      "IEM transport expects 16 stereo AUX buses");
        typename BlockSource::BlockView block{};
        if (!source.tryAcquire(block))
            return AudioPumpResult::noBlock;

        struct ReleaseOnExit {
            BlockSource& source;
            ~ReleaseOnExit() { source.release(); }
        } release{source};

        if (authorizedStreamMask == 0 ||
            !gate.maySend(DbStudioSecurity::DataClass::audioDatagram, false))
            return AudioPumpResult::denied;
        if (block.samples == nullptr || block.frames <= 0 ||
            static_cast<std::size_t>(block.frames) > BlockSource::kMaxFrames)
            return AudioPumpResult::invalidBlock;

        std::array<std::uint8_t, kMaxDatagramBytes> packet{};
        for (std::uint8_t streamId = 0; streamId < 16; ++streamId) {
            if ((authorizedStreamMask & (std::uint16_t{1} << streamId)) == 0)
                continue;
            for (std::size_t offset = 0; offset < static_cast<std::size_t>(block.frames);) {
                std::size_t packetSize = 0;
                const auto result = encoder_.encodeBlockPacket(
                    block.samples, BlockSource::kChannels, BlockSource::kMaxFrames,
                    static_cast<std::size_t>(block.frames), offset, streamId,
                    block.timestampFrames, packet.data(), packet.size(), packetSize);
                if (result != PacketResult::success)
                    return AudioPumpResult::encodeFailure;
                if (!sink.sendDatagram(packet.data(), packetSize))
                    return AudioPumpResult::sinkFailure;
                offset += packetSize > kHeaderBytes
                    ? (packetSize - kHeaderBytes) / 8 : 0;
            }
        }
        return AudioPumpResult::sent;
    }

private:
    AudioPacketStreamEncoder encoder_;
};

} // namespace DbStudioNet


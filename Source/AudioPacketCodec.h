#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <limits>
namespace DbStudioNet {
static_assert(sizeof(float) == 4, "Audio packet v1 requires 32-bit float");
static_assert(std::numeric_limits<float>::is_iec559, "Audio packet v1 requires IEEE-754 float");
constexpr std::size_t kHeaderBytes=48, kMaxDatagramBytes=1200, kMaxFramesPerPacket=128, kChannelsPerStream=2;
constexpr std::uint8_t kProtocolVersion=1, kCodecFloat32=1;
struct PacketMetadata { std::array<std::uint8_t,16> sessionId{}; std::uint8_t streamId=0; std::uint32_t sequence=0; std::uint64_t timestampFrames=0; std::uint32_t sampleRate=48000; std::uint16_t frames=0; };
enum class PacketResult { success, invalidArgument, invalidMetadata, invalidFrameCount, invalidPacket, insufficientCapacity, nonFiniteSample };
namespace detail {
inline void put16be(std::uint8_t*p,std::uint16_t v) noexcept {p[0]=static_cast<std::uint8_t>(v>>8);p[1]=static_cast<std::uint8_t>(v);}
inline void put32be(std::uint8_t*p,std::uint32_t v) noexcept {p[0]=static_cast<std::uint8_t>(v>>24);p[1]=static_cast<std::uint8_t>(v>>16);p[2]=static_cast<std::uint8_t>(v>>8);p[3]=static_cast<std::uint8_t>(v);}
inline void put64be(std::uint8_t*p,std::uint64_t v) noexcept {for(int i=0;i<8;++i)p[i]=static_cast<std::uint8_t>(v>>(56-i*8));}
inline std::uint16_t get16be(const std::uint8_t*p) noexcept {return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0])<<8)|p[1]);}
inline std::uint32_t get32be(const std::uint8_t*p) noexcept {return (static_cast<std::uint32_t>(p[0])<<24)|(static_cast<std::uint32_t>(p[1])<<16)|(static_cast<std::uint32_t>(p[2])<<8)|p[3];}
inline std::uint64_t get64be(const std::uint8_t*p) noexcept {std::uint64_t v=0;for(int i=0;i<8;++i)v=(v<<8)|p[i];return v;}
inline void put32le(std::uint8_t*p,std::uint32_t v) noexcept {p[0]=static_cast<std::uint8_t>(v);p[1]=static_cast<std::uint8_t>(v>>8);p[2]=static_cast<std::uint8_t>(v>>16);p[3]=static_cast<std::uint8_t>(v>>24);}
inline std::uint32_t get32le(const std::uint8_t*p) noexcept {return static_cast<std::uint32_t>(p[0])|(static_cast<std::uint32_t>(p[1])<<8)|(static_cast<std::uint32_t>(p[2])<<16)|(static_cast<std::uint32_t>(p[3])<<24);}
inline bool validMetadata(const PacketMetadata&m) noexcept {if(m.streamId>=16||m.sampleRate<8000||m.sampleRate>192000)return false;for(auto b:m.sessionId)if(b!=0)return true;return false;}
}
inline PacketResult encodeAudioPacket(const PacketMetadata&m,const float*l,const float*r,std::uint8_t*d,std::size_t cap,std::size_t&written) noexcept {
 written=0;if(!l||!r||!d)return PacketResult::invalidArgument;if(!detail::validMetadata(m))return PacketResult::invalidMetadata;
 if(m.frames==0||m.frames>kMaxFramesPerPacket)return PacketResult::invalidFrameCount;
 const auto required=kHeaderBytes+static_cast<std::size_t>(m.frames)*8;if(required>kMaxDatagramBytes)return PacketResult::invalidFrameCount;if(cap<required)return PacketResult::insufficientCapacity;
 // Validate the complete source block before touching the destination packet.
 for(std::size_t i=0;i<m.frames;++i)if(!std::isfinite(l[i])||!std::isfinite(r[i]))return PacketResult::nonFiniteSample;
 d[0]='D';d[1]='B';d[2]='S';d[3]='A';d[4]=kProtocolVersion;d[5]=0;detail::put16be(d+6,static_cast<std::uint16_t>(kHeaderBytes));
 std::memcpy(d+8,m.sessionId.data(),m.sessionId.size());d[24]=m.streamId;d[25]=kCodecFloat32;d[26]=2;d[27]=0;
 detail::put32be(d+28,m.sequence);detail::put64be(d+32,m.timestampFrames);detail::put32be(d+40,m.sampleRate);detail::put16be(d+44,m.frames);detail::put16be(d+46,0);
 for(std::size_t i=0;i<m.frames;++i){std::uint32_t lb=0,rb=0;std::memcpy(&lb,l+i,4);std::memcpy(&rb,r+i,4);detail::put32le(d+kHeaderBytes+i*8,lb);detail::put32le(d+kHeaderBytes+i*8+4,rb);}
 written=required;return PacketResult::success;
}
inline PacketResult decodeAudioPacket(const std::uint8_t*p,std::size_t n,PacketMetadata&m,float*l,float*r,std::size_t cap) noexcept {
 if(!p||!l||!r)return PacketResult::invalidArgument;if(n<kHeaderBytes||n>kMaxDatagramBytes)return PacketResult::invalidPacket;
 if(p[0]!='D'||p[1]!='B'||p[2]!='S'||p[3]!='A'||p[4]!=kProtocolVersion||p[5]!=0||detail::get16be(p+6)!=kHeaderBytes||p[25]!=kCodecFloat32||p[26]!=2||p[27]!=0||detail::get16be(p+46)!=0)return PacketResult::invalidPacket;
 PacketMetadata x{};std::memcpy(x.sessionId.data(),p+8,x.sessionId.size());x.streamId=p[24];x.sequence=detail::get32be(p+28);x.timestampFrames=detail::get64be(p+32);x.sampleRate=detail::get32be(p+40);x.frames=detail::get16be(p+44);
 if(!detail::validMetadata(x)||x.frames==0||x.frames>kMaxFramesPerPacket)return PacketResult::invalidMetadata;
 const auto expected=kHeaderBytes+static_cast<std::size_t>(x.frames)*8;if(n!=expected)return PacketResult::invalidPacket;if(cap<x.frames)return PacketResult::insufficientCapacity;
 // Reject non-finite payloads before modifying caller-owned output buffers or metadata.
 for(std::size_t i=0;i<x.frames;++i){const auto*q=p+kHeaderBytes+i*8;std::uint32_t lb=detail::get32le(q),rb=detail::get32le(q+4);float left=0.0f,right=0.0f;std::memcpy(&left,&lb,4);std::memcpy(&right,&rb,4);if(!std::isfinite(left)||!std::isfinite(right))return PacketResult::nonFiniteSample;}
 for(std::size_t i=0;i<x.frames;++i){const auto*q=p+kHeaderBytes+i*8;const auto lb=detail::get32le(q),rb=detail::get32le(q+4);std::memcpy(l+i,&lb,4);std::memcpy(r+i,&rb,4);}
 m=x;return PacketResult::success;
}

// Single sender-thread packetizer for the 16 stereo AUX streams. The caller
// supplies the channel-major queue block and its absolute first-frame clock.
// Sequence counters advance only after a packet has been encoded successfully.
class AudioPacketStreamEncoder {
public:
    explicit AudioPacketStreamEncoder(const std::array<std::uint8_t, 16>& sessionId,
                                      std::uint32_t sampleRate = 48000) noexcept
        : sessionId_(sessionId), sampleRate_(sampleRate) {}

    PacketResult encodeBlockPacket(const float* channelMajorSamples,
                                   std::size_t channelCount,
                                   std::size_t channelStride,
                                   std::size_t totalFrames,
                                   std::size_t frameOffset,
                                   std::uint8_t streamId,
                                   std::uint64_t blockTimestampFrames,
                                   std::uint8_t* destination,
                                   std::size_t capacity,
                                   std::size_t& written) noexcept {
        written = 0;
        if (channelMajorSamples == nullptr || destination == nullptr)
            return PacketResult::invalidArgument;
        if (streamId >= 16 || sampleRate_ < 8000 || sampleRate_ > 192000)
            return PacketResult::invalidMetadata;
        if (totalFrames == 0 || totalFrames > channelStride || frameOffset >= totalFrames)
            return PacketResult::invalidFrameCount;
        const auto leftChannel = static_cast<std::size_t>(streamId) * 2;
        if (channelCount < leftChannel + 2)
            return PacketResult::invalidArgument;

        const auto packetFrames = (totalFrames - frameOffset < kMaxFramesPerPacket)
            ? totalFrames - frameOffset : kMaxFramesPerPacket;
        if (blockTimestampFrames > std::numeric_limits<std::uint64_t>::max() - frameOffset)
            return PacketResult::invalidMetadata;

        PacketMetadata metadata{};
        metadata.sessionId = sessionId_;
        metadata.streamId = streamId;
        metadata.sequence = nextSequence_[streamId];
        metadata.timestampFrames = blockTimestampFrames + frameOffset;
        metadata.sampleRate = sampleRate_;
        metadata.frames = static_cast<std::uint16_t>(packetFrames);

        const auto* left = channelMajorSamples + leftChannel * channelStride + frameOffset;
        const auto* right = channelMajorSamples + (leftChannel + 1) * channelStride + frameOffset;
        const auto result = encodeAudioPacket(metadata, left, right, destination, capacity, written);
        if (result == PacketResult::success)
            ++nextSequence_[streamId];
        return result;
    }

    std::uint32_t nextSequence(std::uint8_t streamId) const noexcept {
        return streamId < 16 ? nextSequence_[streamId] : 0;
    }

private:
    std::array<std::uint8_t, 16> sessionId_{};
    std::uint32_t sampleRate_ = 48000;
    std::array<std::uint32_t, 16> nextSequence_{};
};

}

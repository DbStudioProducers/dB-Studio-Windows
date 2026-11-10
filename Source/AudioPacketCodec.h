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
 d[0]='D';d[1]='B';d[2]='S';d[3]='A';d[4]=kProtocolVersion;d[5]=0;detail::put16be(d+6,static_cast<std::uint16_t>(kHeaderBytes));
 std::memcpy(d+8,m.sessionId.data(),m.sessionId.size());d[24]=m.streamId;d[25]=kCodecFloat32;d[26]=2;d[27]=0;
 detail::put32be(d+28,m.sequence);detail::put64be(d+32,m.timestampFrames);detail::put32be(d+40,m.sampleRate);detail::put16be(d+44,m.frames);detail::put16be(d+46,0);
 for(std::size_t i=0;i<m.frames;++i){if(!std::isfinite(l[i])||!std::isfinite(r[i]))return PacketResult::nonFiniteSample;std::uint32_t lb=0,rb=0;std::memcpy(&lb,l+i,4);std::memcpy(&rb,r+i,4);detail::put32le(d+kHeaderBytes+i*8,lb);detail::put32le(d+kHeaderBytes+i*8+4,rb);}
 written=required;return PacketResult::success;
}
inline PacketResult decodeAudioPacket(const std::uint8_t*p,std::size_t n,PacketMetadata&m,float*l,float*r,std::size_t cap) noexcept {
 if(!p||!l||!r)return PacketResult::invalidArgument;if(n<kHeaderBytes||n>kMaxDatagramBytes)return PacketResult::invalidPacket;
 if(p[0]!='D'||p[1]!='B'||p[2]!='S'||p[3]!='A'||p[4]!=kProtocolVersion||p[5]!=0||detail::get16be(p+6)!=kHeaderBytes||p[25]!=kCodecFloat32||p[26]!=2||p[27]!=0||detail::get16be(p+46)!=0)return PacketResult::invalidPacket;
 PacketMetadata x{};std::memcpy(x.sessionId.data(),p+8,x.sessionId.size());x.streamId=p[24];x.sequence=detail::get32be(p+28);x.timestampFrames=detail::get64be(p+32);x.sampleRate=detail::get32be(p+40);x.frames=detail::get16be(p+44);
 if(!detail::validMetadata(x)||x.frames==0||x.frames>kMaxFramesPerPacket)return PacketResult::invalidMetadata;
 const auto expected=kHeaderBytes+static_cast<std::size_t>(x.frames)*8;if(n!=expected)return PacketResult::invalidPacket;if(cap<x.frames)return PacketResult::insufficientCapacity;
 for(std::size_t i=0;i<x.frames;++i){const auto*q=p+kHeaderBytes+i*8;auto lb=detail::get32le(q),rb=detail::get32le(q+4);std::memcpy(l+i,&lb,4);std::memcpy(r+i,&rb,4);if(!std::isfinite(l[i])||!std::isfinite(r[i]))return PacketResult::nonFiniteSample;}
 m=x;return PacketResult::success;
}
}

#pragma once
// Application-level fail-closed gate for a future QUIC adapter.
// This class performs NO TLS, certificate validation, authentication,
// authorization, networking, or cryptography.
namespace DbStudioSecurity {
enum class TransportPhase { disconnected, handshaking, peerVerified, deviceAuthenticated, authorized, closed };
enum class DataClass { audioDatagram, reliableControl, telemetry, talkback };
class TransportSecurityGate final {
public:
    constexpr TransportPhase phase() const noexcept { return phase_; }
    constexpr bool beginHandshake() noexcept {
        if (phase_ != TransportPhase::disconnected) return false;
        phase_ = TransportPhase::handshaking; return true;
    }
    // Invoke only after TLS handshake completion and peer identity verification.
    constexpr bool markPeerVerified(bool verified) noexcept {
        if (phase_ != TransportPhase::handshaking) return false;
        if (!verified) { phase_ = TransportPhase::closed; return false; }
        phase_ = TransportPhase::peerVerified; return true;
    }
    // Invoke only after server-side individual device authentication.
    constexpr bool markDeviceAuthenticated(bool authenticated) noexcept {
        if (phase_ != TransportPhase::peerVerified) return false;
        if (!authenticated) { phase_ = TransportPhase::closed; return false; }
        phase_ = TransportPhase::deviceAuthenticated; return true;
    }
    // Invoke only after server-side role, device, session and AUX authorization.
    constexpr bool markAuthorized(bool authorized) noexcept {
        if (phase_ != TransportPhase::deviceAuthenticated) return false;
        if (!authorized) { phase_ = TransportPhase::closed; return false; }
        phase_ = TransportPhase::authorized; return true;
    }
    // earlyData must come from trusted transport state, never packet content.
    // Every application data class is denied in 0-RTT.
    constexpr bool maySend(DataClass, bool earlyData) const noexcept {
        return phase_ == TransportPhase::authorized && !earlyData;
    }
    constexpr bool mayProcessCommand(bool earlyData) const noexcept {
        return phase_ == TransportPhase::authorized && !earlyData;
    }
    constexpr void close() noexcept { phase_ = TransportPhase::closed; }
private:
    TransportPhase phase_ = TransportPhase::disconnected;
};
} // namespace DbStudioSecurity

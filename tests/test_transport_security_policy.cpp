#include "../Source/TransportSecurityPolicy.h"
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL: " << #x << " line " << __LINE__ << '\n'; return 1; } } while (false)
int main() {
    using namespace DbStudioSecurity;
    TransportSecurityGate gate;
    CHECK(gate.phase() == TransportPhase::disconnected);
    CHECK(!gate.maySend(DataClass::audioDatagram, false));
    CHECK(!gate.mayProcessCommand(false));
    CHECK(gate.beginHandshake());
    CHECK(gate.phase() == TransportPhase::handshaking);
    CHECK(!gate.markDeviceAuthenticated(true));
    CHECK(!gate.markAuthorized(true));
    CHECK(!gate.maySend(DataClass::audioDatagram, false));
    TransportSecurityGate invalidTransport;
    CHECK(invalidPeer.beginHandshake());
    CHECK(!invalidPeer.markSecureTransportEstablished(false));
    CHECK(invalidPeer.phase() == TransportPhase::closed);
    CHECK(!invalidPeer.beginHandshake());
    TransportSecurityGate session;
    CHECK(session.beginHandshake());
    CHECK(session.markSecureTransportEstablished(true));
    CHECK(!session.maySend(DataClass::audioDatagram, false));
    CHECK(!session.mayProcessCommand(false));
    CHECK(session.markDeviceAuthenticated(true));
    CHECK(!session.maySend(DataClass::audioDatagram, false));
    CHECK(!session.mayProcessCommand(false));
    TransportSecurityGate denied;
    CHECK(denied.beginHandshake());
    CHECK(denied.markSecureTransportEstablished(true));
    CHECK(denied.markDeviceAuthenticated(true));
    CHECK(!denied.markAuthorized(false));
    CHECK(denied.phase() == TransportPhase::closed);
    CHECK(session.markAuthorized(true));
    CHECK(session.maySend(DataClass::audioDatagram, false));
    CHECK(session.maySend(DataClass::reliableControl, false));
    CHECK(session.maySend(DataClass::telemetry, false));
    CHECK(session.maySend(DataClass::talkback, false));
    CHECK(session.mayProcessCommand(false));
    CHECK(!session.maySend(DataClass::audioDatagram, true));
    CHECK(!session.maySend(DataClass::reliableControl, true));
    CHECK(!session.maySend(DataClass::telemetry, true));
    CHECK(!session.maySend(DataClass::talkback, true));
    CHECK(!session.mayProcessCommand(true));
    session.close();
    CHECK(session.phase() == TransportPhase::closed);
    CHECK(!session.maySend(DataClass::audioDatagram, false));
    CHECK(!session.mayProcessCommand(false));
    CHECK(!session.beginHandshake());
    return 0;
}

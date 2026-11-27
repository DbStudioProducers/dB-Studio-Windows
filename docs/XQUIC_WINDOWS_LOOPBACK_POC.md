# XQUIC Windows loopback proof of concept

**Updated:** 2026-11-23  
**Scope:** PR #4, branch feature/wifi-quic-poc-20261115

## Pinned dependencies

- XQUIC v1.9.8 source: b2d1f2ca05d3456614838c82bf50cafe5c060262
- BoringSSL source: 4f3ef1e422b7cc6b880c5712feb35126ce294ce0
- Windows runner: Windows Server 2022, x64

## Security contract checked before compilation

The pinned XQUIC BoringSSL CMake path defines NOCRYPT=1. In this exact XQUIC revision, that macro is not referenced from the src or include source trees. The active client encryption choice is the runtime no_crypto_flag passed to xqc_connect; the client connection code sets the no-crypto transport option only when that value is 1 and sets it false otherwise. The loopback test patch passes literal 0 to xqc_connect.

The pinned TLS context code sets both the minimum and maximum protocol version to TLS1_3_VERSION for the client and server contexts. The test client selects QUIC v1, forces 1-RTT, enables certificate verification, and checks the exact DER encoding of the peer leaf certificate against a locally generated pin. The positive test uses a self-signed certificate only because the test client explicitly allows the self-signed verification path and then enforces the leaf pin. A second server certificate has the same hostname but a different key and must be rejected by that pin check.

The static source-contract script fails closed if these assumptions change in the pinned source.

## Runtime loopback gates

The Windows job builds the XQUIC static library and its upstream native transport test client/server, then runs three localhost tests:

1. Encrypted QUIC v1 / TLS 1.3 handshake plus reliable bidirectional stream echo.
2. Encrypted QUIC v1 / TLS 1.3 handshake plus bidirectional QUIC DATAGRAM echo.
3. Negative certificate test: a different self-signed leaf certificate with the same hostname must fail the configured certificate pin and must not complete the handshake.

The client uses 1-RTT only; no session ticket is loaded for 0-RTT. The loopback script stores stdout/stderr and XQUIC logs as a workflow artifact.

## What this does not prove

These tests are Windows localhost tests using XQUIC's native test applications. They do not prove Android interoperability, Wi-Fi discovery or traversal, production identity provisioning, individual musician login, server-side AUX authorization, JUCE audio-engine integration, real-time audio latency, or physical field operation. The dB Studio audio packet codec and authorization policy tests remain separate from this transport proof.

## Next gates

1. Integrate XQUIC into the JUCE 8 Windows transport adapter without bypassing TLS or certificate validation.
2. Build the same pinned XQUIC transport for Android NDK and establish a real Windows-to-Android handshake.
3. Add application-level device authentication and enforce musician-to-AUX authorization in the server request path.
4. Connect the existing stereo packet format to QUIC DATAGRAM and reliable control streams, then test sequence handling, loss, jitter and reconnection.
5. Only after virtual cross-platform tests pass, prepare physical Wi-Fi field tests.

# dB Studio Android Receiver

Status: **local audio-output test build**. This APK now has working controls to generate a low-level stereo reference tone through the Android audio output and enumerate recognized output devices.

## What can be tested on a phone

- Install and launch on Android 8.0 (API 26) or newer.
- Start and stop a local stereo sine-wave test tone.
- Adjust reference frequency (220–1265 Hz) and limited test level (up to 6%).
- Inspect output devices recognized by Android (wired/P2, USB, built-in speaker and Bluetooth where exposed by the OS).

Start with the phone volume low and preferably use headphones. Stop the tone before disconnecting headphones. This is only a local output-path check, not a calibrated level or latency test.

## What is still not implemented

This build does **not** receive audio from the desktop or mixer. It has no QUIC v1/TLS 1.3 transport, pairing/authentication, server-side AUX authorization, QUIC DATAGRAM receive path, jitter buffer, remote stream decoder, or actual IEM mix. Server and security status remain unavailable by design; no network metrics are fabricated.

A successful tone test validates only local Android playback. It does not prove the dB Studio monitoring system works.

## Install

1. Download the CI artifact `dbstudio-receiver-bench-debug`.
2. Extract `app-debug.apk` from the ZIP and install it on the phone.
3. Open **dB Studio Receiver Bench**.
4. Lower the phone volume, connect headphones, and tap **Iniciar teste estéreo**.
5. Adjust frequency/level, confirm sound in both ears, tap **Parar teste estéreo**, and inspect the listed output devices.

## Field report

Please report phone model, Android version, whether the tone starts/stops, whether both channels are audible, any distortion/dropouts, and which P2/USB output is listed. Do not report Wi-Fi range, transport latency, packet loss, AUX isolation, or audio quality of a remote mix from this build.

## Next implementation gate

Implement authenticated QUIC v1/TLS 1.3, server-authorized AUX binding, QUIC DATAGRAM receive, bounded jitter buffering and stereo remote playback. Then add automated loopback/network tests before field validation on a dedicated Wi-Fi access point.

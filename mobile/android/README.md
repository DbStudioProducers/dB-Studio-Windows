# dB Studio Android Receiver Bench

Status: **instrumentation shell only (0.1.0)**. This APK is for evaluating the Android screen and installation on a physical phone. It is not an audio receiver and must not be used as an in-ear monitor.

## What this build can validate

- APK installation and launch on Android 8.0 (API 26) or newer.
- Basic rendering of the dark receiver-bench screen.
- Physical device compatibility with the app shell.

## What this build cannot validate

The current `MainActivity` intentionally contains no network client, QUIC/TLS handshake, device pairing, AUX authorization, audio decoder, jitter buffer, or audio output pipeline. The displayed unavailable values are not telemetry. No audio is transmitted or played.

Do not interpret a visible screen, successful installation, or successful CI build as proof of receiver functionality.

## Install

1. Download the CI artifact named `dbstudio-receiver-bench-debug`.
2. Extract `app-debug.apk` from the ZIP.
3. Transfer the APK to the Android phone.
4. Allow installation from the file manager/source when Android requests it.
5. Install and open **dB Studio Receiver Bench**.

Alternatively, with Android Platform Tools installed:

```sh
adb install -r app-debug.apk
adb shell monkey -p com.dbstudio.receiverbench 1
```

## Field evaluation checklist

Record phone model, Android version, install result, launch result, screen clipping/readability, orientation behavior, and any crash or ANR. Do not report Wi-Fi range, latency, packet loss, AUX isolation, or audio quality from this build: those functions are not implemented.

## Next engineering gate

Before calling this a functional receiver, implement and test the authenticated QUIC v1/TLS 1.3 client, server-authorized AUX binding, QUIC DATAGRAM receive path, bounded preallocated jitter buffer, and stereo playback. Then issue a new APK and run loopback tests before field testing on a dedicated Wi-Fi access point.

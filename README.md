# dB Studio — Wi-Fi IEM transmitter (native JUCE foundation 0.5.0)

The dB Studio is being developed as a desktop Wi-Fi audio transmitter and personal in-ear monitoring system, inspired by the workflow of products such as StageWave, with its own routing, control, monitoring and administration features. JUCE 8.0.6 is the selected native audio framework.

## Intended system architecture

- Digital mixer / audio interface sends up to 32 input channels to the desktop server through a supported USB audio driver or a separately implemented Ethernet audio protocol.
- The server creates 16 independent stereo auxiliary mixes from those inputs.
- Each auxiliary mix is assigned to one individual musician/device and transmitted over a dedicated Wi-Fi network to that receiver only. Shared-mix delivery to multiple devices is out of scope.
- Each musician's phone receives its assigned stereo mix and outputs it through a wired headphone path, initially USB-C or compatible audio output.
- The desktop remains the central administration and mixing server. An authenticated master device may later receive delegated control permissions.
- Distribution is strictly individual: one device receives one individually assigned AUX mix. A device cannot subscribe to another musician's mix, and an AUX mix is not shared between devices. With 16 AUX buses, the initial design supports up to 16 simultaneously assigned individual mixes/devices.

USB and Ethernet are alternative input paths in the initial design, not automatically summed. Ethernet audio standards such as Dante, AES67 or AVB are not implied by generic Ethernet connectivity and require separate protocol implementation, licensing review and validation.

## Build (Windows 10/11 x64)

1. Install Visual Studio 2022 with the Desktop development with C++ workload.
2. Install CMake 3.22 or newer.
3. Run:
   ```powershell
   cmake -S . -B build -A x64
   cmake --build build --config Release --parallel 2
   ```
CMake downloads the pinned JUCE 8.0.6 source during configuration.

## Current native foundation

- Native JUCE desktop window and audio device selector.
- Device name, active input/output channel counts, sample rate and buffer diagnostics.
- Float32 audio callback and 32 input peak meters.
- 16 independent mix control states, each with 32 channel gain/pan/mute controls.
- All 16 stereo AUX buses are rendered into preallocated internal buffers, independent of physical output availability.
- A bounded three-slot single-producer/single-consumer handoff queue publishes complete AUX snapshots to a future network sender. The audio callback never waits for the consumer; when the queue is full, the newest block is dropped and counted.

## Implementation status and important boundary

This branch is an engineering foundation, not a finished Wi-Fi IEM system. **Network audio streaming and mobile receiver clients are not implemented yet.** The audio engine now renders 16 internal stereo mix buses and publishes snapshots through a bounded, preallocated single-producer/single-consumer queue. A future network sender must be the sole consumer, acquire and release each block exactly once, and perform all network I/O outside the real-time callback. The queue has three slots; if the sender falls behind, the audio callback drops the new block rather than blocking, and increments a diagnostic counter.

The network layer will need packet sequencing, timestamps, jitter buffering on receivers, loss/reorder handling, reconnect behavior, exclusive one-to-one device/AUX assignment, authentication and encryption. Shared AUX subscriptions are intentionally excluded. Audio format, packet duration, codec/PCM choice and target latency must be benchmarked on the dedicated Wi-Fi network before being fixed. The initial preference is to evaluate low-overhead PCM transport first, then compare bandwidth and latency against an optional codec.

The initial mobile audio output is wired/USB-C. Bluetooth headphone support is deferred. Battery level and Wi-Fi signal strength can only be displayed when the mobile operating system or receiver app exposes reliable telemetry.

Physical outputs are currently cleared and are not used as AUX destinations. A two-channel physical output device is optional and reserved for future local monitoring; all 16 mixes are prepared internally for Wi-Fi distribution.

## Compatibility

The selected framework remains JUCE 8.0.6. The initial native build targets Windows 10/11 x64. Windows 8.1 compatibility is not established. WASAPI/DirectSound/ASIO availability depends on JUCE build configuration, installed drivers and device capabilities. ASIO support and SDK licensing must be evaluated separately before distribution.

No claim of universal compatibility is made. USB multichannel devices expose only channels provided by their drivers. Dante, AES67, AVB and vendor-specific Ethernet protocols require separate implementation, licensing review and physical validation.

## Validation

GitHub Actions builds the native application and separately runs the legacy Python syntax, unit and virtual-lab checks. A successful compile validates compilation only; it does not validate Wi-Fi audio latency, packet loss, mobile playback or physical audio hardware.

## Next implementation milestones

1. Extract a dedicated 16-bus stereo mix renderer independent of physical output count, enforcing one receiver per AUX.
2. Add a preallocated bounded audio-block queue and separate Wi-Fi/network sender.
3. Define and test a versioned audio packet format, stream identifiers, sequence numbers and timestamps.
4. Implement receiver jitter buffer, packet-loss concealment policy and reconnection.
5. Add device registration, naming, exclusive one-device-to-one-AUX assignment and master-role authentication.
6. Add a persistent connection history with device name/identity, connect and disconnect timestamps, total session duration and reconnect events.
7. Implement portable scene save/load using the native .dBs format on desktop and mobile, with versioned schema, validation, integrity checks and safe migrations.
8. Add mobile receivers and validate end-to-end latency on a dedicated Wi-Fi access point.

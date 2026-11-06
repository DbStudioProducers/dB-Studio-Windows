# dB Studio — native JUCE foundation 0.5.0

dB Studio is being developed as a desktop Wi-Fi audio transmitter and personal in-ear monitoring system. JUCE 8.0.6 is the selected native audio framework.

## Intended system architecture

- A digital mixer/audio interface supplies up to 32 inputs through a supported USB audio driver or a separately implemented Ethernet audio protocol.
- The server creates 16 independent stereo AUX mixes.
- Each AUX is assigned to one individual musician/device and will be transmitted over dedicated Wi-Fi to that receiver only. Shared-mix delivery is out of scope.
- The initial mobile output supports one active wired route at a time: P2 (3.5 mm) or USB-C audio, selected from Android's reported routing. Simultaneous dual output is deferred.
- The desktop remains the central administration and mixing server. An authenticated master device may later receive delegated control permissions.
- One device receives one exclusive AUX. With 16 AUX buses, the initial design supports up to 16 simultaneously assigned devices.

USB and Ethernet are alternative input paths, not automatically summed. Generic Ethernet connectivity does not imply Dante, AES67, AVB or another audio protocol; each requires separate implementation, licensing review and validation.

## Repository layout

- `Source/`: native JUCE application, audio engine, renderer and AUX assignment registry.
- `tests/`: native C++ tests and Python architecture/regression tests.
- `db_studio/`: Python virtual-lab modules only (simulated source, mixer and packet codec).
- `run_virtual_lab.py`: local UDP loopback simulation for 16 independent stereo streams.
- `docs/`: product specifications for features not yet implemented.

The previous Tkinter desktop prototype (`run_desktop.py` and `db_studio/ui_screens.py`) was removed. It duplicated the product UI outside the native JUCE application and contained demonstration-only controls/device data. The Python virtual lab remains because CI uses it to exercise the simulated mixer and packet codec; it is not a production desktop app or network sender.

## Build and verify (Windows 10/11 x64)

Requirements: Visual Studio 2022 with Desktop development with C++, CMake 3.22+, Python 3.10+ and pip.

Run from the repository root in PowerShell:

```powershell
python -m pip install -r requirements.txt
python -m unittest tests/test_native_architecture.py -v
python -m py_compile run_virtual_lab.py db_studio/mixer.py db_studio/protocol.py db_studio/simulator.py
python -m unittest discover -s tests -v
python run_virtual_lab.py --seconds 1.5 --rate 48000 --blocksize 128

cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target dBStudioCoreTests dBStudioMixTests dBStudioQueueTests dBStudioAuthorizationTests
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target dBStudio --parallel 2
```

The verification sequence is enforced in GitHub Actions: the architecture, Python regression and UDP loopback virtual-lab job must pass before the native job starts. The native job then configures CMake, builds all four C++ test executables, runs CTest, and only after those checks pass compiles the application and uploads its artifact. CMake fetches the pinned JUCE 8.0.6 source.

## Implemented native foundation

- Native JUCE desktop window and audio-device selector.
- Device name, active input/output channel counts, sample rate and buffer diagnostics.
- Float32 audio callback and 32 input peak meters.
- 16 independent mix states, each with 32 channel gain/pan/MUTE/SOLO controls.
- All 16 stereo AUX buses rendered into preallocated internal buffers, independent of physical output availability.
- Bounded three-slot single-producer/single-consumer handoff queue for future network delivery. The audio callback does not wait; when full, the newest block is dropped and counted.
- Thread-safe native AUX registry: stable device identity, unique device-to-AUX assignment, rename, lookup, unassignment and versioned local persistence.

The registry is a tested core component, not yet connected to receiver discovery or a live Devices screen. Assignment dialogs currently accept manually entered device IDs.

## Implementation boundaries

This branch is an engineering foundation, not a finished Wi-Fi IEM system. **Network audio streaming and mobile receiver clients are not implemented.** A future network sender must be the sole queue consumer, acquire/release each block exactly once and perform all network I/O outside the real-time callback. The Python virtual lab has a versioned PCM packet draft with sequence and frame metadata, verifies delivery of every expected loopback UDP packet, and rejects stereo blocks that exceed the IPv4 UDP datagram payload limit. It is not the production network protocol. Production timestamps, transport, receiver jitter buffering, loss/reorder handling, reconnect, authentication and encryption remain to be implemented and tested.

Bluetooth headphone support is deferred. Android P2/USB-C automatic output routing is specified in `docs/MOBILE_PROTOCOL_AND_ROLES.md`; it cannot be marked implemented until the Android receiver application exists and is tested on supported devices. Battery and Wi-Fi signal can only be shown when a mobile OS or receiver app exposes reliable telemetry. Physical outputs are currently cleared and are not AUX destinations; the internal buses are prepared for future Wi-Fi distribution.

## Compatibility

The initial native target is Windows 10/11 x64. Windows 8.1 compatibility is not established. JUCE 8.0.6 remains pinned. WASAPI/DirectSound/ASIO availability depends on build configuration, installed drivers and hardware. ASIO SDK licensing must be evaluated before distribution. Dante, AES67, AVB and vendor-specific Ethernet protocols require separate integration and validation.

A successful CI build validates compilation, not end-to-end Wi-Fi latency, packet loss, mobile playback or physical hardware.

## Future milestones (not yet implemented)

1. Add remaining engine-level regression tests for device changes, meter timing, control-state persistence and real-time callback behavior.
2. Promote the virtual-lab PCM packet draft into a production wire protocol with stream IDs, timestamps, sequence numbers and explicit compatibility rules.
3. Implement the dedicated Wi-Fi sender and mobile receiver, including jitter buffering, loss/reorder handling and reconnect.
4. Connect receiver discovery/authentication to exclusive device/AUX assignment and delegated master permissions.
5. Add persistent connection history with stable identity, session timestamps, duration and crash recovery.
6. Implement portable `.dBs` scene save/load on desktop and mobile with versioned schema, validation, integrity checks and migrations.
7. Add desktop multitrack recording for up to 32 independent inputs, aligned tracks, recovery, Save As and batch export after encoder/licensing validation.
8. Validate end-to-end latency, packet loss and wired mobile playback on a dedicated Wi-Fi access point.

Detailed requirements for connection history, `.dBs` scenes and multitrack recording remain in `docs/DEVICE_HISTORY_AND_DBS_FORMAT.md`.

# dB Studio — Native JUCE foundation (0.5.0)

This branch introduces the first native desktop foundation using JUCE 8.0.6 and CMake.

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
- Native JUCE desktop window.
- JUCE AudioDeviceManager initialization and device selector.
- Device name, active input/output channel counts, sample rate and buffer diagnostics.
- Audio callback using float32 buffers.
- 32 input peak meters and 16 stereo bus peak values.
- Stereo mix 1 can use a normal two-channel output. Mixes 2–16 require a multichannel output device.

## Important implementation status
This is an engineering foundation, not a finished monitor mixer. The current callback distributes a simple test mix to available stereo output pairs; per-channel gain/pan/mute/solo, independent musician routing, DSP effects, network streaming, authentication, mobile clients and hardware-specific control protocols are not implemented yet.

The legacy Python virtual lab remains in the repository for regression comparison. Its PASS result validates only the simulator and loopback packet path, not physical interfaces.

## Compatibility
The initial native build targets Windows 10/11 x64. Current JUCE 8 support does not establish Windows 8.1 compatibility. WASAPI/DirectSound/ASIO availability depends on the JUCE build configuration, installed drivers and device capabilities. ASIO support and SDK licensing must be evaluated separately before distribution.

No claim of universal compatibility is made. USB multichannel devices expose only channels provided by their drivers. Dante, AES67, AVB and vendor-specific Ethernet protocols require separate implementation, licensing review and physical validation.

## Validation
GitHub Actions builds the native application and separately runs the legacy Python syntax, unit and virtual-lab checks. A successful compile is not a substitute for testing real audio hardware and measuring round-trip latency.

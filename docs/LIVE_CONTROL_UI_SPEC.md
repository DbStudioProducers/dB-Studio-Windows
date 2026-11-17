# dB Studio — Live Control UI Specification

Status: design specification; not a claim of completed UI implementation
Target: desktop and mobile UI implementation planning
Visual direction: Live Control, balanced density, stage-readable dark interface

## 1. Shared visual system

- Base background: near-black charcoal (#101215).
- Primary surface: raised charcoal (#191d21); secondary surface: #252b30.
- Primary action/accent: vivid green (#43e638).
- Meter zones: green for normal level, yellow for elevated level, red for peak/overload.
- Text: high-contrast white for primary values; muted gray for supporting metadata.
- Controls must remain legible in low-light stage environments, with clear pressed/selected states and generous touch targets on mobile.
- Color must not be the sole status indicator: pair it with text/icon/state.
- Avoid decorative motion. Meter animation may be smooth but must not obscure clipping/peak indication.

## 2. Desktop Server — Live Control

### Primary window structure

1. **Top bar**
   - dB Studio wordmark and application version.
   - Audio engine state: active, unavailable, or initializing.
   - Selected audio device and active input/output counts.
   - Audio settings and update actions.
2. **Session/navigation row**
   - Live Control (default).
   - Devices.
   - Scenes (planned; disabled or marked unavailable until implemented).
   - Recording (planned; disabled or marked unavailable until implemented).
   - Session name and local save state.
3. **Mix selector**
   - 16 clearly numbered stereo AUX mixes.
   - Selected AUX is visually explicit.
   - AUX assignment and rename actions remain available but must be labelled as local assignments until server/device transport exists.
4. **Main channel workspace**
   - 32 input channels, grouped into four banks: 01–08, 09–16, 17–24, 25–32.
   - Each channel surface contains channel number/name, signal meter, gain fader, pan, MUTE and SOLO.
   - Meter scale: green, yellow and red zones; peak/clip state remains latched long enough to be noticed.
   - Responsive columns with vertical scrolling; no channel controls may overlap or become inaccessible.
5. **Selected AUX master**
   - Stereo output meter.
   - AUX master level and mute.
   - EQ entry point (future until implemented).
6. **Device/session side panel**
   - Real connected devices only when transport integration supplies a live session.
   - Each connected device row: display name, assigned AUX, connection state, session duration, and telemetry only when actually supplied by the endpoint.
   - Distinguish “assigned locally” from “connected”.
   - Empty state: “Nenhum receptor conectado” and explain that Wi-Fi receiver integration is not yet available.
7. **Diagnostics/footer**
   - Audio driver, sample rate, buffer size and channel counts.
   - Explicitly state when values are local engine data and when receiver/network data is unavailable.

### Desktop behavior

- Selecting an AUX loads only that AUX's mix state.
- Channel gain, pan, MUTE and SOLO modify the selected local mix engine state.
- Device assignment UI must not imply that a phone has connected or is receiving audio.
- Do not show fake battery, RF strength, latency, connection time or musician presence.
- Talkback, recording and scene controls must remain hidden or visibly marked “em desenvolvimento” until their end-to-end paths exist.

## 3. Musician Mobile — Personal Mix

### Primary screen

- Header: dB Studio, authenticated musician display name, assigned AUX (read-only), and connection/stream state.
- Four bank tabs: 01–08, 09–16, 17–24, 25–32.
- Eight channel strips in the selected bank.
- Each strip: channel name/number, meter, fader, MUTE and SOLO.
- Persistent bottom master area: AUX master level, master mute and EQ access.
- Output indicator: P2, USB-C, or no wired output, based on actual OS routing information.
- Connection duration and battery are shown only if real device telemetry is available.

### Permission and interaction rules

- A musician controls exactly one server-assigned AUX.
- Talkback and PTT controls, destinations, and commands are exclusive to authenticated Master sessions; they must not appear anywhere in a musician profile.
- A musician receives Talkback only as audio mixed/routed by the server into the musician's assigned AUX through its designated Talkback channel while that AUX channel is active. The musician has no Talkback transmit, routing, destination-selection, or enable/disable control.
- The Master alone selects destination AUX mixes and momentarily opens the Talkback route by holding PTT; release, disconnect, session expiry, or safety timeout closes it.
- No AUX selector, no access to another musician's mix, and no Master Talkback PTT.
- Switching channel banks changes visibility only; it must never reset mix values.
- Touch targets and fader travel must be suitable for one-handed stage use.
- On loss of server connection, show a persistent disconnected state and prevent controls from appearing successfully applied until acknowledged by the server.

## 4. Master Mobile — Administration and Talkback

### Devices page

- Connected-device list with display name, assigned AUX, online/offline state, connection duration, and available telemetry.
- Separate section for locally known assignments that are not currently connected.
- Actions: rename display name, assign/reassign AUX, disconnect/revoke session (only after server support exists).
- Enforce one musician per AUX; assignment changes must be atomic and acknowledged.

### Talkback page

- Separate tab visible only to authenticated Master role.
- Destination selector listing AUX 01–16.
- Large momentary PTT control.
- PTT is active only while pressed; release, disconnect, session expiry or safety timeout closes the route.
- Show selected destinations and unmistakable active/inactive state.
- Never implement PTT as a persistent toggle.
- A musician session must never receive this control or be able to invoke the command through a crafted request.

## 5. Responsive and accessibility requirements

- Desktop: preserve access to all 32 channels at common 1366×768 and 1920×1080 sizes; scrolling is acceptable, clipping controls is not.
- Mobile: portrait-first layout; eight channels per bank; no horizontal page scrolling for essential controls.
- Maintain high contrast and readable labels at typical phone viewing distance.
- Provide text labels alongside color states.
- Keyboard focus must be visible on desktop; mobile controls must have sufficiently large hit areas.
- Respect system font scaling where possible without hiding essential controls.

## 6. Implementation status boundary

The current JUCE desktop application already contains a local 32-input/16-mix mixer surface, local AUX assignment registry, audio-device selector, output meter, diagnostics and updater action. Its current connected-device summary explicitly states that Wi-Fi receivers are not integrated.

The Musician Mobile and Master Mobile screens in this document are design specifications only. The repository does not yet contain a production Android/iOS receiver application. The following are not to be represented as implemented until end-to-end code and tests exist: network audio transport, mobile authentication, real connected-device presence, telemetry, Talkback routing, scene synchronization, and remote mix control.

## 7. Implementation sequence

1. Refactor the desktop JUCE layout into clear Live Control regions while preserving the existing audio engine and local mix behavior.
2. Add explicit empty/placeholder states for devices, scenes, recording and Talkback without simulating functionality.
3. Add automated layout/architecture checks for responsive bounds, channel count, bank mapping, and truthful connection states.
4. Create the native mobile application shell and musician/master navigation.
5. Implement authenticated control plane and permission tests before enabling remote controls.
6. Implement bounded audio transport and receiver playback only after protocol, security and real-device validation.
7. Run verification tests before every compile; complete CI build and CTest before distributing artifacts.

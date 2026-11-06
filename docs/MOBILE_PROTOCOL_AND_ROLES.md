# dB Studio — Mobile Roles and Communication Contract

Status: implementation specification draft  
Target: dB Studio 0.6 planning  
Scope: Desktop Server, Musician Mobile, Master Mobile

## 1. Product roles

### Desktop Server
The desktop application is the authoritative session coordinator. It owns the audio engine, the 16 logical stereo AUX buses, device/user assignments, session state, and Talkback routing policy.

### Musician device
A musician account is assigned to exactly one AUX bus at a time. The assignment is enforced by the server, not by a client-side selector. A musician can read and change only the mix controls for that assigned AUX.

Allowed controls for the assigned AUX:
- Input-channel level for all 32 inputs.
- MUTE and SOLO per input.
- AUX master level.
- AUX EQ.
- Read-only connection and stream status.

A musician device must not receive controls for another AUX, change its own AUX assignment, or expose the Master PTT control.

### Master device
The authenticated Master role can administer users/devices, rename display names, assign one AUX to each musician, inspect connection telemetry, and operate Talkback. Master privileges are enforced by the server on every command.

The Master Talkback page is a separate tab. It lists the available AUX destinations and allows one or more destinations to be selected. The PTT control exists only in the Master interface.

## 2. Channel-bank presentation

All 32 input channels remain available in every musician AUX mix. To keep the mobile mixer manageable, present four fixed banks:

| Bank | Input channels | Count |
|---|---:|---:|
| 1 | 01–08 | 8 |
| 2 | 09–16 | 8 |
| 3 | 17–24 | 8 |
| 4 | 25–32 | 8 |

Changing banks changes only the visible channel range. It must not reset or modify any channel setting. The server stores mix parameters independently of the currently visible bank.

Each channel strip exposes its channel name, level meter, fader, MUTE, and SOLO. The mobile layout must make all eight channels in the selected bank accessible without hiding essential controls. AUX master level and EQ remain available independently of the selected bank.

## 3. Talkback behavior

1. Only a session authenticated with the Master role can request Talkback.
2. Master selects one or more AUX destinations on the dedicated Talkback tab.
3. PTT is momentary: Talkback opens while the physical/logical press is held and closes on release.
4. The server validates the Master session and selected destinations before opening the route.
5. Talkback audio is mixed only into the selected AUX destinations. Unselected AUX buses must not receive it.
6. Release, disconnect, session expiry, or server-side safety timeout closes the route.
7. A stale or replayed PTT command must never leave Talkback open.
8. The UI must show the selected destinations and an unmistakable active/inactive state.

The PTT action is not a persistent toggle. A separate continuous-talk mode is not part of this contract.

## 4. Device and user administration

The server maintains a stable device identity, user identity, display name, role, assigned AUX (for musician accounts), connection state, last-seen time, and available telemetry.

Battery percentage and radio signal strength are optional telemetry: display them only when the mobile OS reports meaningful values. Do not fabricate values when unavailable. Connection duration is calculated from the server-observed session start time.

Renaming a user changes the display name, not the stable identity or authentication credentials. Reassignment of a musician AUX is a privileged Master operation and must be atomic: an AUX cannot be assigned to two musician accounts simultaneously.

## 5. Proposed transport boundaries

- Audio plane: bounded, sequenced audio packets over a low-latency transport (UDP is the initial candidate).
- Control plane: authenticated, encrypted, reliable bidirectional messages (WebSocket over TLS is the initial candidate).
- The real-time audio callback must never perform network I/O, wait for a network thread, allocate packet buffers, or acquire a blocking mutex.
- A dedicated sender consumes preallocated audio blocks from a bounded single-producer/single-consumer queue.
- Control messages carry a protocol version, session identifier, message identifier, and server-authoritative identity/permissions.
- Audio packets carry session/stream identifiers, sequence number, sample timestamp, frame count, and payload format metadata.
- Receiver buffering handles packet jitter and reordering; late packets are discarded rather than delaying playback indefinitely.
- Authentication tokens and credentials are never logged or sent in plaintext. Server authorization is checked for every state-changing command.

Transport details, codec, encryption profile, packet size, and jitter-buffer target remain to be validated in implementation tests and on real devices.

## 6. Minimum control message families

The eventual protocol should define versioned schemas for:
- Session authentication and role negotiation.
- Device presence, heartbeat, disconnect, and telemetry.
- User display-name update.
- Master-only AUX assignment and device administration.
- Read/write mix parameters scoped to the authenticated musician's assigned AUX.
- Master-only Talkback destination selection and PTT press/release.
- Server acknowledgements and structured errors.

A client-provided role, user ID, or AUX ID is never trusted as proof of authorization. The server derives permissions from the authenticated session.

## 7. Acceptance criteria before enabling mobile audio

- A musician account cannot read or change another AUX, including by manually crafting a request.
- Two musician accounts cannot own the same AUX simultaneously.
- Master-only operations are rejected for musician sessions.
- Talkback is routed only to selected destinations and always closes on release/disconnect/timeout.
- All four channel banks show exactly their specified eight channels and preserve settings when switching.
- Network loss/reconnect does not change saved mix values or accidentally reopen Talkback.
- Audio callback remains bounded, non-blocking, and free of network operations.
- Signal/battery telemetry is shown only when supplied by the device.


## 8. Mobile audio output routing (P2 and USB-C)

Initial mobile release supports one active wired output route at a time. The app must not require the user to choose P2 or USB-C manually when Android exposes the route to the system.

### Route selection policy

- Use Android's active audio output routing as the source of truth. Do not infer connector type from the phone model or assume every handset has a 3.5 mm jack.
- A wired analog headset/headphone route (typically reported as `TYPE_WIRED_HEADSET` or `TYPE_WIRED_HEADPHONES`) is eligible for the P2 route.
- A USB audio route (typically `TYPE_USB_HEADSET` or `TYPE_USB_DEVICE`) is eligible for USB-C audio, including compatible USB-C-to-3.5 mm DAC adapters.
- When Android changes the active output, the receiver follows the system-selected route automatically. Do not force a communication-device route for normal music playback; that API is intended for communication use cases and may change routing behavior.
- Observe device changes using `AudioDeviceCallback` on supported Android versions and query the current output devices through `AudioManager`. Keep a compatibility fallback for older supported Android versions.
- If both route types are attached, use the route Android reports as active/default for media playback. The app must not promise a universal P2-over-USB priority because routing policy and device behavior vary by manufacturer and Android version.
- If no compatible wired route is available, show a clear “saída cabeada não disponível” state; do not silently claim that audio is being delivered.
- On unplug/replug, follow the newly active route and recover playback without resetting the musician's AUX mix.

### Acceptance criteria

1. P2 headset insertion is reflected as the active route when Android selects it.
2. USB-C DAC/headset insertion is reflected as the active route when Android selects it.
3. Unplugging the active route updates status and does not crash or stop the receiver permanently.
4. Switching between P2 and USB-C preserves stream identity, assigned AUX and mix state.
5. Only one output route is opened in the initial release.
6. UI identifies the active route as “P2”, “USB-C” or “sem saída cabeada”, based on actual Android device/routing information.
7. Tests cover device-added/device-removed events, unknown device types, duplicate notifications and route changes during playback.

### Future dual-output mode

Keep route detection separate from the audio rendering/stream decoder so a later version can fan out the same decoded stereo stream to two output sinks. Simultaneous P2 + USB-C is conditional on Android's audio APIs, the phone's hardware/driver and the USB adapter; it must be validated per device. It is not part of the initial release.

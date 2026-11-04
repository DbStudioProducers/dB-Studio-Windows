# dB Studio — Device History and Scene File Specification

Status: product requirement / implementation design. This document does not claim these features are implemented yet.

## 1. Persistent connected-device history

The desktop server will provide a dedicated Connection History area. Each session record must contain:
- Stable device identifier (not only its editable display name).
- Display name captured for the session.
- Connection timestamp and disconnection timestamp, stored in UTC.
- Calculated session duration; an active session is shown as ongoing and its duration is calculated from the connection timestamp.
- Session end reason when reliably known (normal disconnect, timeout, server shutdown, or unknown).
- Reconnection is a new session record, linked to the same stable device identity.

Renaming a device must not rewrite or erase historical session records. The history must survive application restarts. The desktop server is the authoritative recorder; mobile clients may keep a local cache and synchronize records, but must not create duplicate authoritative sessions. If the server closes unexpectedly, an unfinished session is reconciled on next startup and marked as interrupted/unknown rather than inventing an exact disconnect time.

Privacy and retention controls should be provided before public release. Do not store passwords, authentication tokens, audio, or unnecessary personal data in history.

## 2. Native scene file: .dBs

.dBs is the official planned extension for dB Studio scene files. The same file format and schema must be readable and writable by desktop and mobile clients. Each application saves scenes locally on the device where they are created. There is no automatic cloud synchronization, background transfer, or requirement to keep a duplicate scene library on another device. The extension is case-insensitive on import; the canonical extension is .dBs.

### Container and portability

Use a ZIP-based package with a documented internal manifest and UTF-8 JSON data. This keeps the format inspectable, portable and extensible while allowing optional assets later.

Initial package layout:
- manifest.json — format identifier, schema version, application version, creation/modification timestamps, scene identifier, and integrity metadata.
- scene.json — scene name and actual audio/mix configuration.
- assets/ — optional future user assets; absent in the initial version.

The package must not contain executable code, passwords, access tokens, Wi-Fi credentials, device private keys, or transient connection telemetry.

### Scene data

The schema is intended to represent:
- Scene identity, name, notes and timestamps.
- 32 input channel settings.
- 16 independent stereo AUX mix settings.
- Per-channel gain, pan and mute for each AUX.
- Exclusive device-to-AUX assignment references where appropriate; importing a scene must validate assignments and must never create shared AUX subscriptions.
- Processing and effect parameters only for processors supported by the schema/application version.
- Sample-rate and audio-format metadata where needed to interpret settings.

A scene stores configuration, not live audio streams or a recording of the performance. Device connection history is separate persistent application data and is not embedded in scene files.

### Compatibility and safety requirements

- Include a stable format identifier (for example, com.dbstudio.scene) and an integer schema version.
- Reject malformed, unsupported, oversized or integrity-failing packages with a clear error; never partially apply a scene.
- Validate all numeric values and clamp/reject out-of-range parameters according to the schema.
- Load into a temporary configuration, validate it, then atomically apply it.
- Save to a temporary file and atomically replace the destination after successful write.
- Preserve forward-compatibility metadata where safe; never silently discard unknown required fields.
- Provide explicit schema migrations for older versions and keep test fixtures for each supported version.
- Importing a scene must not overwrite device identity, account credentials, network secrets, or the connection history.
- Use the same cross-platform conformance test vectors on Windows, iOS and Android.

## 3. User workflows

Desktop and mobile should both support Save Scene, Save Scene As, and Open Scene using the local storage of that device. A scene created on a phone remains on that phone; a scene created on the desktop remains on the desktop. Optional user-initiated export/share or manual import may be offered for backup or transfer, but must never run automatically. Avoid keeping redundant copies or transmitting scene files in the background. The format should remain compact: store only scene configuration, omit audio/media payloads and unnecessary assets, and use compression. The user controls any copy or transfer.

## 4. Implementation order

1. Define and test the versioned schema and ZIP package reader/writer.
2. Add desktop save/open UI and atomic persistence.
3. Add mobile-compatible conformance tests and implement the same schema in the receiver app.
4. Add optional manual export/import and migration tests; do not implement automatic scene synchronization.
5. Add persistent connection-session storage and the Connection History interface.

These are requirements and design decisions until code and automated tests demonstrate otherwise.
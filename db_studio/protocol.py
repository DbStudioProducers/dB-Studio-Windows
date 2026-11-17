"""Version 1 packet codec for the virtual-lab audio transport.

Wire header: magic, version, channel flags, stream id, sample rate,
sequence, frame count, reserved. Payload is interleaved little-endian float32.
This codec is a simulator/test utility; it is not yet the production sender.
"""
import struct
import numpy as np

_HEADER = struct.Struct("!4sBBHIIII")
_SUPPORTED_MAGIC = (b"DBS1", b"DBO1")
_MIN_RATE = 8000
_MAX_RATE = 384000
_MAX_FRAMES = 8192
_MAX_CHANNELS = 32


def encode_pcm(magic, stream_id, sample_rate, sequence, audio):
    if magic not in _SUPPORTED_MAGIC:
        raise ValueError("Invalid protocol magic")
    if not isinstance(stream_id, (int, np.integer)) or not 0 <= int(stream_id) <= 65535:
        raise ValueError("stream_id must be an unsigned 16-bit integer")
    if not isinstance(sample_rate, (int, np.integer)) or not _MIN_RATE <= int(sample_rate) <= _MAX_RATE:
        raise ValueError("sample_rate is outside the supported range")
    if not isinstance(sequence, (int, np.integer)):
        raise ValueError("sequence must be an integer")

    a = np.asarray(audio, dtype="<f4", order="C")
    if a.ndim != 2 or not 1 <= a.shape[1] <= _MAX_CHANNELS:
        raise ValueError("Audio must be frames x 1..32 channels")
    if not 1 <= a.shape[0] <= _MAX_FRAMES:
        raise ValueError("Audio frame count is outside the supported range")
    if magic == b"DBO1" and a.shape[1] != 2:
        raise ValueError("Output streams must contain exactly two channels")
    if not np.isfinite(a).all():
        raise ValueError("Audio payload must contain only finite samples")

    flags = a.shape[1] if magic == b"DBS1" else 0
    header = _HEADER.pack(
        magic, 1, flags, int(stream_id), int(sample_rate),
        int(sequence) & 0xFFFFFFFF, int(a.shape[0]), 0
    )
    return header + a.tobytes(order="C")


def decode_pcm(packet, expected_magic):
    if expected_magic not in _SUPPORTED_MAGIC:
        raise ValueError("Invalid expected protocol magic")
    if not isinstance(packet, (bytes, bytearray, memoryview)):
        raise ValueError("Packet must be bytes-like")
    if len(packet) < _HEADER.size:
        raise ValueError("Packet shorter than header")

    magic, version, flags, stream_id, rate, seq, frames, reserved = _HEADER.unpack_from(packet)
    if magic != expected_magic or version != 1:
        raise ValueError("Unsupported packet")
    if reserved != 0:
        raise ValueError("Reserved header field must be zero")
    if not _MIN_RATE <= rate <= _MAX_RATE:
        raise ValueError("Invalid sample rate")
    if not 1 <= frames <= _MAX_FRAMES:
        raise ValueError("Invalid frame count")

    if expected_magic == b"DBO1":
        if flags != 0:
            raise ValueError("Output packet flags must be zero")
        channels = 2
    else:
        channels = flags
        if not 1 <= channels <= _MAX_CHANNELS:
            raise ValueError("Invalid input channel count")

    payload = packet[_HEADER.size:]
    expected_size = frames * channels * 4
    if len(payload) != expected_size:
        raise ValueError("Invalid PCM payload size")
    audio = np.frombuffer(payload, dtype="<f4").reshape(frames, channels).copy()
    if not np.isfinite(audio).all():
        raise ValueError("PCM payload contains non-finite samples")
    return stream_id, rate, seq, audio

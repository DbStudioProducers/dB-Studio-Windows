import struct
import numpy as np
_HEADER = struct.Struct("!4sBBHIIII")

def encode_pcm(magic, stream_id, sample_rate, sequence, audio):
    if magic not in (b"DBS1", b"DBO1"):
        raise ValueError("Invalid protocol magic")
    a = np.asarray(audio, dtype="<f4", order="C")
    if a.ndim != 2 or not 1 <= a.shape[1] <= 32:
        raise ValueError("Audio must be frames x 1..32 channels")
    flags = a.shape[1] if magic == b"DBS1" else 0
    header = _HEADER.pack(magic, 1, flags, int(stream_id), int(sample_rate), int(sequence)&0xffffffff, int(a.shape[0]), 0)
    return header + a.tobytes(order="C")

def decode_pcm(packet, expected_magic):
    if len(packet) < _HEADER.size:
        raise ValueError("Packet shorter than header")
    magic, version, flags, stream_id, rate, seq, frames, reserved = _HEADER.unpack_from(packet)
    if magic != expected_magic or version != 1:
        raise ValueError("Unsupported packet")
    channels = 2 if expected_magic == b"DBO1" else flags
    payload = packet[_HEADER.size:]
    if not 1 <= channels <= 32 or len(payload) != frames*channels*4:
        raise ValueError("Invalid PCM payload size")
    return stream_id, rate, seq, np.frombuffer(payload, dtype="<f4").reshape(frames, channels).copy()

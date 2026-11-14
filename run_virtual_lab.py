import argparse
import math
import socket
import threading
import time

import numpy as np

from db_studio.simulator import VirtualConsole
from db_studio.mixer import render_all
from db_studio.protocol import encode_pcm, decode_pcm

# IPv4 UDP payload limit minus the 24-byte dB Studio packet header.
# Stereo float32 consumes eight bytes per frame.
MAX_UDP_PAYLOAD = 65507
PACKET_HEADER_BYTES = 24
MAX_UDP_STEREO_FRAMES = (MAX_UDP_PAYLOAD - PACKET_HEADER_BYTES) // 8


def validate_args(seconds, rate, blocksize):
    if not math.isfinite(seconds) or seconds < 0.05:
        raise ValueError("seconds must be finite and at least 0.05")
    if not 8000 <= rate <= 384000:
        raise ValueError("rate must be between 8000 and 384000 Hz")
    if not 1 <= blocksize <= MAX_UDP_STEREO_FRAMES:
        raise ValueError(
            f"blocksize must be between 1 and {MAX_UDP_STEREO_FRAMES} "
            "frames for a stereo float32 UDP packet"
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--seconds", type=float, default=2.0)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--blocksize", type=int, default=128)
    args = parser.parse_args()
    try:
        validate_args(args.seconds, args.rate, args.blocksize)
    except ValueError as exc:
        parser.error(str(exc))

    console = VirtualConsole(32, args.rate)
    receivers = []
    received = [set() for _ in range(16)]
    peaks = [0.0] * 16
    receiver_errors = [[] for _ in range(16)]
    send_errors = []
    stop = threading.Event()

    for _ in range(16):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(("127.0.0.1", 0))
        sock.settimeout(0.1)
        receivers.append((sock, sock.getsockname()[1]))

    def receive(index, sock):
        while not stop.is_set():
            try:
                packet, _ = sock.recvfrom(MAX_UDP_PAYLOAD)
            except socket.timeout:
                continue
            except OSError as exc:
                if not stop.is_set():
                    receiver_errors[index].append(f"socket: {exc}")
                return

            try:
                stream_id, sample_rate, sequence, audio = decode_pcm(packet, b"DBO1")
                if stream_id != index + 1:
                    receiver_errors[index].append(
                        f"wrong stream id {stream_id}, expected {index + 1}"
                    )
                    continue
                if sample_rate != args.rate:
                    receiver_errors[index].append(
                        f"wrong sample rate {sample_rate}, expected {args.rate}"
                    )
                    continue
                if audio.shape != (args.blocksize, 2):
                    receiver_errors[index].append(
                        f"wrong block shape {audio.shape}, expected ({args.blocksize}, 2)"
                    )
                    continue
                if sequence in received[index]:
                    receiver_errors[index].append(f"duplicate sequence {sequence}")
                    continue
                received[index].add(sequence)
                peaks[index] = max(peaks[index], float(np.max(np.abs(audio))))
            except ValueError as exc:
                receiver_errors[index].append(f"invalid packet: {exc}")

    threads = [
        threading.Thread(target=receive, args=(i, sock), daemon=True)
        for i, (sock, _) in enumerate(receivers)
    ]
    for thread in threads:
        thread.start()

    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mixes = [
        {
            "master": 0.65,
            "channels": {
                str(channel): {"volume": 0.25, "pan": 0.0}
                for channel in range(1, 33)
            },
        }
        for _ in range(16)
    ]

    start = time.monotonic()
    sequence = 0
    try:
        while time.monotonic() - start < args.seconds:
            block = console.read(args.blocksize)
            buses = render_all(block, mixes)
            for index, bus in enumerate(buses):
                packet = encode_pcm(b"DBO1", index + 1, args.rate, sequence, bus)
                sender.sendto(packet, ("127.0.0.1", receivers[index][1]))
            sequence += 1
            time.sleep(args.blocksize / args.rate)
    except OSError as exc:
        send_errors.append(f"sender socket: {exc}")
    finally:
        sender.close()

    # Give receiver threads a bounded drain window before stopping them.
    drain_deadline = time.monotonic() + 1.0
    expected = set(range(sequence))
    while time.monotonic() < drain_deadline:
        if all(received_set == expected for received_set in received):
            break
        time.sleep(0.01)

    stop.set()
    for thread in threads:
        thread.join(timeout=1)
    for sock, _ in receivers:
        sock.close()

    print("dB Studio Virtual Lab")
    print(f"32 canais simulados | 16 mixes estéreo | {args.rate} Hz")
    ok = sequence > 0 and not send_errors
    for index in range(16):
        missing = expected - received[index]
        unexpected = received[index] - expected
        if missing:
            receiver_errors[index].append(f"{len(missing)} pacote(s) ausente(s)")
        if unexpected:
            receiver_errors[index].append(f"{len(unexpected)} sequência(s) inesperada(s)")
        if receiver_errors[index]:
            ok = False
        print(
            f"Mix {index + 1:02d}: {len(received[index])}/{sequence} pacotes "
            f"| pico {peaks[index]:.4f}"
        )
        for error in receiver_errors[index]:
            print(f"  ERRO: {error}")

    for error in send_errors:
        print(f"ERRO: {error}")

    print("RESULTADO:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())

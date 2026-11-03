import argparse, socket, threading, time
import numpy as np
from db_studio.simulator import VirtualConsole
from db_studio.mixer import render_all
from db_studio.protocol import encode_pcm, decode_pcm

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--seconds", type=float, default=2.0)
    p.add_argument("--rate", type=int, default=48000)
    p.add_argument("--blocksize", type=int, default=128)
    args = p.parse_args()
    console = VirtualConsole(32, args.rate)
    receivers = []
    counts = [0]*16
    peaks = [0.0]*16
    stop = threading.Event()
    for i in range(16):
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.bind(("127.0.0.1", 0)); s.settimeout(.2)
        receivers.append((s, s.getsockname()[1]))
    def receive(index, sock):
        while not stop.is_set():
            try:
                packet, _ = sock.recvfrom(65507)
                _, _, _, audio = decode_pcm(packet, b"DBO1")
                counts[index] += 1
                peaks[index] = max(peaks[index], float(np.max(np.abs(audio))))
            except socket.timeout:
                continue
    threads = [threading.Thread(target=receive, args=(i,s), daemon=True) for i,(s,_) in enumerate(receivers)]
    for t in threads: t.start()
    sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    mixes = [{"master": 0.65, "channels": {str(ch): {"volume": 0.25, "pan": 0.0} for ch in range(1,33)}} for _ in range(16)]
    start = time.monotonic(); seq = 0
    while time.monotonic()-start < args.seconds:
        block = console.read(args.blocksize)
        buses = render_all(block, mixes)
        for i,bus in enumerate(buses):
            sender.sendto(encode_pcm(b"DBO1", i+1, args.rate, seq, bus), ("127.0.0.1", receivers[i][1]))
        seq += 1
        time.sleep(args.blocksize/args.rate)
    time.sleep(.3); stop.set()
    for t in threads: t.join(timeout=1)
    sender.close()
    for s,_ in receivers: s.close()
    ok = all(c > 0 for c in counts)
    print("dB Studio Virtual Lab")
    print(f"32 canais simulados | 16 mixes estéreo | {args.rate} Hz")
    for i,c in enumerate(counts,1): print(f"Mix {i:02d}: {c} pacotes | pico {peaks[i-1]:.4f}")
    print("RESULTADO:", "PASS" if ok else "FAIL")
    return 0 if ok else 1

if __name__ == "__main__":
    raise SystemExit(main())

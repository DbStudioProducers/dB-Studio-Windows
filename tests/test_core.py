import struct
import unittest

import numpy as np

from db_studio.simulator import VirtualConsole
from db_studio.mixer import render_all, render_bus
from db_studio.protocol import encode_pcm, decode_pcm, _HEADER


class CoreTests(unittest.TestCase):
    def test_32_channels(self):
        self.assertEqual(VirtualConsole().read(128).shape, (128, 32))

    def test_16_stereo_mixes(self):
        x = VirtualConsole().read(128)
        buses = render_all(x, [{"channels": {"1": {"volume": .5}}} for _ in range(16)])
        self.assertEqual(len(buses), 16)
        self.assertTrue(all(b.shape == (128, 2) for b in buses))

    def test_mixer_mute_solo_and_isolation(self):
        x = np.ones((8, 3), dtype=np.float32) * .1
        mix = {"channels": {
            "1": {"volume": 1.0},
            "2": {"volume": 1.0, "solo": True},
            "3": {"volume": 1.0, "mute": True, "solo": True},
        }, "master": 1.0}
        out = render_bus(x, mix)
        self.assertTrue(np.allclose(out, .1, atol=1e-6))
        # A SOLO in this mix excludes non-solo channels; MUTE still wins.
        self.assertTrue(np.allclose(out[:, 0], .1, atol=1e-6))
        self.assertTrue(np.allclose(out[:, 1], .1, atol=1e-6))

    def test_packet_roundtrip(self):
        x = np.zeros((16, 2), dtype=np.float32)
        packet = encode_pcm(b"DBO1", 2, 48000, 4, x)
        sid, rate, seq, y = decode_pcm(packet, b"DBO1")
        self.assertEqual((sid, rate, seq), (2, 48000, 4))
        self.assertTrue(np.array_equal(x, y))

    def test_input_packet_channel_count_and_sequence_wrap(self):
        x = np.arange(24, dtype=np.float32).reshape(8, 3)
        packet = encode_pcm(b"DBS1", 65535, 96000, 0x100000001, x)
        sid, rate, seq, y = decode_pcm(packet, b"DBS1")
        self.assertEqual((sid, rate, seq), (65535, 96000, 1))
        self.assertTrue(np.array_equal(x, y))

    def test_rejects_invalid_encode_shapes_and_metadata(self):
        cases = [
            (b"NOPE", 1, 48000, 0, np.zeros((4, 2))),
            (b"DBO1", 1, 48000, 0, np.zeros((4, 1))),
            (b"DBS1", 1, 48000, 0, np.zeros((4, 33))),
            (b"DBO1", 1, 1000, 0, np.zeros((4, 2))),
            (b"DBO1", 65536, 48000, 0, np.zeros((4, 2))),
            (b"DBO1", 1, 48000, 0, np.zeros((0, 2))),
            (b"DBO1", 1, 48000, 0, np.zeros((8193, 2))),
        ]
        for args in cases:
            with self.subTest(args=args[:4]):
                with self.assertRaises(ValueError):
                    encode_pcm(*args)

    def test_rejects_malformed_packets(self):
        valid = encode_pcm(b"DBO1", 1, 48000, 0, np.zeros((4, 2)))
        malformed = [
            valid[:-1],
            valid + b"extra",
            b"x" * (_HEADER.size - 1),
            _HEADER.pack(b"DBO1", 2, 0, 1, 48000, 0, 4, 0) + valid[_HEADER.size:],
            _HEADER.pack(b"DBO1", 1, 1, 1, 48000, 0, 4, 0) + valid[_HEADER.size:],
            _HEADER.pack(b"DBO1", 1, 0, 1, 48000, 0, 4, 1) + valid[_HEADER.size:],
            _HEADER.pack(b"DBO1", 1, 0, 1, 1000, 0, 4, 0) + valid[_HEADER.size:],
            _HEADER.pack(b"DBO1", 1, 0, 1, 48000, 0, 0, 0),
            _HEADER.pack(b"DBS1", 1, 33, 1, 48000, 0, 4, 0) + b"\0" * 16,
        ]
        for packet in malformed:
            with self.subTest(length=len(packet)):
                with self.assertRaises(ValueError):
                    decode_pcm(packet, b"DBO1" if packet[:4] != b"DBS1" else b"DBS1")


if __name__ == "__main__":
    unittest.main()

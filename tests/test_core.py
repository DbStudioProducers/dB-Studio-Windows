import unittest

import numpy as np

from db_studio.mixer import render_all, render_bus
from db_studio.protocol import _HEADER, decode_pcm, encode_pcm
from db_studio.simulator import VirtualConsole


class CoreTests(unittest.TestCase):
    def test_32_channels(self):
        self.assertEqual(VirtualConsole().read(128).shape, (128, 32))

    def test_16_independent_stereo_mixes(self):
        audio = np.full((128, 32), 0.2, dtype=np.float32)
        mixes = [
            {"master": 1.0, "channels": {"1": {"volume": 0.5}}},
            {"master": 1.0, "channels": {"2": {"volume": 0.25}}},
        ] + [{} for _ in range(14)]
        buses = render_all(audio, mixes)
        self.assertEqual(len(buses), 16)
        self.assertTrue(all(bus.shape == (128, 2) for bus in buses))
        self.assertTrue(np.allclose(buses[0], 0.2 * 0.5 * np.sqrt(0.5), atol=1e-6))
        self.assertTrue(np.allclose(buses[1], 0.2 * 0.25 * np.sqrt(0.5), atol=1e-6))
        self.assertTrue(np.allclose(buses[2], 0.0, atol=1e-7))

    def test_mixer_mute_solo_and_isolation(self):
        audio = np.full((8, 3), 0.1, dtype=np.float32)
        solo_mix = {
            "master": 1.0,
            "channels": {
                "1": {"volume": 1.0},
                "2": {"volume": 1.0, "solo": True},
                "3": {"volume": 1.0, "mute": True, "solo": True},
            },
        }
        solo_output = render_bus(audio, solo_mix)
        expected_single = 0.1 * np.sqrt(0.5)
        self.assertTrue(np.allclose(solo_output, expected_single, atol=1e-6))

        normal_mix = {
            "master": 1.0,
            "channels": {
                "1": {"volume": 1.0},
                "2": {"volume": 1.0},
                "3": {"volume": 1.0, "mute": True},
            },
        }
        normal_output = render_bus(audio, normal_mix)
        self.assertTrue(np.allclose(normal_output, 2 * expected_single, atol=1e-6))

    def test_non_finite_simulated_inputs_become_silence(self):
        audio = np.array([[np.nan, np.inf, -np.inf]], dtype=np.float32)
        output = render_bus(audio, {"master": 1.0, "channels": {
            "1": {"volume": 1.0}, "2": {"volume": 1.0}, "3": {"volume": 1.0}
        }})
        self.assertTrue(np.all(np.isfinite(output)))
        self.assertTrue(np.allclose(output, 0.0))

    def test_packet_roundtrip(self):
        audio = np.zeros((16, 2), dtype=np.float32)
        packet = encode_pcm(b"DBO1", 2, 48000, 4, audio)
        stream_id, rate, sequence, decoded = decode_pcm(packet, b"DBO1")
        self.assertEqual((stream_id, rate, sequence), (2, 48000, 4))
        self.assertTrue(np.array_equal(audio, decoded))

    def test_input_packet_channel_count_and_sequence_wrap(self):
        audio = np.arange(24, dtype=np.float32).reshape(8, 3)
        packet = encode_pcm(b"DBS1", 65535, 96000, 0x100000001, audio)
        stream_id, rate, sequence, decoded = decode_pcm(packet, b"DBS1")
        self.assertEqual((stream_id, rate, sequence), (65535, 96000, 1))
        self.assertTrue(np.array_equal(audio, decoded))

    def test_rejects_invalid_encode_shapes_and_metadata(self):
        cases = [
            (b"NOPE", 1, 48000, 0, np.zeros((4, 2))),
            (b"DBO1", 1, 48000, 0, np.zeros((4, 1))),
            (b"DBS1", 1, 48000, 0, np.zeros((4, 33))),
            (b"DBO1", 1, 1000, 0, np.zeros((4, 2))),
            (b"DBO1", 65536, 48000, 0, np.zeros((4, 2))),
            (b"DBO1", 1, 48000, 0, np.zeros((0, 2))),
            (b"DBO1", 1, 48000, 0, np.zeros((8193, 2))),
            (b"DBO1", 1, 48000, 0, np.array([[np.nan, 0.0]])),
        ]
        for args in cases:
            with self.subTest(metadata=args[:4]):
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
            _HEADER.pack(b"DBS1", 1, 33, 1, 48000, 0, 4, 0) + b"\\0" * 16,
            _HEADER.pack(b"DBO1", 1, 0, 1, 48000, 0, 4, 0)
                + np.array([np.inf] * 8, dtype="<f4").tobytes(),
        ]
        for packet in malformed:
            expected_magic = b"DBS1" if packet[:4] == b"DBS1" else b"DBO1"
            with self.subTest(length=len(packet)):
                with self.assertRaises(ValueError):
                    decode_pcm(packet, expected_magic)


if __name__ == "__main__":
    unittest.main()

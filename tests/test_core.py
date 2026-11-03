import unittest
import numpy as np
from db_studio.simulator import VirtualConsole
from db_studio.mixer import render_all
from db_studio.protocol import encode_pcm, decode_pcm

class CoreTests(unittest.TestCase):
    def test_32_channels(self):
        self.assertEqual(VirtualConsole().read(128).shape, (128,32))
    def test_16_stereo_mixes(self):
        x = VirtualConsole().read(128)
        buses = render_all(x, [{"channels": {"1": {"volume": .5}}} for _ in range(16)])
        self.assertEqual(len(buses), 16)
        self.assertTrue(all(b.shape == (128,2) for b in buses))
    def test_packet_roundtrip(self):
        x = np.zeros((16,2), dtype=np.float32)
        packet = encode_pcm(b"DBO1", 2, 48000, 4, x)
        sid, rate, seq, y = decode_pcm(packet, b"DBO1")
        self.assertEqual((sid,rate,seq), (2,48000,4))
        self.assertTrue(np.array_equal(x,y))

if __name__ == "__main__":
    unittest.main()

import numpy as np

class VirtualConsole:
    def __init__(self, channels=32, sample_rate=48000, seed=7):
        self.channels = min(32, max(1, int(channels)))
        self.sample_rate = int(sample_rate)
        self.phase = np.zeros(self.channels, dtype=np.float64)
        self.freqs = np.linspace(110.0, 880.0, self.channels)

    def read(self, frames=128):
        frames = int(frames)
        t = np.arange(frames, dtype=np.float64) / self.sample_rate
        phase = self.phase[:, None] + 2*np.pi*self.freqs[:, None]*t[None, :]
        block = (0.12*np.sin(phase)).T.astype(np.float32)
        self.phase = (self.phase + 2*np.pi*self.freqs*frames/self.sample_rate) % (2*np.pi)
        return block

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NativeArchitectureChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        cls.engine_h = (ROOT / 'Source' / 'AudioEngine.h').read_text(encoding='utf-8')
        cls.engine_cpp = (ROOT / 'Source' / 'AudioEngine.cpp').read_text(encoding='utf-8')
        cls.readme = (ROOT / 'README.md').read_text(encoding='utf-8')

    def test_juce_8_is_pinned(self):
        self.assertRegex(self.cmake, r'GIT_TAG\s+8\.0\.6')

    def test_audio_architecture_has_32_inputs_and_16_stereo_mixes(self):
        self.assertRegex(self.engine_h, r'kInputChannels\s*=\s*32')
        self.assertRegex(self.engine_h, r'kMixes\s*=\s*16')
        self.assertRegex(self.engine_h, r'kOutputChannels\s*=\s*32')

    def test_aux_rendering_is_not_limited_by_physical_outputs(self):
        self.assertIn('Render every AUX bus regardless of physical output availability', self.engine_cpp)
        self.assertIn('for (int mix = 0; mix < kMixes; ++mix)', self.engine_cpp)
        self.assertIn('for (int out = 0; out < numOutputs; ++out)', self.engine_cpp)

    def test_network_handoff_is_bounded_and_non_blocking(self):
        self.assertRegex(self.engine_h, r'kQueueSlots\s*=\s*3')
        self.assertIn('droppedNetworkBlocks_', self.engine_h)
        self.assertIn('queueWrite_.store(write + 1, std::memory_order_release)', self.engine_cpp)
        self.assertIn('droppedNetworkBlocks_.fetch_add(1, std::memory_order_relaxed)', self.engine_cpp)

    def test_product_requirements_are_documented_not_misrepresented_as_complete(self):
        spec = (ROOT / 'docs' / 'DEVICE_HISTORY_AND_DBS_FORMAT.md').read_text(encoding='utf-8')
        self.assertIn('Connection History', spec)
        self.assertIn('.dBs', spec)
        self.assertIn('multitrack recording', spec.lower())
        self.assertIn('does not claim these features are implemented yet', spec.lower())

if __name__ == '__main__':
    unittest.main(verbosity=2)
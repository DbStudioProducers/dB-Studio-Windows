from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NativeArchitectureChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        cls.engine_h = (ROOT / 'Source' / 'AudioEngine.h').read_text(encoding='utf-8')
        cls.engine_cpp = (ROOT / 'Source' / 'AudioEngine.cpp').read_text(encoding='utf-8')
        cls.renderer = (ROOT / 'Source' / 'MixRenderer.h').read_text(encoding='utf-8')

    def test_juce_8_is_pinned(self):
        self.assertIn('GIT_TAG 8.0.6', self.cmake)

    def test_audio_architecture_has_32_inputs_and_16_stereo_mixes(self):
        self.assertIn('constexpr int kInputs = 32;', self.renderer)
        self.assertIn('constexpr int kMixes = 16;', self.renderer)
        self.assertIn('constexpr int kBuses = kMixes * 2;', self.renderer)
        self.assertIn('DbStudioMix::kInputs', self.engine_h)
        self.assertIn('DbStudioMix::kMixes', self.engine_h)
        self.assertIn('DbStudioMix::kBuses', self.engine_h)

    def test_aux_rendering_is_not_limited_by_physical_outputs(self):
        self.assertIn('for (int out = 0; out < numOutputs; ++out)', self.engine_cpp)
        self.assertIn('DbStudioMix::render(inputs, availableInputs, samples', self.engine_cpp)
        self.assertIn('for (int mix = 0; mix < kMixes; ++mix)', self.renderer)
        self.assertIn('BusBuffers', self.renderer)

    def test_non_finite_audio_samples_and_controls_are_sanitized(self):
        self.assertIn('std::isfinite(value)', self.engine_cpp)
        self.assertIn('std::isfinite(raw)', self.renderer)
        self.assertIn('std::isfinite(gain)', self.engine_cpp)
        self.assertIn('std::isfinite(pan)', self.engine_cpp)
        self.assertIn('std::clamp(l, -0.98f, 0.98f)', self.renderer)
        self.assertIn('std::clamp(r, -0.98f, 0.98f)', self.renderer)

    def test_network_handoff_is_bounded_and_non_blocking(self):
        self.assertIn('SpscAudioBlockQueue<kOutputChannels, kMaxBlockSize, 3>', self.engine_h)
        self.assertIn('networkQueue_.tryPush(networkChannels.data(), samples)', self.engine_cpp)
        self.assertIn('networkQueue_.tryAcquire(view)', self.engine_cpp)
        self.assertIn('SpscAudioQueue', self.cmake)
        workflow = (ROOT / '.github' / 'workflows' / 'windows-build.yml').read_text(encoding='utf-8')
        self.assertIn('dBStudioQueueTests', workflow)
        self.assertIn('needs: python-virtual-lab', workflow)
        self.assertIn('cancel-in-progress: true', workflow)
        ordered_steps = [
            'Verify native architecture before compilation',
            'Configure JUCE native application and core tests',
            'Build core verification tests',
            'Run core verification tests before application compilation',
            'Compile native application',
            'Upload native application',
        ]
        positions = [workflow.index(step) for step in ordered_steps]
        self.assertEqual(positions, sorted(positions))

    def test_product_requirements_are_documented_not_misrepresented_as_complete(self):
        spec = (ROOT / 'docs' / 'DEVICE_HISTORY_AND_DBS_FORMAT.md').read_text(encoding='utf-8')
        self.assertIn('Connection History', spec)
        self.assertIn('.dBs', spec)
        self.assertIn('multitrack recording', spec.lower())
        self.assertIn('does not claim these features are implemented yet', spec.lower())

if __name__ == '__main__':
    unittest.main(verbosity=2)

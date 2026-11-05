#include "AudioEngine.h"
#include "MixRenderer.h"
#include <algorithm>
#include <cmath>

AudioEngine::AudioEngine() {
    for (auto& peak : inputPeaks_) peak.store(0.0f);
    for (auto& peak : outputPeaks_) peak.store(0.0f);
    for (int mix = 0; mix < kMixes; ++mix)
        for (int input = 0; input < kInputChannels; ++input) {
            inputGains_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(0.125f);
            inputPans_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(0.0f);
            inputMutes_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(false);
        }
}

AudioEngine::~AudioEngine() { shutdown(); }

juce::String AudioEngine::initialise() {
    // 32 inputs are requested; physical stereo output is optional local
    // monitoring only. Wi-Fi distribution uses the internal AUX buses.
    const auto error = deviceManager_.initialise(kInputChannels, 2, nullptr, true);
    if (error.isNotEmpty())
        return error;
    deviceManager_.addAudioCallback(this);
    return {};
}

void AudioEngine::shutdown() {
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    if (device != nullptr) {
        sampleRate_.store(device->getCurrentSampleRate());
        blockSize_.store(device->getCurrentBufferSizeSamples());
    }
}

void AudioEngine::audioDeviceStopped() {
    sampleRate_.store(0.0);
    blockSize_.store(0);
    for (auto& peak : inputPeaks_) peak.store(0.0f);
    for (auto& peak : outputPeaks_) peak.store(0.0f);
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputs, int numInputs,
                                        float* const* outputs, int numOutputs, int samples,
                                        const juce::AudioIODeviceCallbackContext&) {
    // Physical outputs are deliberately silent in this foundation. They are
    // not the AUX destinations; all 16 stereo mixes are rendered below.
    for (int out = 0; out < numOutputs; ++out)
        if (outputs[out] != nullptr) juce::FloatVectorOperations::clear(outputs[out], samples);

    std::array<float, kInputChannels> peaks{};
    std::array<float, kMixes> mixPeaks{};

    // Guard the fixed, preallocated buffers. Oversized callbacks are dropped
    // safely rather than allocating memory on the real-time audio thread.
    if (samples <= 0 || samples > kMaxBlockSize) {
        for (auto& peak : inputPeaks_) peak.store(0.0f);
        for (auto& peak : outputPeaks_) peak.store(0.0f);
        return;
    }

    const int availableInputs = std::min(numInputs, kInputChannels);
    for (int channel = 0; channel < availableInputs; ++channel) {
        if (inputs[channel] == nullptr) continue;
        float peak = 0.0f;
        for (int sample = 0; sample < samples; ++sample) {
            const float value = inputs[channel][sample];
            if (std::isfinite(value))
                peak = juce::jmax(peak, std::abs(value));
        }
        peaks[static_cast<size_t>(channel)] = peak;
    }

    // Cache per-channel controls once per block; no locks, allocation or
    // trigonometric operations occur inside the sample loop.
    std::array<std::array<float, kInputChannels>, kMixes> gainLeft{};
    std::array<std::array<float, kInputChannels>, kMixes> gainRight{};
    std::array<std::array<bool, kInputChannels>, kMixes> enabled{};
    for (int mix = 0; mix < kMixes; ++mix) {
        const auto m = static_cast<size_t>(mix);
        for (int channel = 0; channel < availableInputs; ++channel) {
            const auto ch = static_cast<size_t>(channel);
            const bool active = inputs[channel] != nullptr && !inputMutes_[m][ch].load();
            enabled[m][ch] = active;
            if (!active) continue;
            const float gain = inputGains_[m][ch].load();
            const float pan = inputPans_[m][ch].load();
            const float angle = (pan + 1.0f) * 0.78539816339f;
            gainLeft[m][ch] = gain * std::cos(angle);
            gainRight[m][ch] = gain * std::sin(angle);
        }
    }

    // Render all 16 stereo AUX buses with the shared, independently tested core.
    DbStudioMix::render(inputs, availableInputs, samples, gainLeft, gainRight,
                        enabled, mixBusBuffers_, mixPeaks);


    const auto write = queueWrite_.load(std::memory_order_relaxed);
    const auto read = queueRead_.load(std::memory_order_acquire);
    if (write - read < kQueueSlots) {
        auto& block = networkQueue_[write % kQueueSlots];
        block.frames = samples;
        block.sequence = nextSequence_.fetch_add(1, std::memory_order_relaxed);
        for (int bus = 0; bus < kOutputChannels; ++bus)
            std::copy_n(mixBusBuffers_[static_cast<size_t>(bus)].data(), samples,
                        block.samples.data() + static_cast<size_t>(bus * kMaxBlockSize));
        queueWrite_.store(write + 1, std::memory_order_release);
    } else {
        droppedNetworkBlocks_.fetch_add(1, std::memory_order_relaxed);
    }

    for (int channel = 0; channel < kInputChannels; ++channel)
        inputPeaks_[static_cast<size_t>(channel)].store(peaks[static_cast<size_t>(channel)]);
    for (int mix = 0; mix < kMixes; ++mix)
        outputPeaks_[static_cast<size_t>(mix)].store(mixPeaks[static_cast<size_t>(mix)]);
}


bool AudioEngine::tryAcquireNetworkBlock(const float*& samples, int& frames, unsigned long long& sequence) noexcept {
    const auto read = queueRead_.load(std::memory_order_relaxed);
    const auto write = queueWrite_.load(std::memory_order_acquire);
    if (read == write || networkBlockAcquired_) return false;
    const auto& block = networkQueue_[read % kQueueSlots];
    samples = block.samples.data();
    frames = block.frames;
    sequence = block.sequence;
    networkBlockAcquired_ = true;
    return true;
}
void AudioEngine::releaseNetworkBlock() noexcept {
    if (!networkBlockAcquired_) return;
    queueRead_.fetch_add(1, std::memory_order_release);
    networkBlockAcquired_ = false;
}

float AudioEngine::inputPeak(int channel) const noexcept {
    return channel >= 0 && channel < kInputChannels
        ? inputPeaks_[static_cast<size_t>(channel)].load() : 0.0f;
}
float AudioEngine::outputPeak(int mix) const noexcept {
    return mix >= 0 && mix < kMixes
        ? outputPeaks_[static_cast<size_t>(mix)].load() : 0.0f;
}

void AudioEngine::setMixInputGain(int mix, int input, float gain) noexcept {
    if (mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels)
        inputGains_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(
            std::isfinite(gain) ? juce::jlimit(0.0f, 2.0f, gain) : 0.0f);
}
void AudioEngine::setMixInputPan(int mix, int input, float pan) noexcept {
    if (mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels)
        inputPans_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(
            std::isfinite(pan) ? juce::jlimit(-1.0f, 1.0f, pan) : 0.0f);
}
void AudioEngine::setMixInputMuted(int mix, int input, bool muted) noexcept {
    if (mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels)
        inputMutes_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(muted);
}
float AudioEngine::mixInputGain(int mix, int input) const noexcept {
    return mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels
        ? inputGains_[static_cast<size_t>(mix)][static_cast<size_t>(input)].load() : 0.0f;
}
float AudioEngine::mixInputPan(int mix, int input) const noexcept {
    return mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels
        ? inputPans_[static_cast<size_t>(mix)][static_cast<size_t>(input)].load() : 0.0f;
}
bool AudioEngine::mixInputMuted(int mix, int input) const noexcept {
    return mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels
        ? inputMutes_[static_cast<size_t>(mix)][static_cast<size_t>(input)].load() : true;
}

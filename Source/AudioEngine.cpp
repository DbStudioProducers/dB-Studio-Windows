#include "AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <limits>

AudioEngine::AudioEngine() {
    for (auto& peak : inputPeaks_) peak.store(0.0f);
    for (auto& peak : outputPeaks_) peak.store(0.0f);
    for (int mix = 0; mix < kMixes; ++mix)
        for (int input = 0; input < kInputChannels; ++input) {
            inputGains_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(0.125f);
            inputPans_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(0.0f);
            inputMutes_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(false);
            inputSolos_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(false);
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
    audioFrameCursor_ = 0;
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
    const auto blockTimestampFrames = audioFrameCursor_;
    if (samples > 0 && audioFrameCursor_ <=
            std::numeric_limits<std::uint64_t>::max() - static_cast<std::uint64_t>(samples))
        audioFrameCursor_ += static_cast<std::uint64_t>(samples);
    // Physical outputs are deliberately silent in this foundation. They are
    // not the AUX destinations; all 16 stereo mixes are rendered below.
    for (int out = 0; out < numOutputs; ++out)
        if (outputs != nullptr && outputs[out] != nullptr) juce::FloatVectorOperations::clear(outputs[out], samples);

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
        if (inputs == nullptr || inputs[channel] == nullptr) continue;
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
        bool anySolo = false;
        for (int channel = 0; channel < availableInputs; ++channel)
            anySolo = anySolo || inputSolos_[m][static_cast<size_t>(channel)].load();
        for (int channel = 0; channel < availableInputs; ++channel) {
            const auto ch = static_cast<size_t>(channel);
            const bool active = inputs != nullptr && inputs[channel] != nullptr &&
                DbStudioMix::isInputAudible(inputMutes_[m][ch].load(), inputSolos_[m][ch].load(), anySolo);
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


    std::array<const float*, kOutputChannels> networkChannels{};
    for (int bus = 0; bus < kOutputChannels; ++bus)
        networkChannels[static_cast<size_t>(bus)] = mixBusBuffers_[static_cast<size_t>(bus)].data();
    networkQueue_.tryPush(networkChannels.data(), samples, blockTimestampFrames);

    for (int channel = 0; channel < kInputChannels; ++channel)
        inputPeaks_[static_cast<size_t>(channel)].store(peaks[static_cast<size_t>(channel)]);
    for (int mix = 0; mix < kMixes; ++mix)
        outputPeaks_[static_cast<size_t>(mix)].store(mixPeaks[static_cast<size_t>(mix)]);
}


bool AudioEngine::tryAcquireNetworkBlock(const float*& samples, int& frames, unsigned long long& sequence) noexcept {
    BlockView view{};
    if (!tryAcquire(view)) return false;
    samples = view.samples;
    frames = view.frames;
    sequence = view.sequence;
    return true;
}
bool AudioEngine::tryAcquire(BlockView& view) noexcept {
    NetworkQueue::BlockView queued{};
    if (!networkQueue_.tryAcquire(queued)) return false;
    view = {queued.samples, queued.frames, queued.sequence, queued.timestampFrames};
    return true;
}
void AudioEngine::releaseNetworkBlock() noexcept {
    networkQueue_.release();
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
void AudioEngine::setMixInputSolo(int mix, int input, bool solo) noexcept {
    if (mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels)
        inputSolos_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(solo);
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
bool AudioEngine::mixInputSolo(int mix, int input) const noexcept {
    return mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels
        ? inputSolos_[static_cast<size_t>(mix)][static_cast<size_t>(input)].load() : false;
}


#include "AudioEngine.h"
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
    const auto error = deviceManager_.initialise(kInputChannels, kOutputChannels, nullptr, true);
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
    for (int out = 0; out < numOutputs; ++out)
        if (outputs[out] != nullptr) juce::FloatVectorOperations::clear(outputs[out], samples);

    std::array<float, kInputChannels> peaks{};
    std::array<float, kMixes> mixPeaks{};

    const int availableInputs = std::min(numInputs, kInputChannels);
    const int availableOutputs = std::min(numOutputs, kOutputChannels);

    for (int channel = 0; channel < availableInputs; ++channel) {
        if (inputs[channel] == nullptr) continue;
        const auto range = juce::FloatVectorOperations::findMinAndMax(inputs[channel], samples);
        peaks[static_cast<size_t>(channel)] = juce::jmax(std::abs(range.getStart()), std::abs(range.getEnd()));
    }

    // Cache per-channel controls once per block. This keeps trigonometric work
    // and atomic reads out of the sample loop, reducing real-time CPU pressure.
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

    // Every musician mix has its own gain, pan and mute state for each input.
    // The callback performs no locks or memory allocation.
    for (int mix = 0; mix < kMixes; ++mix) {
        const int left = mix * 2;
        const int right = left + 1;
        if (left >= availableOutputs) break;
        float* outL = outputs[left];
        float* outR = right < availableOutputs ? outputs[right] : nullptr;
        if (outL == nullptr) continue;

        float peak = 0.0f;
        const auto m = static_cast<size_t>(mix);
        for (int sample = 0; sample < samples; ++sample) {
            float l = 0.0f, r = 0.0f;
            for (int channel = 0; channel < availableInputs; ++channel) {
                const auto ch = static_cast<size_t>(channel);
                if (!enabled[m][ch]) continue;
                const float input = inputs[channel][sample];
                l += input * gainLeft[m][ch];
                r += input * gainRight[m][ch];
            }
            l = juce::jlimit(-0.98f, 0.98f, l);
            r = juce::jlimit(-0.98f, 0.98f, r);
            outL[sample] = l;
            if (outR != nullptr) outR[sample] = r;
            peak = juce::jmax(peak, juce::jmax(std::abs(l), std::abs(r)));
        }
        mixPeaks[m] = peak;
    }

    for (int channel = 0; channel < kInputChannels; ++channel)
        inputPeaks_[static_cast<size_t>(channel)].store(peaks[static_cast<size_t>(channel)]);
    for (int mix = 0; mix < kMixes; ++mix)
        outputPeaks_[static_cast<size_t>(mix)].store(mixPeaks[static_cast<size_t>(mix)]);
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
        inputGains_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(juce::jlimit(0.0f, 2.0f, gain));
}
void AudioEngine::setMixInputPan(int mix, int input, float pan) noexcept {
    if (mix >= 0 && mix < kMixes && input >= 0 && input < kInputChannels)
        inputPans_[static_cast<size_t>(mix)][static_cast<size_t>(input)].store(juce::jlimit(-1.0f, 1.0f, pan));
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

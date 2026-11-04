#include "AudioEngine.h"
#include <algorithm>
#include <cmath>

AudioEngine::AudioEngine() {
    masterGains_.fill(0.65f);
    for (auto& peak : inputPeaks_) peak.store(0.0f);
    for (auto& peak : outputPeaks_) peak.store(0.0f);
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

void AudioEngine::audioDeviceIOCallback(const float** inputs, int numInputs,
                                        float** outputs, int numOutputs, int samples) {
    for (int out = 0; out < numOutputs; ++out)
        if (outputs[out] != nullptr) juce::FloatVectorOperations::clear(outputs[out], samples);

    std::array<float, kInputChannels> peaks{};
    std::array<float, kMixes> mixPeaks{};

    const int availableInputs = std::min(numInputs, kInputChannels);
    const int availableOutputs = std::min(numOutputs, kOutputChannels);

    for (int channel = 0; channel < availableInputs; ++channel) {
        if (inputs[channel] == nullptr) continue;
        peaks[static_cast<size_t>(channel)] = juce::FloatVectorOperations::findMinAndMax(inputs[channel], samples).getMagnitude();
    }

    // Each stereo mix is rendered independently. Mix N maps to output channels 2N and 2N+1.
    // With a stereo-only device, only mix 1 can be physically emitted; all other mixes remain
    // available when a multichannel output device is selected.
    for (int mix = 0; mix < kMixes; ++mix) {
        const int left = mix * 2;
        const int right = left + 1;
        if (left >= availableOutputs) break;
        float* outL = outputs[left];
        float* outR = right < availableOutputs ? outputs[right] : nullptr;
        if (outL == nullptr) continue;

        float peak = 0.0f;
        for (int sample = 0; sample < samples; ++sample) {
            float l = 0.0f, r = 0.0f;
            for (int channel = 0; channel < availableInputs; ++channel) {
                const float* source = inputs[channel];
                if (source == nullptr) continue;
                const float value = source[sample] * 0.125f;
                l += value * 0.70710678f;
                r += value * 0.70710678f;
            }
            l *= masterGains_[static_cast<size_t>(mix)];
            r *= masterGains_[static_cast<size_t>(mix)];
            l = juce::jlimit(-0.98f, 0.98f, l);
            r = juce::jlimit(-0.98f, 0.98f, r);
            outL[sample] += l;
            if (outR != nullptr) outR[sample] += r;
            peak = juce::jmax(peak, juce::jmax(std::abs(l), std::abs(r)));
        }
        mixPeaks[static_cast<size_t>(mix)] = peak;
    }

    for (int i = 0; i < kInputChannels; ++i) inputPeaks_[static_cast<size_t>(i)].store(peaks[static_cast<size_t>(i)]);
    for (int i = 0; i < kMixes; ++i) outputPeaks_[static_cast<size_t>(i)].store(mixPeaks[static_cast<size_t>(i)]);
}

float AudioEngine::inputPeak(int channel) const noexcept {
    return channel >= 0 && channel < kInputChannels ? inputPeaks_[static_cast<size_t>(channel)].load() : 0.0f;
}

float AudioEngine::outputPeak(int mix) const noexcept {
    return mix >= 0 && mix < kMixes ? outputPeaks_[static_cast<size_t>(mix)].load() : 0.0f;
}

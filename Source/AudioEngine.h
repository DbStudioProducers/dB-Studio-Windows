#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>

class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    static constexpr int kInputChannels = 32;
    static constexpr int kMixes = 16;
    static constexpr int kOutputChannels = 32;

    AudioEngine();
    ~AudioEngine() override;

    juce::String initialise();
    void shutdown();
    juce::AudioDeviceManager& deviceManager() noexcept { return deviceManager_; }
    const juce::AudioDeviceManager& deviceManager() const noexcept { return deviceManager_; }

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData, int numInputChannels,
                               float* const* outputChannelData, int numOutputChannels,
                               int numSamples, const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    float inputPeak(int channel) const noexcept;
    float outputPeak(int mix) const noexcept;
    double sampleRate() const noexcept { return sampleRate_.load(); }
    int blockSize() const noexcept { return blockSize_.load(); }

private:
    juce::AudioDeviceManager deviceManager_;
    std::array<std::atomic<float>, kInputChannels> inputPeaks_{};
    std::array<std::atomic<float>, kMixes> outputPeaks_{};
    std::atomic<double> sampleRate_{0.0};
    std::atomic<int> blockSize_{0};
    std::array<float, kMixes> masterGains_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>

class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    static constexpr int kInputChannels = 32;
    static constexpr int kMixes = 16;
    static constexpr int kOutputChannels = 32; // Logical AUX channels: 16 stereo buses.
    static constexpr int kMaxBlockSize = 8192;

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
    void setMixInputGain(int mix, int input, float gain) noexcept;
    void setMixInputPan(int mix, int input, float pan) noexcept;
    void setMixInputMuted(int mix, int input, bool muted) noexcept;
    float mixInputGain(int mix, int input) const noexcept;
    float mixInputPan(int mix, int input) const noexcept;
    bool mixInputMuted(int mix, int input) const noexcept;
    double sampleRate() const noexcept { return sampleRate_.load(); }
    int blockSize() const noexcept { return blockSize_.load(); }
    bool tryAcquireNetworkBlock(const float*& samples, int& frames, unsigned long long& sequence) noexcept;
    void releaseNetworkBlock() noexcept;
    unsigned long long droppedNetworkBlocks() const noexcept { return droppedNetworkBlocks_.load(); }

private:
    juce::AudioDeviceManager deviceManager_;
    std::array<std::atomic<float>, kInputChannels> inputPeaks_{};
    std::array<std::atomic<float>, kMixes> outputPeaks_{};
    std::atomic<double> sampleRate_{0.0};
    std::atomic<int> blockSize_{0};
    std::array<std::array<std::atomic<float>, kInputChannels>, kMixes> inputGains_{};
    std::array<std::array<std::atomic<float>, kInputChannels>, kMixes> inputPans_{};
    std::array<std::array<std::atomic<bool>, kInputChannels>, kMixes> inputMutes_{};

    // Preallocated internal AUX buses. Each mix owns L/R buffers independent
    // of the computer's physical output count. The network sender will consume
    // these through a bounded real-time-safe queue in the next networking step.
    std::array<std::array<float, kMaxBlockSize>, kOutputChannels> mixBusBuffers_{};
    static constexpr unsigned int kQueueSlots = 3;
    struct NetworkBlock { std::array<float, kOutputChannels * kMaxBlockSize> samples{}; int frames = 0; unsigned long long sequence = 0; };
    std::array<NetworkBlock, kQueueSlots> networkQueue_{};
    alignas(64) std::atomic<unsigned int> queueWrite_{0};
    alignas(64) std::atomic<unsigned int> queueRead_{0};
    std::atomic<unsigned long long> nextSequence_{0};
    std::atomic<unsigned long long> droppedNetworkBlocks_{0};
    bool networkBlockAcquired_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};

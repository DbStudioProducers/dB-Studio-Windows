#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <atomic>
#include "MixRenderer.h"
#include "SpscAudioBlockQueue.h"

class AudioEngine final : public juce::AudioIODeviceCallback {
public:
    static constexpr int kInputChannels = DbStudioMix::kInputs;
    static constexpr int kMixes = DbStudioMix::kMixes;
    static constexpr int kOutputChannels = DbStudioMix::kBuses; // Logical AUX channels: 16 stereo buses.
    static constexpr int kMaxBlockSize = DbStudioMix::kMaxFrames;
    static constexpr std::size_t kChannels = static_cast<std::size_t>(kOutputChannels);
    static constexpr std::size_t kMaxFrames = static_cast<std::size_t>(kMaxBlockSize);
    struct BlockView {
        const float* samples = nullptr;
        int frames = 0;
        unsigned long long sequence = 0;
        std::uint64_t timestampFrames = 0;
    };

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
    void setMixInputSolo(int mix, int input, bool solo) noexcept;
    float mixInputGain(int mix, int input) const noexcept;
    float mixInputPan(int mix, int input) const noexcept;
    bool mixInputMuted(int mix, int input) const noexcept;
    bool mixInputSolo(int mix, int input) const noexcept;
    double sampleRate() const noexcept { return sampleRate_.load(); }
    int blockSize() const noexcept { return blockSize_.load(); }
    bool tryAcquireNetworkBlock(const float*& samples, int& frames, unsigned long long& sequence) noexcept;
    void releaseNetworkBlock() noexcept;
    bool tryAcquire(BlockView& view) noexcept;
    void release() noexcept { releaseNetworkBlock(); }
    unsigned long long droppedNetworkBlocks() const noexcept { return networkQueue_.droppedBlocks(); }

private:
    juce::AudioDeviceManager deviceManager_;
    std::array<std::atomic<float>, kInputChannels> inputPeaks_{};
    std::array<std::atomic<float>, kMixes> outputPeaks_{};
    std::atomic<double> sampleRate_{0.0};
    std::atomic<int> blockSize_{0};
    // Written only by the audio callback; advances even when the bounded
    // network queue drops a block so receivers can observe audio-time gaps.
    std::uint64_t audioFrameCursor_ = 0;
    std::array<std::array<std::atomic<float>, kInputChannels>, kMixes> inputGains_{};
    std::array<std::array<std::atomic<float>, kInputChannels>, kMixes> inputPans_{};
    std::array<std::array<std::atomic<bool>, kInputChannels>, kMixes> inputMutes_{};
    std::array<std::array<std::atomic<bool>, kInputChannels>, kMixes> inputSolos_{};

    // Preallocated internal AUX buses. Each mix owns L/R buffers independent
    // of the computer's physical output count. The network sender will consume
    // these through a bounded real-time-safe queue in the next networking step.
    DbStudioMix::BusBuffers mixBusBuffers_{};
    using NetworkQueue = DbStudioAudio::SpscAudioBlockQueue<kOutputChannels, kMaxBlockSize, 3>;
    NetworkQueue networkQueue_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};


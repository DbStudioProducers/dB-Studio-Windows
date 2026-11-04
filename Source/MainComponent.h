#pragma once
#include "AudioEngine.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>

class MainComponent final : public juce::Component, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshDeviceSummary();
    void loadSelectedMix();

    AudioEngine engine_;
    juce::Label title_, status_, deviceSummary_, footer_;
    juce::TextButton devicesButton_{"DISPOSITIVOS"};
    juce::ComboBox mixSelector_;
    juce::Label mixLabel_{"mixLabel", "MIX INDIVIDUAL:"};
    juce::TextEditor diagnostics_;
    juce::Component channelGrid_;
    std::array<juce::Label, AudioEngine::kInputChannels> channelLabels_;
    std::array<juce::Slider, AudioEngine::kInputChannels> gainSliders_;
    std::array<juce::Slider, AudioEngine::kInputChannels> panSliders_;
    std::array<juce::TextButton, AudioEngine::kInputChannels> muteButtons_;
    int selectedMix_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

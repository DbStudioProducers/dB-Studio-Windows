#pragma once
#include "AudioEngine.h"
#include "AuxAssignmentRegistry.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <array>

class LevelMeter final : public juce::Component {
public:
    void setLevel(float value) noexcept { level_ = juce::jlimit(0.0f, 1.0f, value); repaint(); }
    void paint(juce::Graphics& g) override;
private:
    float level_ = 0.0f;
};

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
    void promptAssignDevice();
    void promptRenameDevice();
    bool persistAssignments();

    AudioEngine engine_;
    juce::Label title_, status_, deviceSummary_, footer_;
    juce::TextButton devicesButton_{"ÁUDIO"};
    juce::TextButton assignButton_{"ATRIBUIR AUX"};
    juce::TextButton renameButton_{"RENOMEAR"};
    AuxAssignmentRegistry assignments_;
    juce::File assignmentFile_;
    juce::ComboBox mixSelector_;
    juce::Label mixLabel_{"mixLabel", "MIX INDIVIDUAL:"};
    juce::Label outputMeterLabel_{"outputMeterLabel", "SAÍDA:"};
    LevelMeter outputMeter_;
    juce::TextEditor diagnostics_;
    juce::Component channelGrid_;
    std::array<juce::Label, AudioEngine::kInputChannels> channelLabels_;
    std::array<LevelMeter, AudioEngine::kInputChannels> inputMeters_;
    std::array<juce::Slider, AudioEngine::kInputChannels> gainSliders_;
    std::array<juce::Slider, AudioEngine::kInputChannels> panSliders_;
    std::array<juce::TextButton, AudioEngine::kInputChannels> muteButtons_;
    std::array<juce::TextButton, AudioEngine::kInputChannels> soloButtons_;
    int selectedMix_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

#pragma once
#include "AudioEngine.h"
#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent final : public juce::Component, private juce::Timer {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshDeviceSummary();

    AudioEngine engine_;
    juce::TextButton devicesButton_{"CONFIGURAR DISPOSITIVOS"};
    juce::Label title_, status_, deviceSummary_, meterSummary_, note_;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector_;
    juce::TextEditor diagnostics_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

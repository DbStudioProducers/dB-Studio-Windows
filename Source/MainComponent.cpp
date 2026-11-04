#include "MainComponent.h"

namespace {
const juce::Colour background{0xff101215};
const juce::Colour panel{0xff191d21};
const juce::Colour green{0xff43e638};
const juce::Colour muted{0xff929ba3};
}

MainComponent::MainComponent() {
    setSize(1180, 720);
    title_.setText("dB STUDIO", juce::dontSendNotification);
    title_.setFont(juce::Font(27.0f, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title_);

    status_.setText("INICIALIZANDO MOTOR JUCE...", juce::dontSendNotification);
    status_.setColour(juce::Label::textColourId, green);
    addAndMakeVisible(status_);

    deviceSummary_.setColour(juce::Label::textColourId, juce::Colours::white);
    deviceSummary_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(deviceSummary_);

    meterSummary_.setColour(juce::Label::textColourId, green);
    meterSummary_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(meterSummary_);

    note_.setText("Base nativa inicial: detecção de dispositivos e callback de áudio. "
                  "O roteamento avançado, DSP por canal e rede serão integrados nas próximas etapas.",
                  juce::dontSendNotification);
    note_.setColour(juce::Label::textColourId, muted);
    note_.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(note_);

    devicesButton_.onClick = [this] {
        selector_ = std::make_unique<juce::AudioDeviceSelectorComponent>(
            engine_.deviceManager(), 0, AudioEngine::kInputChannels,
            0, AudioEngine::kOutputChannels, false, false, true, false);
        selector_->setSize(650, 500);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(selector_.release());
        options.dialogTitle = "Dispositivos de áudio";
        options.dialogBackgroundColour = panel;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();
    };
    addAndMakeVisible(devicesButton_);

    diagnostics_.setMultiLine(true);
    diagnostics_.setReadOnly(true);
    diagnostics_.setColour(juce::TextEditor::backgroundColourId, panel);
    diagnostics_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    diagnostics_.setText("Diagnóstico do dispositivo será atualizado automaticamente.");
    addAndMakeVisible(diagnostics_);

    const auto error = engine_.initialise();
    status_.setText(error.isEmpty() ? "MOTOR JUCE ATIVO" : "ÁUDIO INDISPONÍVEL: " + error,
                    juce::dontSendNotification);
    refreshDeviceSummary();
    startTimerHz(5);
}

MainComponent::~MainComponent() {
    stopTimer();
    engine_.shutdown();
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(background);
    g.setColour(panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(18).removeFromBottom(getHeight() - 110), 8.0f);
    g.setColour(green);
    g.fillRect(18, 92, getWidth() - 36, 2);
}

void MainComponent::resized() {
    auto area = getLocalBounds().reduced(28);
    auto header = area.removeFromTop(55);
    title_.setBounds(header.removeFromLeft(320));
    status_.setBounds(header);
    area.removeFromTop(28);
    devicesButton_.setBounds(area.removeFromTop(42).removeFromLeft(250));
    area.removeFromTop(18);
    deviceSummary_.setBounds(area.removeFromTop(90));
    area.removeFromTop(10);
    meterSummary_.setBounds(area.removeFromTop(160));
    area.removeFromTop(10);
    note_.setBounds(area.removeFromTop(55));
    area.removeFromTop(8);
    diagnostics_.setBounds(area);
}

void MainComponent::timerCallback() {
    refreshDeviceSummary();
}

void MainComponent::refreshDeviceSummary() {
    auto* device = engine_.deviceManager().getCurrentAudioDevice();
    if (device == nullptr) {
        deviceSummary_.setText("Nenhum dispositivo de áudio ativo. Conecte um equipamento ou selecione um dispositivo padrão.",
                               juce::dontSendNotification);
        diagnostics_.setText("Estado: sem dispositivo ativo\nO aplicativo permanece aberto para configuração.");
        return;
    }

    const auto inputs = device->getActiveInputChannels().countNumberOfSetBits();
    const auto outputs = device->getActiveOutputChannels().countNumberOfSetBits();
    deviceSummary_.setText("Dispositivo: " + device->getName()
        + "\nEntradas ativas: " + juce::String(inputs)
        + " | Saídas ativas: " + juce::String(outputs)
        + "\nTaxa: " + juce::String(engine_.sampleRate(), 0)
        + " Hz | Buffer: " + juce::String(engine_.blockSize()) + " samples",
        juce::dontSendNotification);

    juce::String meters = "PICOS DE ENTRADA (32 canais)\n";
    for (int i = 0; i < AudioEngine::kInputChannels; ++i) {
        meters += "IN " + juce::String(i + 1).paddedLeft('0', 2) + ": "
            + juce::String(engine_.inputPeak(i), 3) + ((i % 4 == 3) ? "\n" : "     ");
    }
    meters += "\nPICOS DE SAÍDA (16 mixes estéreo)\n";
    for (int i = 0; i < AudioEngine::kMixes; ++i)
        meters += "MIX " + juce::String(i + 1).paddedLeft('0', 2) + ": "
            + juce::String(engine_.outputPeak(i), 3) + ((i % 4 == 3) ? "\n" : "     ");
    meterSummary_.setText(meters, juce::dontSendNotification);
    diagnostics_.setText("Dispositivo atual: " + device->getName()
        + "\nTipo de driver: " + device->getTypeName()
        + "\nEntradas/saídas: " + juce::String(inputs) + "/" + juce::String(outputs)
        + "\nFormato de processamento: float32 interno"
        + "\nNota: mixes 2–16 exigem saídas físicas multicanais.");
}


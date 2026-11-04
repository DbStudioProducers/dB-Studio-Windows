#include "MainComponent.h"

namespace {
const juce::Colour background{0xff101215};
const juce::Colour panel{0xff191d21};
const juce::Colour green{0xff43e638};
const juce::Colour muted{0xff929ba3};
}

void LevelMeter::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff30363b));
    g.fillRoundedRectangle(bounds, 2.0f);
    auto active = bounds.reduced(1.0f);
    active.setWidth(active.getWidth() * level_);
    if (level_ > 0.90f) g.setColour(juce::Colour(0xffef4444));
    else if (level_ > 0.70f) g.setColour(juce::Colour(0xffffc247));
    else g.setColour(green);
    g.fillRoundedRectangle(active, 1.5f);
}

MainComponent::MainComponent() {
    assignmentFile_ = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("dB Studio").getChildFile("assignments.dbsaux");
    assignmentFile_.getParentDirectory().createDirectory();
    if (assignmentFile_.existsAsFile()) {
        const auto loaded = assignments_.loadFromFile(assignmentFile_.getFullPathName().toStdString());
        if (loaded != AuxAssignmentRegistry::Result::success)
            status_.setText("AVISO: cadastro AUX salvo inválido; não foi carregado.", juce::dontSendNotification);
    }
    setSize(1320, 850);
    title_.setText("dB STUDIO", juce::dontSendNotification);
    title_.setFont(juce::Font(27.0f, juce::Font::bold));
    title_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title_);

    status_.setText("INICIALIZANDO MOTOR JUCE...", juce::dontSendNotification);
    status_.setColour(juce::Label::textColourId, green);
    addAndMakeVisible(status_);

    mixLabel_.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(mixLabel_);
    for (int mix = 0; mix < AudioEngine::kMixes; ++mix)
        mixSelector_.addItem("MIX " + juce::String(mix + 1).paddedLeft('0', 2), mix + 1);
    mixSelector_.setSelectedId(1, juce::dontSendNotification);
    mixSelector_.onChange = [this] {
        selectedMix_ = mixSelector_.getSelectedId() - 1;
        loadSelectedMix();
    };
    addAndMakeVisible(mixSelector_);

    outputMeterLabel_.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(outputMeterLabel_);
    addAndMakeVisible(outputMeter_);

    devicesButton_.onClick = [this] {
        auto* selector = new juce::AudioDeviceSelectorComponent(
            engine_.deviceManager(), 0, AudioEngine::kInputChannels,
            0, 2, false, false, true, false);
        selector->setSize(680, 520);
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned(selector);
        options.dialogTitle = "Dispositivos de áudio";
        options.dialogBackgroundColour = panel;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();
    };
    addAndMakeVisible(devicesButton_);
    assignButton_.onClick = [this] { promptAssignDevice(); };
    addAndMakeVisible(assignButton_);
    renameButton_.onClick = [this] { promptRenameDevice(); };
    addAndMakeVisible(renameButton_);

    for (int i = 0; i < AudioEngine::kInputChannels; ++i) {
        auto& label = channelLabels_[static_cast<size_t>(i)];
        label.setText("IN " + juce::String(i + 1).paddedLeft('0', 2), juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, green);
        channelGrid_.addAndMakeVisible(label);
        channelGrid_.addAndMakeVisible(inputMeters_[static_cast<size_t>(i)]);

        auto& gain = gainSliders_[static_cast<size_t>(i)];
        gain.setSliderStyle(juce::Slider::LinearVertical);
        gain.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 18);
        gain.setRange(0.0, 2.0, 0.01);
        gain.setColour(juce::Slider::thumbColourId, green);
        gain.setColour(juce::Slider::trackColourId, juce::Colour(0xff3a4540));
        gain.onValueChange = [this, i] {
            engine_.setMixInputGain(selectedMix_, i, static_cast<float>(gainSliders_[static_cast<size_t>(i)].getValue()));
        };
        channelGrid_.addAndMakeVisible(gain);

        auto& pan = panSliders_[static_cast<size_t>(i)];
        pan.setSliderStyle(juce::Slider::LinearHorizontal);
        pan.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        pan.setRange(-1.0, 1.0, 0.01);
        pan.setColour(juce::Slider::thumbColourId, juce::Colours::white);
        pan.onValueChange = [this, i] {
            engine_.setMixInputPan(selectedMix_, i, static_cast<float>(panSliders_[static_cast<size_t>(i)].getValue()));
        };
        channelGrid_.addAndMakeVisible(pan);

        auto& mute = muteButtons_[static_cast<size_t>(i)];
        mute.setButtonText("MUTE");
        mute.setClickingTogglesState(true);
        mute.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a2525));
        mute.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc62828));
        mute.onClick = [this, i] {
            engine_.setMixInputMuted(selectedMix_, i, muteButtons_[static_cast<size_t>(i)].getToggleState());
        };
        channelGrid_.addAndMakeVisible(mute);
    }
    addAndMakeVisible(channelGrid_);

    deviceSummary_.setColour(juce::Label::textColourId, muted);
    deviceSummary_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(deviceSummary_);

    diagnostics_.setMultiLine(true);
    diagnostics_.setReadOnly(true);
    diagnostics_.setColour(juce::TextEditor::backgroundColourId, panel);
    diagnostics_.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    addAndMakeVisible(diagnostics_);

    footer_.setText("32 ENTRADAS  |  16 MIXES ESTÉREO  |  PROCESSAMENTO FLOAT32", juce::dontSendNotification);
    footer_.setColour(juce::Label::textColourId, muted);
    addAndMakeVisible(footer_);

    const auto error = engine_.initialise();
    status_.setText(error.isEmpty() ? "MOTOR JUCE ATIVO" : "ÁUDIO INDISPONÍVEL: " + error,
                    juce::dontSendNotification);
    loadSelectedMix();
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
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(16), 9.0f);
    g.setColour(green);
    g.fillRect(18, 86, getWidth() - 36, 2);
    g.setColour(juce::Colour(0xff30363b));
    for (int row = 0; row < 4; ++row)
        g.drawRoundedRectangle(28.0f, 150.0f + row * 145.0f, static_cast<float>(getWidth() - 56),
                               137.0f, 5.0f, 1.0f);
}

void MainComponent::resized() {
    auto area = getLocalBounds().reduced(28);
    auto header = area.removeFromTop(48);
    title_.setBounds(header.removeFromLeft(260));
    status_.setBounds(header.removeFromLeft(330));
    devicesButton_.setBounds(header.removeFromRight(170).reduced(2));
    area.removeFromTop(14);
    auto controls = area.removeFromTop(38);
    mixLabel_.setBounds(controls.removeFromLeft(130));
    mixSelector_.setBounds(controls.removeFromLeft(150));
    assignButton_.setBounds(controls.removeFromLeft(125).reduced(2));
    renameButton_.setBounds(controls.removeFromLeft(105).reduced(2));
    outputMeterLabel_.setBounds(controls.removeFromLeft(62));
    outputMeter_.setBounds(controls.removeFromLeft(110).reduced(2, 12));
    deviceSummary_.setBounds(controls);
    area.removeFromTop(12);
    auto gridArea = area.removeFromTop(590);
    const int cellW = (gridArea.getWidth() - 24) / 8;
    const int cellH = (gridArea.getHeight() - 18) / 4;
    for (int i = 0; i < AudioEngine::kInputChannels; ++i) {
        const int col = i % 8, row = i / 8;
        auto cell = juce::Rectangle<int>(gridArea.getX() + col * (cellW + 3),
                                         gridArea.getY() + row * (cellH + 4), cellW, cellH);
        channelLabels_[static_cast<size_t>(i)].setBounds(cell.removeFromTop(20));
        inputMeters_[static_cast<size_t>(i)].setBounds(cell.removeFromTop(8).reduced(5, 0));
        muteButtons_[static_cast<size_t>(i)].setBounds(cell.removeFromBottom(24).reduced(8, 1));
        panSliders_[static_cast<size_t>(i)].setBounds(cell.removeFromBottom(22).reduced(5, 0));
        gainSliders_[static_cast<size_t>(i)].setBounds(cell.reduced(12, 0));
    }
    area.removeFromTop(6);
    diagnostics_.setBounds(area.removeFromTop(55));
    footer_.setBounds(area);
}

void MainComponent::timerCallback() {
    for (int i = 0; i < AudioEngine::kInputChannels; ++i)
        inputMeters_[static_cast<size_t>(i)].setLevel(engine_.inputPeak(i));
    outputMeter_.setLevel(engine_.outputPeak(selectedMix_));
    refreshDeviceSummary();
}

void MainComponent::loadSelectedMix() {
    for (int i = 0; i < AudioEngine::kInputChannels; ++i) {
        const auto index = static_cast<size_t>(i);
        gainSliders_[index].setValue(engine_.mixInputGain(selectedMix_, i), juce::dontSendNotification);
        panSliders_[index].setValue(engine_.mixInputPan(selectedMix_, i), juce::dontSendNotification);
        muteButtons_[index].setToggleState(engine_.mixInputMuted(selectedMix_, i), juce::dontSendNotification);
    }
}


void MainComponent::promptAssignDevice() {
    auto* dialog = new juce::AlertWindow(
        "Atribuir dispositivo", "ID estável, nome e AUX exclusivo.", juce::AlertWindow::NoIcon);
    dialog->addTextEditor("deviceId", {}, "ID do dispositivo");
    dialog->addTextEditor("displayName", {}, "Nome do músico/dispositivo");
    dialog->addComboBox("mix", {}, "AUX");
    if (auto* combo = dialog->getComboBoxComponent("mix")) {
        for (int i = 0; i < AuxAssignmentRegistry::kMixCount; ++i)
            combo->addItem("AUX " + juce::String(i + 1).paddedLeft('0', 2), i + 1);
        combo->setSelectedId(1);
    }
    dialog->addButton("ATRIBUIR", 1);
    dialog->addButton("CANCELAR", 0);
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, safeDialog = juce::Component::SafePointer<juce::AlertWindow>(dialog)](int result) {
            if (result != 1 || safeDialog == nullptr) return;
            const auto id = safeDialog->getTextEditorContents("deviceId").trim();
            const auto name = safeDialog->getTextEditorContents("displayName").trim();
            const auto* combo = safeDialog->getComboBoxComponent("mix");
            const int mix = combo != nullptr ? combo->getSelectedId() - 1 : -1;
            if (mix < 0 || mix >= AuxAssignmentRegistry::kMixCount) return;
            const auto assignment = assignments_.assign(
                id.toStdString(), name.toStdString(), static_cast<std::size_t>(mix));
            const bool ok = assignment == AuxAssignmentRegistry::Result::success;
            const auto message = ok ? "AUX exclusivo atribuído." :
                assignment == AuxAssignmentRegistry::Result::deviceAlreadyAssigned ? "Dispositivo já possui um AUX." :
                assignment == AuxAssignmentRegistry::Result::mixAlreadyAssigned ? "AUX já ocupado." :
                assignment == AuxAssignmentRegistry::Result::invalidDeviceId ? "ID inválido." : "AUX inválido.";
            juce::AlertWindow::showMessageBoxAsync(
                ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                "dB Studio", message);
            refreshDeviceSummary();
        }), true);
}

void MainComponent::promptRenameDevice() {
    auto* dialog = new juce::AlertWindow(
        "Renomear dispositivo", "A identidade estável não será alterada.", juce::AlertWindow::NoIcon);
    dialog->addTextEditor("deviceId", {}, "ID estável");
    dialog->addTextEditor("displayName", {}, "Novo nome");
    dialog->addButton("SALVAR", 1);
    dialog->addButton("CANCELAR", 0);
    dialog->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, safeDialog = juce::Component::SafePointer<juce::AlertWindow>(dialog)](int result) {
            if (result != 1 || safeDialog == nullptr) return;
            const auto id = safeDialog->getTextEditorContents("deviceId").trim();
            const auto name = safeDialog->getTextEditorContents("displayName").trim();
            const auto renameResult = assignments_.renameDevice(id.toStdString(), name.toStdString());
            const bool ok = renameResult == AuxAssignmentRegistry::Result::success;
            juce::AlertWindow::showMessageBoxAsync(
                ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon,
                "dB Studio", ok ? "Nome atualizado." : "Dispositivo não encontrado.");
            refreshDeviceSummary();
        }), true);
}

void MainComponent::refreshDeviceSummary() {
    auto* device = engine_.deviceManager().getCurrentAudioDevice();
    const auto assignedText = " | " + juce::String(static_cast<int>(assignments_.assignedCount())) + "/16 AUX atribuídos";
    if (device == nullptr) {
        deviceSummary_.setText("SEM DISPOSITIVO DE ÁUDIO" + assignedText, juce::dontSendNotification);
        diagnostics_.setText("Cadastro local de AUX. Ainda não indica receptores conectados por Wi-Fi.");
        return;
    }
    const auto inputs = device->getActiveInputChannels().countNumberOfSetBits();
    const auto outputs = device->getActiveOutputChannels().countNumberOfSetBits();
    deviceSummary_.setText(device->getName() + "  |  " + juce::String(inputs) + " IN / "
        + juce::String(outputs) + " OUT  |  " + juce::String(engine_.sampleRate(), 0) + " Hz" + assignedText,
        juce::dontSendNotification);
    diagnostics_.setText("Driver: " + device->getTypeName() + " | Buffer: "
        + juce::String(engine_.blockSize()) + " samples | 16 AUX estéreo internos. Receptores Wi-Fi ainda não integrados. "
        "Saída física opcional para monitoração.");
}


bool MainComponent::persistAssignments() {
    return assignments_.saveToFile(assignmentFile_.getFullPathName().toStdString())
        == AuxAssignmentRegistry::Result::success;
}

#include "MainComponent.h"

namespace {
const juce::Colour background{0xff101215};
}

class DbStudioApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "dB Studio"; }
    const juce::String getApplicationVersion() override { return "0.5.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String&) override {
        mainWindow_ = std::make_unique<MainWindow>(getApplicationName());
    }
    void shutdown() override { mainWindow_.reset(); }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow {
    public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, background, allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setResizable(true, true);
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION(DbStudioApplication)

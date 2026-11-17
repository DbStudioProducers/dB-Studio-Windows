#include "MainComponent.h"
#include <memory>

namespace {
const juce::Colour background{0xff101215};
}

class DbStudioApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "dB Studio"; }
    const juce::String getApplicationVersion() override { return "0.5.1"; }
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
            setTitleBarButtonsRequired(allButtons, false);
            setResizable(true, true);
            setResizeLimits(960, 650, 8192, 8192);
            setContentOwned(new MainComponent(), true);
            const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            const auto workArea = display != nullptr ? display->userArea : juce::Rectangle<int>(0, 0, 1280, 800);
            const int initialWidth = juce::jmin(1500, juce::jmax(960, workArea.getWidth() - 80));
            const int initialHeight = juce::jmin(950, juce::jmax(650, workArea.getHeight() - 80));
            centreWithSize(initialWidth, initialHeight);
            setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<MainWindow> mainWindow_;
};

START_JUCE_APPLICATION(DbStudioApplication)

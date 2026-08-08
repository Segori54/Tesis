#include <JuceHeader.h>

#include "MainComponent.h"

#include <iostream>

class ConsoleLogger final : public juce::Logger
{
public:
    void logMessage(const juce::String& message) override
    {
        std::cout << message << std::endl;
    }
};

class RealtimeFeedbackEngineApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "RealtimeFeedbackEngine"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override
    {
        logger = std::make_unique<ConsoleLogger>();
        juce::Logger::setCurrentLogger(logger.get());
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::Logger::setCurrentLogger(nullptr);
        logger.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(std::move(name), juce::Colours::lightgrey, allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<ConsoleLogger> logger;
};

START_JUCE_APPLICATION(RealtimeFeedbackEngineApplication)

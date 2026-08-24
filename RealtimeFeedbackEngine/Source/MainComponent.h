#pragma once

#include <JuceHeader.h>

#include "AudioEngine.h"
#include "Processors/DelayProcessor.h"

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void timerCallback() override;
    void toggleAudio();
    void updateRunButton();
    void refreshDeviceChoices();

    juce::AudioDeviceManager deviceManager;
    DelayProcessor delayProcessor;
    AudioEngine audioEngine;
    juce::ComboBox deviceTypeBox;
    juce::ComboBox inputDeviceBox;
    juce::ComboBox outputDeviceBox;
    juce::StringArray deviceTypeNames;
    juce::String selectedDeviceType;
    juce::String selectedInputDevice;
    juce::String selectedOutputDevice;
    juce::Slider latencyKnob;
    juce::TextButton runButton { "Run" };
    bool audioIsRunning = false;
    bool automaticStartAttempted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

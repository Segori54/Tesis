#include "MainComponent.h"

#include "Processors/PassthroughProcessor.h"

MainComponent::MainComponent()
    : audioEngine(std::make_unique<PassthroughProcessor>())
{
    setSize(480, 240);

    audioEngine.initialise(deviceManager);
}

MainComponent::~MainComponent()
{
    audioEngine.shutdown();
    shutdownAudio();
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected, sampleRate);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
}

void MainComponent::releaseResources()
{
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colours::darkgrey);
    graphics.setColour(juce::Colours::white);
    graphics.setFont(18.0f);
    graphics.drawFittedText("RealtimeFeedbackEngine\nPassthrough", getLocalBounds(),
                            juce::Justification::centred, 2);
}

void MainComponent::resized()
{
}

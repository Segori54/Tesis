#pragma once

#include "Processors/IProcessor.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <memory>

class AudioEngine final : private juce::AudioIODeviceCallback
{
public:
    explicit AudioEngine(std::unique_ptr<IProcessor> processorToUse);
    ~AudioEngine() override;

    bool initialise(juce::AudioDeviceManager& deviceManager);
    void shutdown() noexcept;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int totalNumInputChannels,
                                          float* const* outputChannelData,
                                          int totalNumOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    std::unique_ptr<IProcessor> processor;
    juce::AudioDeviceManager* deviceManager = nullptr;
};

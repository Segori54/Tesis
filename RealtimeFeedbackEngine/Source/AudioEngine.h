#pragma once

#include "Processors/IProcessor.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <array>
#include <atomic>

class AudioEngine final : private juce::AudioIODeviceCallback
{
public:
    explicit AudioEngine(IProcessor& processorToUse) noexcept;
    ~AudioEngine() override;

    bool initialise(juce::AudioDeviceManager& deviceManager,
                    juce::String deviceType,
                    juce::String inputDeviceName,
                    juce::String outputDeviceName);
    void shutdown() noexcept;

    [[nodiscard]] juce::String getCurrentDeviceType() const;
    [[nodiscard]] juce::String getCurrentDeviceName() const;
    [[nodiscard]] double getCurrentSampleRate() const noexcept;
    [[nodiscard]] int getCurrentBlockSize() const noexcept;
    [[nodiscard]] int getInputLatencySamples() const noexcept;
    [[nodiscard]] int getOutputLatencySamples() const noexcept;
    [[nodiscard]] float getInputLevelDbfs(int channel) const noexcept;
    [[nodiscard]] float getOutputLevelDbfs(int channel) const noexcept;
    [[nodiscard]] juce::String getStatusMessage() const;

private:
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int totalNumInputChannels,
                                          float* const* outputChannelData,
                                          int totalNumOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    static void updateLevels(const float* const* channelData,
                             int numChannels,
                             int numSamples,
                             std::array<std::atomic<float>, 32>& levels) noexcept;

    IProcessor& processor;
    juce::AudioDeviceManager* deviceManager = nullptr;
    juce::String currentDeviceType { "No active device" };
    juce::String currentDeviceName { "No active device" };
    double currentSampleRate = 0.0;
    int currentBlockSize = 0;
    int inputLatencySamples = 0;
    int outputLatencySamples = 0;
    std::array<std::atomic<float>, 32> inputLevelsDbfs;
    std::array<std::atomic<float>, 32> outputLevelsDbfs;
    juce::String statusMessage { "Waiting to initialise audio device" };
};

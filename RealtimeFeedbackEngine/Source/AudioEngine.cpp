#include "AudioEngine.h"

#include "Config/AudioConfig.h"

#include <JuceHeader.h>

#include <iostream>
#include <utility>

AudioEngine::AudioEngine(std::unique_ptr<IProcessor> processorToUse)
    : processor(std::move(processorToUse))
{
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

bool AudioEngine::initialise(juce::AudioDeviceManager& manager)
{
    shutdown();

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    preferredSetup.sampleRate = audio_config::preferredSampleRate;
    preferredSetup.bufferSize = audio_config::preferredBlockSize;

    bool asioWasAttempted = false;
    for (auto* deviceType : manager.getAvailableDeviceTypes())
    {
        if (deviceType->getTypeName().equalsIgnoreCase("ASIO"))
        {
            asioWasAttempted = true;
            manager.setCurrentAudioDeviceType(deviceType->getTypeName(), true);
            break;
        }
    }

    auto error = manager.initialise(audio_config::inputChannels,
                                    audio_config::outputChannels,
                                    nullptr,
                                    true,
                                    {},
                                    &preferredSetup);

    if (error.isNotEmpty() && asioWasAttempted)
    {
        std::cout << "ASIO unavailable: " << error << "\n";
        manager.closeAudioDevice();
        manager.setCurrentAudioDeviceType({}, true);
        error = manager.initialise(audio_config::inputChannels,
                                   audio_config::outputChannels,
                                   nullptr,
                                   true,
                                   {},
                                   &preferredSetup);
    }

    if (error.isNotEmpty())
    {
        std::cerr << "Audio device initialisation failed: " << error << "\n";
        return false;
    }

    auto* device = manager.getCurrentAudioDevice();
    const auto actualSetup = manager.getAudioDeviceSetup();
    const auto printDeviceNames = [] (const char* label, const juce::StringArray& names)
    {
        std::cout << label << "\n";
        if (names.isEmpty())
        {
            std::cout << "    <none>\n";
            return;
        }

        for (int index = 0; index < names.size(); ++index)
            std::cout << "    " << names[index] << "\n";
    };

    std::cout << "Current audio device type: " << manager.getCurrentAudioDeviceType() << "\n";
    std::cout << "Current audio device name: "
              << (device != nullptr ? device->getName() : juce::String("<none>")) << "\n";
    std::cout << "Available device types:\n";

    for (auto* deviceType : manager.getAvailableDeviceTypes())
    {
        deviceType->scanForDevices();
        std::cout << "  " << deviceType->getTypeName() << "\n";
        printDeviceNames("    Available output devices:", deviceType->getDeviceNames(false));
        printDeviceNames("    Available input devices:", deviceType->getDeviceNames(true));
    }

    std::cout << "Selected input device: "
              << (actualSetup.inputDeviceName.isNotEmpty() ? actualSetup.inputDeviceName
                                                            : juce::String("<none>")) << "\n"
              << "Selected output device: "
              << (actualSetup.outputDeviceName.isNotEmpty() ? actualSetup.outputDeviceName
                                                             : juce::String("<none>")) << "\n"
              << "Current sample rate: " << actualSetup.sampleRate << " Hz\n"
              << "Current block size: " << actualSetup.bufferSize << " samples\n";

    if (device != nullptr)
    {
        std::cout << "Input latency: " << device->getInputLatencyInSamples() << " samples\n"
                  << "Output latency: " << device->getOutputLatencyInSamples() << " samples\n";
    }

    std::cout << "Audio device diagnostics complete. Starting audio callback.\n";

    manager.addAudioCallback(this);
    deviceManager = &manager;

    return device != nullptr;
}

void AudioEngine::shutdown() noexcept
{
    if (deviceManager == nullptr)
        return;

    deviceManager->removeAudioCallback(this);
    deviceManager->closeAudioDevice();
    deviceManager = nullptr;
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int totalNumInputChannels,
                                                   float* const* outputChannelData,
                                                   int totalNumOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused(context);

    // These AudioBuffer objects refer directly to device-owned callback memory.
    // The external-data constructors do not allocate; ownership remains with the device.
    juce::AudioBuffer<float> inputBuffer(const_cast<float* const*>(inputChannelData),
                                         totalNumInputChannels,
                                         numSamples);
    juce::AudioBuffer<float> outputBuffer(outputChannelData,
                                          totalNumOutputChannels,
                                          numSamples);

    processor->process(inputBuffer, outputBuffer);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
        processor->prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
}

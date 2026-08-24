#include "DelayProcessor.h"

#include "../Config/AudioConfig.h"

#include <algorithm>
#include <cmath>

void DelayProcessor::prepare(double sampleRate, int maximumBlockSize) noexcept
{
    currentSampleRate = sampleRate;

    const auto maximumDelaySamples = static_cast<int>(std::ceil(
        sampleRate * static_cast<double>(audio_config::maximumAddedLatencyMilliseconds) / 1000.0));

    delayBuffer.setSize(audio_config::outputChannels,
                        maximumDelaySamples + maximumBlockSize + 1,
                        false,
                        true,
                        false);
    delayBuffer.clear();
    writePosition = 0;
}

void DelayProcessor::process(const juce::AudioBuffer<float>& input,
                             juce::AudioBuffer<float>& output) noexcept
{
    const auto bufferLength = delayBuffer.getNumSamples();
    if (bufferLength == 0)
    {
        output.clear();
        return;
    }

    const auto requestedMilliseconds = addedLatencyMilliseconds.load(std::memory_order_relaxed);
    const auto maximumDelaySamples = bufferLength - 1;
    const auto delaySamples = std::clamp(
        static_cast<int>(std::lround(currentSampleRate * static_cast<double>(requestedMilliseconds) / 1000.0)),
        0,
        maximumDelaySamples);
    addedLatencySamples.store(delaySamples, std::memory_order_relaxed);

    const auto channelsToProcess = std::min(output.getNumChannels(), delayBuffer.getNumChannels());
    const auto inputChannels = input.getNumChannels();
    const auto numSamples = output.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto readPosition = (writePosition - delaySamples + bufferLength) % bufferLength;

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            const auto* inputSamples = channel < inputChannels ? input.getReadPointer(channel) : nullptr;
            auto* delaySamplesForChannel = delayBuffer.getWritePointer(channel);
            auto* outputSamples = output.getWritePointer(channel);
            const auto inputSample = inputSamples != nullptr ? inputSamples[sample] : 0.0f;

            delaySamplesForChannel[writePosition] = inputSample;
            outputSamples[sample] = delaySamplesForChannel[readPosition];
        }

        ++writePosition;
        if (writePosition == bufferLength)
            writePosition = 0;
    }

    for (int channel = channelsToProcess; channel < output.getNumChannels(); ++channel)
        output.clear(channel, 0, numSamples);
}

void DelayProcessor::setAddedLatencyMilliseconds(int milliseconds) noexcept
{
    addedLatencyMilliseconds.store(
        std::clamp(milliseconds, 0, audio_config::maximumAddedLatencyMilliseconds),
        std::memory_order_relaxed);
}

int DelayProcessor::getAddedLatencyMilliseconds() const noexcept
{
    return addedLatencyMilliseconds.load(std::memory_order_relaxed);
}

int DelayProcessor::getAddedLatencySamples() const noexcept
{
    return addedLatencySamples.load(std::memory_order_relaxed);
}

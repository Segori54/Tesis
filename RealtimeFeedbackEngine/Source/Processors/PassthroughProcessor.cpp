#include "PassthroughProcessor.h"

#include <algorithm>

void PassthroughProcessor::prepare(double, int) noexcept
{
}

void PassthroughProcessor::process(const juce::AudioBuffer<float>& input,
                                   juce::AudioBuffer<float>& output) noexcept
{
    const auto channelsToCopy = std::min(input.getNumChannels(), output.getNumChannels());
    const auto numSamples = output.getNumSamples();

    for (int channel = 0; channel < output.getNumChannels(); ++channel)
    {
        auto* outputSamples = output.getWritePointer(channel);
        if (outputSamples == nullptr)
            continue;

        const auto* inputSamples = channel < channelsToCopy ? input.getReadPointer(channel) : nullptr;

        if (inputSamples == nullptr)
        {
            std::fill_n(outputSamples, numSamples, 0.0f);
            continue;
        }

        if (inputSamples != outputSamples)
        {
            for (int sample = 0; sample < numSamples; ++sample)
                outputSamples[sample] = inputSamples[sample];
        }
    }
}

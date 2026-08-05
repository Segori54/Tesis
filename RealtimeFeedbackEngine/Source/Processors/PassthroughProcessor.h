#pragma once

#include "IProcessor.h"

class PassthroughProcessor final : public IProcessor
{
public:
    void prepare(double sampleRate, int maximumBlockSize) noexcept override;

    void process(const juce::AudioBuffer<float>& input,
                 juce::AudioBuffer<float>& output) noexcept override;
};

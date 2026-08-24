#pragma once

#include "IProcessor.h"

#include <atomic>

class DelayProcessor final : public IProcessor
{
public:
    void prepare(double sampleRate, int maximumBlockSize) noexcept override;

    void process(const juce::AudioBuffer<float>& input,
                 juce::AudioBuffer<float>& output) noexcept override;

    void setAddedLatencyMilliseconds(int milliseconds) noexcept;
    [[nodiscard]] int getAddedLatencyMilliseconds() const noexcept;
    [[nodiscard]] int getAddedLatencySamples() const noexcept;

private:
    std::atomic<int> addedLatencyMilliseconds { 0 };
    std::atomic<int> addedLatencySamples { 0 };

    juce::AudioBuffer<float> delayBuffer;
    double currentSampleRate = 0.0;
    int writePosition = 0;
};

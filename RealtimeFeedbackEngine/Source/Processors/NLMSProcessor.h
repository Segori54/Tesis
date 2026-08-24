#pragma once

#include "IProcessor.h"

#include <vector>

class NLMSProcessor final : public IProcessor
{
public:
    NLMSProcessor(int filterLength, float mu, float epsilon);

    void prepare(double sampleRate, int maximumBlockSize) noexcept override;

    void process(const juce::AudioBuffer<float>& input,
                 juce::AudioBuffer<float>& output) noexcept override;

    // One sample of the adaptive system-identification experiment.
    // Returns the instantaneous error e[n] = d[n] - y[n].
    float processSample(float inputSample, float desiredSample) noexcept;

    const std::vector<float>& getCoefficients() const noexcept { return coefficients; }
    float getMu() const noexcept { return stepSize; }
    float getEpsilon() const noexcept { return regularisation; }

private:
    float filterSample(float inputSample) noexcept;
    void resetState() noexcept;

    std::vector<float> coefficients;
    std::vector<float> inputHistory;
    float stepSize = 0.0f;
    float regularisation = 0.0f;
};

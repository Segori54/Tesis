#include "NLMSProcessor.h"

#include <algorithm>
#include <cmath>

NLMSProcessor::NLMSProcessor(int filterLength, float mu, float epsilon)
    : coefficients(static_cast<std::size_t>(std::max(1, filterLength)), 0.0f),
      inputHistory(coefficients.size(), 0.0f),
      stepSize(mu),
      regularisation(epsilon)
{
}

void NLMSProcessor::prepare(double, int) noexcept
{
    resetState();
    std::fill(coefficients.begin(), coefficients.end(), 0.0f);
}

void NLMSProcessor::process(const juce::AudioBuffer<float>& input,
                            juce::AudioBuffer<float>& output) noexcept
{
    // NLMS is intentionally not connected to the live microphone path yet.
    // This method only evaluates the current FIR estimate without adaptation.
    const auto samples = std::min(input.getNumSamples(), output.getNumSamples());
    if (input.getNumChannels() > 0 && output.getNumChannels() > 0)
    {
        const auto* inputSamples = input.getReadPointer(0);
        auto* outputSamples = output.getWritePointer(0);

        for (int sample = 0; sample < samples; ++sample)
            outputSamples[sample] = filterSample(inputSamples[sample]);
    }

    for (int channel = 0; channel < output.getNumChannels(); ++channel)
    {
        if (channel != 0 || input.getNumChannels() == 0)
            output.clear(channel, 0, output.getNumSamples());
        else if (samples < output.getNumSamples())
            output.clear(channel, samples, output.getNumSamples() - samples);
    }
}

float NLMSProcessor::processSample(float inputSample, float desiredSample) noexcept
{
    const auto estimate = filterSample(inputSample);
    const auto error = desiredSample - estimate;

    float inputPower = regularisation;
    for (const auto sample : inputHistory)
        inputPower += sample * sample;

    const auto normalisedStep = stepSize * error / inputPower;
    for (std::size_t tap = 0; tap < coefficients.size(); ++tap)
        coefficients[tap] += normalisedStep * inputHistory[tap];

    return error;
}

float NLMSProcessor::filterSample(float inputSample) noexcept
{
    for (std::size_t tap = inputHistory.size(); tap > 1; --tap)
        inputHistory[tap - 1] = inputHistory[tap - 2];

    inputHistory[0] = inputSample;

    float estimate = 0.0f;
    for (std::size_t tap = 0; tap < coefficients.size(); ++tap)
        estimate += coefficients[tap] * inputHistory[tap];

    return estimate;
}

void NLMSProcessor::resetState() noexcept
{
    std::fill(inputHistory.begin(), inputHistory.end(), 0.0f);
}

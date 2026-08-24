#include "../Source/Processors/NLMSProcessor.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <utility>
#include <vector>

namespace
{
class DeterministicWhiteNoise
{
public:
    float next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return static_cast<float>(static_cast<double>(state) / 2147483648.0);
    }

private:
    std::uint32_t state = 0x13579bdfu;
};

class DeterministicGaussianNoise
{
public:
    float next() noexcept
    {
        constexpr double twoPi = 6.28318530717958647692;
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        const auto first = (static_cast<double>(state) + 1.0) / 4294967297.0;

        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        const auto second = (static_cast<double>(state) + 1.0) / 4294967297.0;

        return static_cast<float>(std::sqrt(-2.0 * std::log(first)) * std::cos(twoPi * second));
    }

private:
    std::uint32_t state = 0x2468ace1u;
};

constexpr int trueSystemLength = 64;
constexpr int adaptiveFilterLengths[] = { 8, 16, 32, 48, 64, 96, 128, 256, 512 };
constexpr std::size_t sampleCount = 100000;
constexpr std::size_t evaluationStart = 80000;
constexpr std::size_t benchmarkSamples = 100000;
constexpr float mu = 0.10f;
constexpr float epsilon = 1.0e-6f;
constexpr float snrDb = 30.0f;
constexpr double convergenceLimit = 1.0e-3;

constexpr std::array<float, trueSystemLength> makeTrueSystem()
{
    std::array<float, trueSystemLength> system {};
    system[0] = 0.80f;
    system[1] = -0.40f;
    system[2] = 0.25f;
    system[3] = -0.10f;
    system[4] = 0.05f;
    system[5] = 0.02f;

    // The same fixed decaying tail is the physical 64-tap response for every M.
    for (int tap = 6; tap < trueSystemLength; ++tap)
    {
        const auto sign = (tap % 2 == 0) ? 1.0f : -1.0f;
        system[static_cast<std::size_t>(tap)] = sign * 0.02f / static_cast<float>(tap - 4);
    }

    return system;
}

constexpr auto trueSystem = makeTrueSystem();

struct IdentificationResult
{
    int filterLength = 0;
    const char* condition = "";
    std::size_t convergenceSample = 0;
    bool converged = false;
    bool unstable = false;
    double mse = 0.0;
    double coefficientError = 0.0;
    double modeledError = 0.0;
    double extraCoefficientRms = 0.0;
    bool hasExtraCoefficients = false;
};

struct BenchmarkResult
{
    int filterLength = 0;
    double totalTimeUs = 0.0;
    double microsecondsPerSample = 0.0;
    double relativeCost = 0.0;
};

double totalCoefficientError(const std::vector<float>& estimated,
                             int filterLength)
{
    double errorSquared = 0.0;
    for (int tap = 0; tap < filterLength; ++tap)
    {
        const auto trueCoefficient = tap < trueSystemLength
                                   ? trueSystem[static_cast<std::size_t>(tap)]
                                   : 0.0f;
        const auto difference = static_cast<double>(estimated[static_cast<std::size_t>(tap)])
                              - trueCoefficient;
        errorSquared += difference * difference;
    }

    return std::sqrt(errorSquared);
}

double modeledCoefficientError(const std::vector<float>& estimated,
                               int filterLength)
{
    double errorSquared = 0.0;
    const auto modeledLength = std::min(filterLength, trueSystemLength);
    for (int tap = 0; tap < modeledLength; ++tap)
    {
        const auto difference = static_cast<double>(estimated[static_cast<std::size_t>(tap)])
                              - trueSystem[static_cast<std::size_t>(tap)];
        errorSquared += difference * difference;
    }

    return std::sqrt(errorSquared);
}

double extraCoefficientRms(const std::vector<float>& estimated,
                           int filterLength)
{
    if (filterLength <= trueSystemLength)
        return 0.0;

    double energy = 0.0;
    for (int tap = trueSystemLength; tap < filterLength; ++tap)
    {
        const auto coefficient = static_cast<double>(estimated[static_cast<std::size_t>(tap)]);
        energy += coefficient * coefficient;
    }

    return std::sqrt(energy / static_cast<double>(filterLength - trueSystemLength));
}

bool isFinite(const std::vector<float>& values)
{
    for (const auto value : values)
        if (! std::isfinite(value))
            return false;

    return true;
}
}

int main()
{
    std::vector<float> input(sampleCount, 0.0f);
    std::vector<float> gaussianNoise(sampleCount, 0.0f);
    DeterministicWhiteNoise inputGenerator;
    DeterministicGaussianNoise noiseGenerator;

    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        input[sample] = inputGenerator.next();
        gaussianNoise[sample] = noiseGenerator.next();
    }

    std::vector<float> cleanDesired(sampleCount, 0.0f);
    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        for (int tap = 0; tap < trueSystemLength; ++tap)
            if (sample >= static_cast<std::size_t>(tap))
                cleanDesired[sample] += trueSystem[static_cast<std::size_t>(tap)]
                                      * input[sample - static_cast<std::size_t>(tap)];
    }

    double cleanPower = 0.0;
    double unitNoisePower = 0.0;
    for (std::size_t sample = 0; sample < sampleCount; ++sample)
    {
        cleanPower += static_cast<double>(cleanDesired[sample]) * cleanDesired[sample];
        unitNoisePower += static_cast<double>(gaussianNoise[sample]) * gaussianNoise[sample];
    }

    const auto noiseScale = std::sqrt(cleanPower
                                      / (std::pow(10.0, snrDb / 10.0) * unitNoisePower));
    std::vector<float> noisyDesired(sampleCount, 0.0f);
    for (std::size_t sample = 0; sample < sampleCount; ++sample)
        noisyDesired[sample] = cleanDesired[sample]
                             + static_cast<float>(noiseScale) * gaussianNoise[sample];

    std::ofstream identificationCsv("NLMSFinalLengthTest.csv", std::ios::trunc);
    std::ofstream benchmarkCsv("NLMSFinalLengthBenchmark.csv", std::ios::trunc);
    if (! identificationCsv || ! benchmarkCsv)
    {
        std::cerr << "Could not create NLMS CSV output files\n";
        return 1;
    }

    identificationCsv << "M,condition,convergence_sample,mse,coefficient_error,"
                          "modeled_error,extra_coeff_rms\n";
    benchmarkCsv << "M,benchmark_samples,total_time_us,us_per_sample,relative_cost\n";
    identificationCsv << std::setprecision(10);
    benchmarkCsv << std::setprecision(10);

    std::vector<IdentificationResult> identificationResults;
    std::vector<BenchmarkResult> benchmarkResults;
    identificationResults.reserve((sizeof(adaptiveFilterLengths)
                                   / sizeof(adaptiveFilterLengths[0])) * 2);
    benchmarkResults.reserve(sizeof(adaptiveFilterLengths)
                            / sizeof(adaptiveFilterLengths[0]));

    for (const auto filterLength : adaptiveFilterLengths)
    {
        const std::array<const char*, 2> conditions = { "CLEAN", "NOISY" };
        for (const auto condition : conditions)
        {
            const auto& desired = condition[0] == 'C' ? cleanDesired : noisyDesired;
            NLMSProcessor nlms(filterLength, mu, epsilon);
            double evaluationSquaredError = 0.0;
            IdentificationResult result;
            result.filterLength = filterLength;
            result.condition = condition;
            result.hasExtraCoefficients = filterLength > trueSystemLength;

            for (std::size_t sample = 0; sample < sampleCount; ++sample)
            {
                const auto error = nlms.processSample(input[sample], desired[sample]);
                if (sample >= evaluationStart)
                    evaluationSquaredError += static_cast<double>(error) * error;

                const auto currentCoefficientError = totalCoefficientError(nlms.getCoefficients(), filterLength);
                if (! result.converged && currentCoefficientError < convergenceLimit)
                {
                    result.converged = true;
                    result.convergenceSample = sample + 1;
                }

                if (! isFinite(nlms.getCoefficients()) || ! std::isfinite(error))
                {
                    result.unstable = true;
                    break;
                }

            }

            result.mse = evaluationSquaredError
                       / static_cast<double>(sampleCount - evaluationStart);
            result.coefficientError = totalCoefficientError(nlms.getCoefficients(), filterLength);
            result.modeledError = modeledCoefficientError(nlms.getCoefficients(), filterLength);
            result.extraCoefficientRms = extraCoefficientRms(nlms.getCoefficients(), filterLength);
            identificationResults.push_back(result);

            identificationCsv << filterLength << ',' << condition << ',';
            if (result.converged)
                identificationCsv << result.convergenceSample;
            else
                identificationCsv << "not_converged";
            identificationCsv << ',' << result.mse
                              << ',' << result.coefficientError
                              << ',' << result.modeledError << ',';
            if (result.hasExtraCoefficients)
                identificationCsv << result.extraCoefficientRms;
            else
                identificationCsv << "N/A";
            identificationCsv << '\n';
        }

        NLMSProcessor benchmarkFilter(filterLength, mu, epsilon);
        float checksum = 0.0f;
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t sample = 0; sample < benchmarkSamples; ++sample)
            checksum += benchmarkFilter.processSample(input[sample % sampleCount], 0.0f);
        const auto stop = std::chrono::steady_clock::now();
        volatile float benchmarkSink = checksum;
        (void) benchmarkSink;

        BenchmarkResult benchmark;
        benchmark.filterLength = filterLength;
        benchmark.totalTimeUs = std::chrono::duration<double, std::micro>(stop - start).count();
        benchmark.microsecondsPerSample = benchmark.totalTimeUs / static_cast<double>(benchmarkSamples);
        benchmarkResults.push_back(benchmark);
    }

    const auto baselineMicrosecondsPerSample = benchmarkResults.front().microsecondsPerSample;
    for (auto& benchmark : benchmarkResults)
    {
        benchmark.relativeCost = benchmark.microsecondsPerSample / baselineMicrosecondsPerSample;
        benchmarkCsv << benchmark.filterLength << ',' << benchmarkSamples << ','
                     << benchmark.totalTimeUs << ',' << benchmark.microsecondsPerSample << ','
                     << benchmark.relativeCost << '\n';
    }

    std::cout << std::setprecision(10)
              << "NLMS final adaptive-filter-length characterization\n"
              << "true system length: " << trueSystemLength << '\n'
              << "mu: " << mu << '\n'
              << "epsilon: " << epsilon << '\n'
              << "SNR: " << snrDb << " dB\n"
              << "sampleCount: " << sampleCount << '\n'
              << "evaluationStart: " << evaluationStart << '\n'
              << "convergence criterion: total coefficient error < " << convergenceLimit << '\n'
              << "identification CSV: NLMSFinalLengthTest.csv\n"
              << "benchmark CSV: NLMSFinalLengthBenchmark.csv\n\n"
              << "M | condition | convergence_sample | MSE | coefficient_error | modeled_error | "
                 "extra_coeff_RMS | us_per_sample | relative_cost\n";

    for (const auto& result : identificationResults)
    {
        const auto& benchmark = benchmarkResults[static_cast<std::size_t>(
            std::distance(std::begin(adaptiveFilterLengths),
                          std::find(std::begin(adaptiveFilterLengths),
                                    std::end(adaptiveFilterLengths),
                                    result.filterLength)))];

        std::cout << result.filterLength << " | " << result.condition << " | ";
        if (result.converged)
            std::cout << result.convergenceSample;
        else
            std::cout << "not converged";

        std::cout << " | " << result.mse
                  << " | " << result.coefficientError
                  << " | " << result.modeledError << " | ";
        if (result.hasExtraCoefficients)
            std::cout << result.extraCoefficientRms;
        else
            std::cout << "N/A";
        std::cout << " | " << benchmark.microsecondsPerSample
                  << " | " << benchmark.relativeCost << '\n';
    }

    std::cout << "\nM | benchmark_samples | total_time_us | us_per_sample | relative_cost\n";
    for (const auto& benchmark : benchmarkResults)
        std::cout << benchmark.filterLength << " | " << benchmarkSamples
                  << " | " << benchmark.totalTimeUs
                  << " | " << benchmark.microsecondsPerSample
                  << " | " << benchmark.relativeCost << '\n';

    bool testPassed = true;
    for (const auto& result : identificationResults)
        testPassed = testPassed && ! result.unstable;

    if (! testPassed)
    {
        std::cerr << "NLMS final length characterization FAILED\n";
        return 1;
    }

    std::cout << "NLMS final length characterization PASSED\n";
    return 0;
}

#include "ThdAnalyzer.h"
#include "JuceHeader.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

ThdAnalyzer::ThdAnalyzer(const juce::File& outDir, int fftSize, double fundamentalFreq,
                         const std::vector<juce::String>& paramNames, const juce::String& signalType)
    : fftSize(fftSize), fundamentalFreq(fundamentalFreq), paramNames(paramNames), outputDir(outDir),
      signalType(signalType) {}

ThdAnalyzer::~ThdAnalyzer() {}

void ThdAnalyzer::applyHannWindow(std::vector<float>& buffer) {
    const int N = (int)buffer.size();
    for (int i = 0; i < N; ++i) {
        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)(N - 1)));
        buffer[i] *= window;
    }
}

double ThdAnalyzer::computeTHD(const std::vector<std::complex<float>>& fftResult, double sampleRate) {
    const double binHz = sampleRate / (double)fftSize;
    const int k0 = (int)std::round(fundamentalFreq / binHz);

    // k0 == 0: fundamental is below half a bin, so it can't be resolved (and would divide by zero below)
    if (k0 < 1 || k0 >= fftSize / 2)
        return 0.0;

    // Fundamental power
    float fundamentalMag = std::abs(fftResult[k0]);
    double P1 = (double)(fundamentalMag * fundamentalMag);

    if (P1 <= 0.0)
        return 0.0;

    // Harmonic powers
    double harmonicPowerSum = 0.0;
    const int maxHarmonic = std::min(10, (fftSize / 2) / k0);

    for (int h = 2; h <= maxHarmonic; ++h) {
        int kh = h * k0;
        if (kh >= fftSize / 2)
            break;

        float harmonicMag = std::abs(fftResult[kh]);
        double harmonicPower = (double)(harmonicMag * harmonicMag);
        harmonicPowerSum += harmonicPower;
    }

    // THD as ratio
    double thd = std::sqrt(harmonicPowerSum / P1);
    return thd;
}

void ThdAnalyzer::processFFTWindow(RunThdData& data, int64_t centreSample) {
    if ((int)data.buffer.size() < fftSize)
        return;

    // Apply window
    applyHannWindow(data.buffer);

    // Perform FFT
    juce::dsp::FFT fft((int)std::log2(fftSize));
    std::vector<std::complex<float>> timeData(fftSize);
    std::vector<std::complex<float>> fftResult(fftSize);

    // Copy to complex buffer
    for (int i = 0; i < fftSize; ++i) {
        timeData[i] = std::complex<float>(data.buffer[i], 0.0f);
    }

    // Perform FFT (out-of-place: JUCE's fallback FFT engine does not support input == output)
    fft.perform(timeData.data(), fftResult.data(), false);

    // Compute THD
    double thd = computeTHD(fftResult, data.sampleRate);
    data.thdResults.push_back({centreSample, thd});

    // Clear buffer for next window
    data.buffer.clear();
}

void ThdAnalyzer::processBlock(const BlockContext& ctx) {
    auto& data = perRunData[ctx.runId];

    // Initialize on first block
    if (data.buffer.empty()) {
        data.paramValues = ctx.paramNamedValues;
        data.inputGainDb = ctx.inputGainDb;
        data.sampleRate = ctx.sampleRate;
    }

    // Accumulate samples
    for (int i = 0; i < ctx.numSamples; ++i) {
        data.buffer.push_back(ctx.outL[i]);

        // Process FFT window when we have enough samples
        if ((int)data.buffer.size() >= fftSize) {
            int64_t centreSample = ctx.firstSample + i - fftSize / 2;
            processFFTWindow(data, centreSample);
        }
    }
}

void ThdAnalyzer::finish(const juce::File& outDir) {
    juce::String filename = "grid_thd_" + signalType.toLowerCase() + ".csv";
    juce::File csvFile = outDir.getChildFile(filename);
    std::ofstream out(csvFile.getFullPathName().toStdString());

    if (!out.is_open()) {
        std::cerr << "Failed to open " << filename.toStdString() << " for writing" << std::endl;
        return;
    }

    // Header
    out << "runId,centreSample,thd";
    for (const auto& paramName : paramNames) {
        out << "," << paramName.toStdString();
    }
    out << ",inputGainDb\n";

    // Data rows
    for (const auto& [runId, data] : perRunData) {
        for (const auto& [centreSample, thd] : data.thdResults) {
            out << runId << "," << centreSample << "," << thd;

            // Parameter values
            for (const auto& paramName : paramNames) {
                float value = 0.0f;
                auto it = data.paramValues.find(paramName);
                if (it != data.paramValues.end())
                    value = it->second;
                out << "," << value;
            }

            out << "," << data.inputGainDb << "\n";
        }
    }
}

std::unique_ptr<Analyzer> createThdAnalyzer(const juce::File& outDir, int fftSize, double fundamentalFreq,
                                            const std::vector<juce::String>& paramNames,
                                            const juce::String& signalType) {
    return std::make_unique<ThdAnalyzer>(outDir, fftSize, fundamentalFreq, paramNames, signalType);
}

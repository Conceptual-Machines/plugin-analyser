#include "LinearResponseAnalyzer.h"
#include "JuceHeader.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

LinearResponseAnalyzer::LinearResponseAnalyzer(const juce::File& outDir, int fftSize,
                                               const std::vector<juce::String>& paramNames,
                                               const juce::String& signalType)
    : fftSize(fftSize), paramNames(paramNames), outputDir(outDir), signalType(signalType) {}

LinearResponseAnalyzer::~LinearResponseAnalyzer() {}

void LinearResponseAnalyzer::applyHannWindow(std::vector<float>& buffer) {
    const int N = (int)buffer.size();
    for (int i = 0; i < N; ++i) {
        float window = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)(N - 1)));
        buffer[i] *= window;
    }
}

void LinearResponseAnalyzer::processFFTWindow(RunSpectrum& spectrum) {
    if ((int)spectrum.inBuffer.size() < fftSize || (int)spectrum.outBuffer.size() < fftSize)
        return;

    // Apply window
    applyHannWindow(spectrum.inBuffer);
    applyHannWindow(spectrum.outBuffer);

    // Perform FFT
    juce::dsp::FFT fft((int)std::log2(fftSize));
    std::vector<std::complex<float>> inFFT(fftSize);
    std::vector<std::complex<float>> outFFT(fftSize);

    // Copy to complex buffers
    for (int i = 0; i < fftSize; ++i) {
        inFFT[i] = std::complex<float>(spectrum.inBuffer[i], 0.0f);
        outFFT[i] = std::complex<float>(spectrum.outBuffer[i], 0.0f);
    }

    // Perform FFT
    fft.perform(inFFT.data(), outFFT.data(), false);

    // Accumulate magnitude squared
    const int numBins = fftSize / 2;
    if ((int)spectrum.sumInMagSq.size() < numBins) {
        spectrum.sumInMagSq.resize(numBins, 0.0);
        spectrum.sumOutMagSq.resize(numBins, 0.0);
    }

    for (int k = 0; k < numBins; ++k) {
        float inMag = std::abs(inFFT[k]);
        float outMag = std::abs(outFFT[k]);
        spectrum.sumInMagSq[k] += (double)(inMag * inMag);
        spectrum.sumOutMagSq[k] += (double)(outMag * outMag);
    }

    spectrum.numAverages++;

    // Clear buffers for next window
    spectrum.inBuffer.clear();
    spectrum.outBuffer.clear();
}

void LinearResponseAnalyzer::processBlock(const BlockContext& ctx) {
    auto& spectrum = perRunSpectra[ctx.runId];

    // Initialize on first block
    if (spectrum.sumInMagSq.empty()) {
        spectrum.paramValues = ctx.paramNamedValues;
        spectrum.inputGainDb = ctx.inputGainDb;
        spectrum.sampleRate = ctx.sampleRate;
    }

    // Accumulate samples
    for (int i = 0; i < ctx.numSamples; ++i) {
        spectrum.inBuffer.push_back(ctx.inL[i]);
        spectrum.outBuffer.push_back(ctx.outL[i]);

        // Process FFT window when we have enough samples
        if ((int)spectrum.inBuffer.size() >= fftSize) {
            processFFTWindow(spectrum);
        }
    }
}

void LinearResponseAnalyzer::finish(const juce::File& outDir) {
    juce::String filename = "grid_linear_response_" + signalType.toLowerCase() + ".csv";
    juce::File csvFile = outDir.getChildFile(filename);
    std::ofstream out(csvFile.getFullPathName().toStdString());

    if (!out.is_open())
        throw std::runtime_error("Failed to open " + csvFile.getFullPathName().toStdString() + " for writing");

    // Header
    out << "runId,freqHz,magDb";
    for (const auto& paramName : paramNames) {
        out << "," << paramName.toStdString();
    }
    out << ",inputGainDb\n";

    // Data rows
    for (const auto& [runId, spectrum] : perRunSpectra) {
        if (spectrum.numAverages == 0)
            continue;

        const int numBins = fftSize / 2;
        const double binHz = spectrum.sampleRate / (double)fftSize;

        for (int k = 0; k < numBins; ++k) {
            double magIn = std::sqrt(spectrum.sumInMagSq[k] / spectrum.numAverages);
            double magOut = std::sqrt(spectrum.sumOutMagSq[k] / spectrum.numAverages);

            if (magIn <= 0.0)
                continue;

            double H = magOut / magIn;
            double magDb = 20.0 * std::log10(std::max(H, 1e-10));
            double freqHz = (double)k * binHz;

            out << runId << "," << freqHz << "," << magDb;

            // Parameter values
            for (const auto& paramName : paramNames) {
                float value = 0.0f;
                auto it = spectrum.paramValues.find(paramName);
                if (it != spectrum.paramValues.end())
                    value = it->second;
                out << "," << value;
            }

            out << "," << spectrum.inputGainDb << "\n";
        }
    }

    out.close();
    if (out.fail())
        throw std::runtime_error("Failed to write " + csvFile.getFullPathName().toStdString());
}

std::unique_ptr<Analyzer> createLinearResponseAnalyzer(const juce::File& outDir, int fftSize,
                                                       const std::vector<juce::String>& paramNames,
                                                       const juce::String& signalType) {
    return std::make_unique<LinearResponseAnalyzer>(outDir, fftSize, paramNames, signalType);
}

#include "LinearResponseAnalyzer.h"
#include "JuceHeader.h"
#include <cmath>
#include <fstream>
#include <iostream>

LinearResponseAnalyzer::LinearResponseAnalyzer(const juce::File& outDir, int fftSize,
                                               const std::vector<juce::String>& paramNames,
                                               const juce::String& signalType)
    : fftSize(fftSize), paramNames(paramNames), outputDir(outDir), signalType(signalType) {
    // Precompute periodic Hann window: dividing by fftSize (not fftSize - 1)
    // so overlap-and-add sums to exactly flat 1.0 at 50% overlap
    window.resize(fftSize);
    for (int i = 0; i < fftSize; ++i) {
        window[i] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * (float)i / (float)fftSize));
    }
}

LinearResponseAnalyzer::~LinearResponseAnalyzer() {}

void LinearResponseAnalyzer::processFFTWindow(RunSpectrum& spectrum) {
    if ((int)spectrum.inBuffer.size() < fftSize || (int)spectrum.outBuffer.size() < fftSize)
        return;

    // Perform FFT
    juce::dsp::FFT fft((int)std::log2(fftSize));
    std::vector<std::complex<float>> inTime(fftSize);
    std::vector<std::complex<float>> outTime(fftSize);
    std::vector<std::complex<float>> inFFT(fftSize);
    std::vector<std::complex<float>> outFFT(fftSize);

    // Copy to complex buffers with precomputed periodic Hann window (leaving raw buffers intact for overlap)
    for (int i = 0; i < fftSize; ++i) {
        float w = window[i];
        inTime[i] = std::complex<float>(spectrum.inBuffer[i] * w, 0.0f);
        outTime[i] = std::complex<float>(spectrum.outBuffer[i] * w, 0.0f);
    }

    // Transform input and output separately (FFT::perform is input -> output, out-of-place)
    fft.perform(inTime.data(), inFFT.data(), false);
    fft.perform(outTime.data(), outFFT.data(), false);

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

    // 50% overlap: advance by fftSize / 2, keeping the second half for the next window
    const int hopSize = fftSize / 2;
    spectrum.inBuffer.erase(spectrum.inBuffer.begin(), spectrum.inBuffer.begin() + hopSize);
    spectrum.outBuffer.erase(spectrum.outBuffer.begin(), spectrum.outBuffer.begin() + hopSize);
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

    if (!out.is_open()) {
        std::cerr << "Failed to open " << filename.toStdString() << " for writing" << std::endl;
        return;
    }

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
}

std::unique_ptr<Analyzer> createLinearResponseAnalyzer(const juce::File& outDir, int fftSize,
                                                       const std::vector<juce::String>& paramNames,
                                                       const juce::String& signalType) {
    return std::make_unique<LinearResponseAnalyzer>(outDir, fftSize, paramNames, signalType);
}

#pragma once

#include "Analyzer.h"
#include "JuceHeader.h"
#include <complex>
#include <map>
#include <vector>

struct LinearResponseAnalyzer : public Analyzer {
    LinearResponseAnalyzer(const juce::File& outDir, int fftSize, const std::vector<juce::String>& paramNames,
                           const juce::String& signalType);
    ~LinearResponseAnalyzer() override;

    void processBlock(const BlockContext& ctx) override;
    void finish(const juce::File& outDir) override;

private:
    struct RunSpectrum {
        std::vector<double> sumInMagSq;
        std::vector<double> sumOutMagSq;
        int numAverages = 0;
        std::vector<float> inBuffer;
        std::vector<float> outBuffer;
        std::map<juce::String, float> paramValues;
        float inputGainDb;
        double sampleRate = 48000.0;
    };

    std::map<int, RunSpectrum> perRunSpectra;
    int fftSize;
    std::vector<juce::String> paramNames;
    juce::File outputDir;
    juce::String signalType;
    std::vector<float> window;

    void processFFTWindow(RunSpectrum& spectrum);
};

std::unique_ptr<Analyzer> createLinearResponseAnalyzer(const juce::File& outDir, int fftSize,
                                                       const std::vector<juce::String>& paramNames,
                                                       const juce::String& signalType);

#pragma once

#include "Analyzer.h"
#include "JuceHeader.h"
#include <map>
#include <vector>

struct TransferCurveAnalyzer : public Analyzer {
    TransferCurveAnalyzer(const juce::File& outDir, int numBins, const std::vector<juce::String>& paramNames,
                          const juce::String& signalType);
    ~TransferCurveAnalyzer() override;

    void processBlock(const BlockContext& ctx) override;
    void finish(const juce::File& outDir) override;

private:
    struct BinData {
        double sumY = 0.0;
        int count = 0;
    };

    struct RunBinData {
        std::vector<BinData> bins;
        std::map<juce::String, float> paramValues;
        float inputGainDb;
        float maxInput = 1.0f;
    };

    std::map<int, RunBinData> perRunBins;
    int numBins;
    std::vector<juce::String> paramNames;
    juce::File outputDir;
    juce::String signalType;

    int getBinIndex(float x, float maxInput) const;
    float getBinCenter(int binIndex, float maxInput) const;
};

std::unique_ptr<Analyzer> createTransferCurveAnalyzer(const juce::File& outDir, int numBins,
                                                      const std::vector<juce::String>& paramNames,
                                                      const juce::String& signalType);

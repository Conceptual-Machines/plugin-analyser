#include "TransferCurveAnalyzer.h"
#include <algorithm>
#include <fstream>
#include <iostream>

TransferCurveAnalyzer::TransferCurveAnalyzer(const juce::File& outDir, int numBins,
                                             const std::vector<juce::String>& paramNames,
                                             const juce::String& signalType)
    : numBins(numBins), paramNames(paramNames), outputDir(outDir), signalType(signalType) {}

TransferCurveAnalyzer::~TransferCurveAnalyzer() {}

int TransferCurveAnalyzer::getBinIndex(float x, float maxInput) const {
    if (!(maxInput > 0.0f))
        maxInput = 1.0f;
    // Map x from [-maxInput, maxInput] to [0, numBins-1]
    float normalized = (x + maxInput) / (2.0f * maxInput); // [0, 1]
    int bin = (int)(normalized * (float)numBins);
    return std::clamp(bin, 0, numBins - 1);
}

float TransferCurveAnalyzer::getBinCenter(int binIndex, float maxInput) const {
    if (!(maxInput > 0.0f))
        maxInput = 1.0f;
    // Inverse of getBinIndex: map [0, numBins-1] to [-maxInput, maxInput]
    float normalized = ((float)binIndex + 0.5f) / (float)numBins; // [0, 1]
    return normalized * (2.0f * maxInput) - maxInput;             // [-maxInput, maxInput]
}

void TransferCurveAnalyzer::processBlock(const BlockContext& ctx) {
    auto& runData = perRunBins[ctx.runId];

    // Initialize bins on first block
    if (runData.bins.empty()) {
        runData.bins.resize(numBins);
        runData.paramValues = ctx.paramNamedValues;
        runData.inputGainDb = ctx.inputGainDb;
        runData.maxInput = std::pow(10.0f, ctx.inputGainDb / 20.0f);
        if (!(runData.maxInput > 0.0f))
            runData.maxInput = 1.0f;
    }

    // Accumulate input->output mapping
    for (int i = 0; i < ctx.numSamples; ++i) {
        float x = ctx.inL[i];
        float y = ctx.outL[i];

        int binIdx = getBinIndex(x, runData.maxInput);
        runData.bins[binIdx].sumY += (double)y;
        runData.bins[binIdx].count++;
    }
}

void TransferCurveAnalyzer::finish(const juce::File& outDir) {
    juce::String filename = "grid_transfer_curves_" + signalType.toLowerCase() + ".csv";
    juce::File csvFile = outDir.getChildFile(filename);
    std::ofstream out(csvFile.getFullPathName().toStdString());

    if (!out.is_open()) {
        std::cerr << "Failed to open " << filename.toStdString() << " for writing" << std::endl;
        return;
    }

    // Header
    out << "runId,binIndex,x,meanY,count";
    for (const auto& paramName : paramNames) {
        out << "," << paramName.toStdString();
    }
    out << ",inputGainDb\n";

    // Data rows
    for (const auto& [runId, runData] : perRunBins) {
        for (int binIdx = 0; binIdx < numBins; ++binIdx) {
            const auto& bin = runData.bins[binIdx];
            if (bin.count == 0)
                continue;

            float x = getBinCenter(binIdx, runData.maxInput);
            double meanY = bin.sumY / bin.count;

            out << runId << "," << binIdx << "," << x << "," << meanY << "," << bin.count;

            // Parameter values
            for (const auto& paramName : paramNames) {
                float value = 0.0f;
                auto it = runData.paramValues.find(paramName);
                if (it != runData.paramValues.end())
                    value = it->second;
                out << "," << value;
            }

            out << "," << runData.inputGainDb << "\n";
        }
    }
}

std::unique_ptr<Analyzer> createTransferCurveAnalyzer(const juce::File& outDir, int numBins,
                                                      const std::vector<juce::String>& paramNames,
                                                      const juce::String& signalType) {
    return std::make_unique<TransferCurveAnalyzer>(outDir, numBins, paramNames, signalType);
}

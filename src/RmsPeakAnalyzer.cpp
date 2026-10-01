#include "RmsPeakAnalyzer.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

RmsPeakAnalyzer::RmsPeakAnalyzer(const juce::File& outDir, const std::vector<juce::String>& paramNames,
                                 const juce::String& signalType)
    : paramNames(paramNames), outputDir(outDir), signalType(signalType) {}

RmsPeakAnalyzer::~RmsPeakAnalyzer() {}

void RmsPeakAnalyzer::processBlock(const BlockContext& ctx) {
    auto& stats = perRunStats[ctx.runId];

    // Store metadata on first block of each run
    if (runParamValues.find(ctx.runId) == runParamValues.end()) {
        runParamValues[ctx.runId] = ctx.paramNamedValues;
        runInputGainDb[ctx.runId] = ctx.inputGainDb;
    }

    for (int i = 0; i < ctx.numSamples; ++i) {
        // Input L
        float inL = ctx.inL[i];
        stats.sumSqInL += (double)(inL * inL);
        stats.peakInL = std::max(stats.peakInL, std::abs(inL));

        // Input R
        if (ctx.inR != nullptr) {
            float inR = ctx.inR[i];
            stats.sumSqInR += (double)(inR * inR);
            stats.peakInR = std::max(stats.peakInR, std::abs(inR));
        }

        // Output L
        float outL = ctx.outL[i];
        stats.sumSqOutL += (double)(outL * outL);
        stats.peakOutL = std::max(stats.peakOutL, std::abs(outL));

        // Output R
        if (ctx.outR != nullptr) {
            float outR = ctx.outR[i];
            stats.sumSqOutR += (double)(outR * outR);
            stats.peakOutR = std::max(stats.peakOutR, std::abs(outR));
        }

        stats.sampleCount++;
    }
}

void RmsPeakAnalyzer::finish(const juce::File& outDir) {
    juce::String filename = "grid_rms_peak_" + signalType.toLowerCase() + ".csv";
    juce::File csvFile = outDir.getChildFile(filename);
    std::ofstream out(csvFile.getFullPathName().toStdString());

    if (!out.is_open())
        throw std::runtime_error("Failed to open " + csvFile.getFullPathName().toStdString() + " for writing");

    // Header
    out << "runId";
    for (const auto& paramName : paramNames) {
        out << "," << paramName.toStdString();
    }
    out << ",inputGainDb";
    out << ",rmsInL,rmsInR,rmsOutL,rmsOutR";
    out << ",peakInL,peakInR,peakOutL,peakOutR";
    out << "\n";

    // Data rows
    for (const auto& [runId, stats] : perRunStats) {
        out << runId;

        // Parameter values
        auto paramIt = runParamValues.find(runId);
        for (const auto& paramName : paramNames) {
            float value = 0.0f;
            if (paramIt != runParamValues.end()) {
                auto valIt = paramIt->second.find(paramName);
                if (valIt != paramIt->second.end())
                    value = valIt->second;
            }
            out << "," << value;
        }

        // Input gain
        float inputGain = 0.0f;
        auto gainIt = runInputGainDb.find(runId);
        if (gainIt != runInputGainDb.end())
            inputGain = gainIt->second;
        out << "," << inputGain;

        double rmsInL = stats.sampleCount > 0 ? std::sqrt(stats.sumSqInL / stats.sampleCount) : 0.0;
        double rmsInR = stats.sampleCount > 0 ? std::sqrt(stats.sumSqInR / stats.sampleCount) : 0.0;
        double rmsOutL = stats.sampleCount > 0 ? std::sqrt(stats.sumSqOutL / stats.sampleCount) : 0.0;
        double rmsOutR = stats.sampleCount > 0 ? std::sqrt(stats.sumSqOutR / stats.sampleCount) : 0.0;

        out << "," << rmsInL << "," << rmsInR << "," << rmsOutL << "," << rmsOutR;
        out << "," << stats.peakInL << "," << stats.peakInR << "," << stats.peakOutL << "," << stats.peakOutR;
        out << "\n";
    }

    out.close();
    if (out.fail())
        throw std::runtime_error("Failed to write " + csvFile.getFullPathName().toStdString());
}

std::unique_ptr<Analyzer> createRmsPeakAnalyzer(const juce::File& outDir, const std::vector<juce::String>& paramNames,
                                                const juce::String& signalType) {
    return std::make_unique<RmsPeakAnalyzer>(outDir, paramNames, signalType);
}

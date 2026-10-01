#include "RawCsvAnalyzer.h"
#include <iostream>
#include <stdexcept>

RawCsvAnalyzer::RawCsvAnalyzer(const juce::File& outDir, const juce::String& signalType) : signalType(signalType) {
    juce::String filename = "raw_" + signalType.toLowerCase() + ".csv";
    juce::File csvFile = outDir.getChildFile(filename);
    csvPath = csvFile;
    this->csvFile = std::make_unique<std::ofstream>(csvFile.getFullPathName().toStdString());
    if (!this->csvFile->is_open())
        throw std::runtime_error("Failed to open " + csvFile.getFullPathName().toStdString() + " for writing");
}

RawCsvAnalyzer::~RawCsvAnalyzer() {
    if (csvFile && csvFile->is_open())
        csvFile->close();
}

void RawCsvAnalyzer::processBlock(const BlockContext& ctx) {
    if (!csvFile)
        return;

    if (!headerWritten) {
        *csvFile << "runId,sample,time_sec,inL";
        if (ctx.inR != nullptr)
            *csvFile << ",inR";
        *csvFile << ",outL";
        if (ctx.outR != nullptr)
            *csvFile << ",outR";
        *csvFile << "\n";
        headerWritten = true;
    }

    for (int i = 0; i < ctx.numSamples; ++i) {
        int64_t sampleIndex = ctx.firstSample + i;
        double timeSec = (double)sampleIndex / ctx.sampleRate;

        *csvFile << ctx.runId << "," << sampleIndex << "," << timeSec << "," << ctx.inL[i];
        if (ctx.inR != nullptr)
            *csvFile << "," << ctx.inR[i];
        *csvFile << "," << ctx.outL[i];
        if (ctx.outR != nullptr)
            *csvFile << "," << ctx.outR[i];
        *csvFile << "\n";
    }
}

void RawCsvAnalyzer::finish(const juce::File& outDir) {
    if (csvFile && csvFile->is_open()) {
        csvFile->close();
        const bool failed = csvFile->fail();
        csvFile.reset();
        if (failed)
            throw std::runtime_error("Failed to write " + csvPath.getFullPathName().toStdString());
    }
}

std::unique_ptr<Analyzer> createRawCsvAnalyzer(const juce::File& outDir, const juce::String& signalType) {
    return std::make_unique<RawCsvAnalyzer>(outDir, signalType);
}

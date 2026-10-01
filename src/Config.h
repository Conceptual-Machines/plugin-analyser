#pragma once

#include "JuceHeader.h"
#include <string>
#include <vector>

struct ParameterBucketConfig {
    juce::String paramName;
    juce::String strategy; // "Linear", "ExplicitValues", "Log", "EdgeAndCenter"
    float min = 0.0f;
    float max = 1.0f;
    int numBuckets = 0;
    std::vector<float> values;
};

struct Config {
    juce::String pluginPath;
    double sampleRate = 48000.0;
    double seconds = 5.0;
    double preRollSeconds = 0.1; // silence rendered after reset() and before each run, not measured
    int blockSize = 256;
    juce::String signalType; // "sine", "noise", "sweep"
    double sineFrequency = 1000.0;
    double sweepStartHz = 20.0;
    double sweepEndHz = 20000.0;
    std::vector<float> inputGainBucketsDb;
    std::vector<ParameterBucketConfig> parameterBuckets;
    std::vector<juce::String> analyzers;

    static Config fromJson(const juce::File& jsonFile);
    static Config fromJsonString(const juce::String& jsonString);
};

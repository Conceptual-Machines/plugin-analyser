#include "SignalGenerator.h"
#include <cmath>

void SineGenerator::fillBlock(juce::AudioBuffer<float>& buffer, int numSamples) {
    const double phaseIncrement = 2.0 * juce::MathConstants<double>::pi * frequency / sampleRate;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        double currentPhase = phase;

        for (int i = 0; i < numSamples; ++i) {
            channelData[i] = amplitude * (float)std::sin(currentPhase);
            currentPhase += phaseIncrement;

            if (currentPhase > 2.0 * juce::MathConstants<double>::pi)
                currentPhase -= 2.0 * juce::MathConstants<double>::pi;
        }
    }

    phase = std::fmod(phase + phaseIncrement * numSamples, 2.0 * juce::MathConstants<double>::pi);
}

void NoiseGenerator::fillBlock(juce::AudioBuffer<float>& buffer, int numSamples) {
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            // Generate white noise in range [-amplitude, amplitude]
            channelData[i] = amplitude * (2.0f * rng.nextFloat() - 1.0f);
        }
    }
}

void SweepGenerator::reset() {
    currentPhase = 0.0;
    currentFreq = startHz;
    currentSample = 0;
}

void SweepGenerator::fillBlock(juce::AudioBuffer<float>& buffer, int numSamples) {
    const int64_t totalSamples = (int64_t)(duration * sampleRate);
    const double logStart = std::log(startHz);
    const double logEnd = std::log(endHz);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            if (currentSample >= totalSamples) {
                channelData[i] = 0.0f;
                continue;
            }

            // Logarithmic sweep
            double t = (double)currentSample / (double)totalSamples;
            double logFreq = logStart + t * (logEnd - logStart);
            currentFreq = std::exp(logFreq);

            const double phaseIncrement = 2.0 * juce::MathConstants<double>::pi * currentFreq / sampleRate;
            channelData[i] = amplitude * (float)std::sin(currentPhase);

            currentPhase += phaseIncrement;
            if (currentPhase > 2.0 * juce::MathConstants<double>::pi)
                currentPhase -= 2.0 * juce::MathConstants<double>::pi;

            currentSample++;
        }
    }
}

#include "MeasurementEngine.h"
#include "BucketSpec.h"
#include "LinearResponseAnalyzer.h"
#include "PluginLoader.h"
#include "RawCsvAnalyzer.h"
#include "RmsPeakAnalyzer.h"
#include "ThdAnalyzer.h"
#include "TransferCurveAnalyzer.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

std::vector<RunConfig> buildRunGrid(const Config& config, const std::vector<juce::String>& paramNames) {
    std::cerr << "[buildRunGrid] Starting with " << paramNames.size() << " parameters, "
              << config.parameterBuckets.size() << " bucket configs" << std::endl;
    std::vector<RunConfig> runs;

    // Convert ParameterBucketConfig to BucketSpec and generate values
    std::vector<std::pair<juce::String, std::vector<float>>> paramValueLists;

    for (const auto& bucketConfig : config.parameterBuckets) {
        std::cerr << "[buildRunGrid] Processing bucket for parameter: " << bucketConfig.paramName << std::endl;
        BucketSpec spec;
        spec.paramName = bucketConfig.paramName;
        spec.strategy = BucketSpec::strategyFromString(bucketConfig.strategy);
        spec.min = bucketConfig.min;
        spec.max = bucketConfig.max;
        spec.numBuckets = bucketConfig.numBuckets;
        spec.values = bucketConfig.values;

        auto values = spec.generateValues();
        std::cerr << "[buildRunGrid] Generated " << values.size() << " values for " << bucketConfig.paramName
                  << std::endl;
        paramValueLists.push_back({bucketConfig.paramName, values});
    }

    // Build Cartesian product of parameter values and input gain buckets
    int runId = 0;
    std::cerr << "[buildRunGrid] Building Cartesian product with " << config.inputGainBucketsDb.size()
              << " input gain buckets..." << std::endl;

    // Helper function to generate combinations recursively
    std::function<void(int, std::map<juce::String, float>)> generateCombinations;
    generateCombinations = [&](int paramIndex, std::map<juce::String, float> currentParams) {
        if (paramIndex >= (int)paramValueLists.size()) {
            // All parameters set, now combine with input gain buckets
            for (float inputGainDb : config.inputGainBucketsDb) {
                RunConfig run;
                run.runId = runId++;
                run.paramValues = currentParams;
                run.inputGainDb = inputGainDb;
                runs.push_back(run);
            }
            if (runId % 1000 == 0) {
                std::cerr << "[buildRunGrid] Generated " << runId << " runs so far..." << std::endl;
            }
            return;
        }

        // Try all values for current parameter
        const auto& [paramName, values] = paramValueLists[paramIndex];
        for (float value : values) {
            auto newParams = currentParams;
            newParams[paramName] = value;
            generateCombinations(paramIndex + 1, newParams);
        }
    };

    generateCombinations(0, {});
    std::cerr << "[buildRunGrid] Complete: generated " << runs.size() << " total runs" << std::endl;
    return runs;
}

std::vector<std::unique_ptr<Analyzer>> createAnalyzers(const Config& config, const juce::File& outDir,
                                                       const std::vector<juce::String>& paramNames) {
    std::vector<std::unique_ptr<Analyzer>> analyzers;

    for (const auto& analyzerName : config.analyzers) {
        if (analyzerName.equalsIgnoreCase("RawCsv")) {
            analyzers.push_back(createRawCsvAnalyzer(outDir, config.signalType));
        } else if (analyzerName.equalsIgnoreCase("RmsPeak")) {
            analyzers.push_back(createRmsPeakAnalyzer(outDir, paramNames, config.signalType));
        } else if (analyzerName.equalsIgnoreCase("TransferCurve")) {
            analyzers.push_back(createTransferCurveAnalyzer(outDir, 512, paramNames, config.signalType));
        } else if (analyzerName.equalsIgnoreCase("LinearResponse")) {
            if (config.signalType.equalsIgnoreCase("noise") || config.signalType.equalsIgnoreCase("sweep")) {
                analyzers.push_back(createLinearResponseAnalyzer(outDir, 4096, paramNames, config.signalType));
            } else {
                std::cerr << "Warning: LinearResponse analyzer requires noise or sweep signal type" << std::endl;
            }
        } else if (analyzerName.equalsIgnoreCase("Thd")) {
            if (config.signalType.equalsIgnoreCase("sine")) {
                analyzers.push_back(
                    createThdAnalyzer(outDir, 2048, config.sineFrequency, paramNames, config.signalType));
            } else {
                std::cerr << "Warning: Thd analyzer requires sine signal type" << std::endl;
            }
        } else {
            std::cerr << "Warning: Unknown analyzer: " << analyzerName << std::endl;
        }
    }

    return analyzers;
}

// Worker function to process a single run
static void processRun(const RunConfig& run, juce::AudioPluginInstance& plugin,
                       const std::map<juce::String, juce::AudioProcessorParameter*>& paramMap,
                       const std::vector<juce::String>& paramNames, const Config& config, double sampleRate,
                       int blockSize, int64_t totalSamples, std::vector<std::unique_ptr<Analyzer>>& analyzers,
                       std::mutex& analyzerMutex) {
    // Set plugin parameters
    for (const auto& [paramName, value] : run.paramValues) {
        setParameterValue(plugin, paramMap, paramName, value);
    }

    // Convert input gain from dB to linear amplitude
    float inputGainLinear = std::pow(10.0f, run.inputGainDb / 20.0f);

    // Create signal generator
    std::unique_ptr<SineGenerator> sineGen;
    std::unique_ptr<NoiseGenerator> noiseGen;
    std::unique_ptr<SweepGenerator> sweepGen;

    if (config.signalType.equalsIgnoreCase("sine")) {
        sineGen = std::make_unique<SineGenerator>();
        sineGen->sampleRate = sampleRate;
        sineGen->frequency = config.sineFrequency;
        sineGen->amplitude = inputGainLinear;
    } else if (config.signalType.equalsIgnoreCase("noise")) {
        noiseGen = std::make_unique<NoiseGenerator>();
        noiseGen->amplitude = inputGainLinear;
    } else if (config.signalType.equalsIgnoreCase("sweep")) {
        sweepGen = std::make_unique<SweepGenerator>();
        sweepGen->sampleRate = sampleRate;
        sweepGen->startHz = config.sweepStartHz;
        sweepGen->endHz = config.sweepEndHz;
        sweepGen->duration = config.seconds;
        sweepGen->amplitude = inputGainLinear;
        sweepGen->reset();
    }

    // Process samples
    juce::AudioBuffer<float> inputBuffer(2, blockSize);
    juce::AudioBuffer<float> outputBuffer(2, blockSize);
    juce::MidiBuffer midiBuffer;

    // Start every run from a clean state, so results don't depend on the previous run (reverb tails,
    // compressor envelopes, filter memory) or, with several threads, on which run an instance did last
    plugin.reset();

    // Pre-roll silence so parameter smoothing settles on the new values before anything is measured
    const int64_t preRollSamples = (int64_t)(std::max(0.0, config.preRollSeconds) * sampleRate);
    for (int64_t done = 0; done < preRollSamples; done += blockSize) {
        outputBuffer.clear();
        midiBuffer.clear(); // don't accumulate MIDI the plugin emits during the pre-roll
        plugin.processBlock(outputBuffer, midiBuffer);
    }

    // The output lags the input by the plugin's latency (queried after the parameters have taken effect).
    // Render that many extra samples, drop the first `latency` output samples, and pair each output sample
    // with the input from `latency` samples earlier.
    const int latency = std::max(0, plugin.getLatencySamples());
    const int64_t renderSamples = totalSamples + latency;
    juce::AudioBuffer<float> delayedInput(2, blockSize);
    juce::AudioBuffer<float> delayLine(2, std::max(latency, 1));
    delayLine.clear();
    int delayPos = 0;

    int64_t currentSample = 0;
    while (currentSample < renderSamples) {
        int numThisBlock = (int)std::min((int64_t)blockSize, renderSamples - currentSample);

        // Clear buffers
        inputBuffer.clear();
        outputBuffer.clear();

        // Fill input with test signal
        if (sineGen) {
            sineGen->fillBlock(inputBuffer, numThisBlock);
        } else if (noiseGen) {
            noiseGen->fillBlock(inputBuffer, numThisBlock);
        } else if (sweepGen) {
            sweepGen->fillBlock(inputBuffer, numThisBlock);
        }

        // Copy input to output buffer (processBlock works in-place)
        outputBuffer.makeCopyOf(inputBuffer);

        // Process through plugin (modifies outputBuffer in-place)
        plugin.processBlock(outputBuffer, midiBuffer);

        // Delay the input by `latency` samples so it lines up with the output
        for (int ch = 0; ch < 2; ++ch) {
            if (latency == 0) {
                delayedInput.copyFrom(ch, 0, inputBuffer, ch, 0, numThisBlock);
                continue;
            }
            int pos = delayPos;
            for (int i = 0; i < numThisBlock; ++i) {
                delayedInput.setSample(ch, i, delayLine.getSample(ch, pos));
                delayLine.setSample(ch, pos, inputBuffer.getSample(ch, i));
                pos = (pos + 1) % latency;
            }
        }
        if (latency > 0)
            delayPos = (int)((delayPos + numThisBlock) % latency);

        // Output samples before index `latency` correspond to no input sample: skip them
        const int skip = (int)std::clamp<int64_t>((int64_t)latency - currentSample, 0, numThisBlock);
        if (skip == numThisBlock) {
            currentSample += numThisBlock;
            continue;
        }

        // Build BlockContext
        BlockContext ctx;
        ctx.firstSample = currentSample + skip - latency;
        ctx.sampleRate = sampleRate;
        ctx.numSamples = numThisBlock - skip;
        ctx.inL = delayedInput.getReadPointer(0) + skip;
        ctx.inR = delayedInput.getReadPointer(1) + skip;
        ctx.outL = outputBuffer.getReadPointer(0) + skip;
        ctx.outR = outputBuffer.getNumChannels() > 1 ? outputBuffer.getReadPointer(1) + skip : nullptr;
        ctx.runId = run.runId;
        ctx.paramNamedValues = run.paramValues;
        ctx.inputGainDb = run.inputGainDb;

        // Build params vector in fixed order
        for (const auto& paramName : paramNames) {
            float value = 0.0f;
            auto it = run.paramValues.find(paramName);
            if (it != run.paramValues.end())
                value = it->second;
            ctx.params.push_back(value);
        }

        // Process through analyzers (with mutex protection)
        {
            std::lock_guard<std::mutex> lock(analyzerMutex);
            for (auto& analyzer : analyzers) {
                analyzer->processBlock(ctx);
            }
        }

        currentSample += numThisBlock;
    }
}

void runMeasurementGrid(juce::AudioPluginInstance& plugin, double sampleRate, int blockSize, int64_t totalSamples,
                        const std::vector<RunConfig>& runs, const std::vector<std::unique_ptr<Analyzer>>& analyzers,
                        const Config& config, const juce::File& outDir, std::function<void(int)> progressCallback,
                        int numThreads) {
    std::cerr << "[runMeasurementGrid] Starting with " << runs.size() << " runs, " << totalSamples
              << " samples per run, " << numThreads << " thread(s)" << std::endl;

    // Build parameter name list in order
    std::vector<juce::String> paramNames;
    for (const auto& bucket : config.parameterBuckets) {
        paramNames.push_back(bucket.paramName);
    }

    // If single-threaded, use original sequential code
    if (numThreads <= 1) {
        auto paramMap = buildParameterMap(plugin, false);
        std::mutex dummyMutex;
        for (const auto& run : runs) {
            if (progressCallback) {
                progressCallback(run.runId);
            }
            processRun(run, plugin, paramMap, paramNames, config, sampleRate, blockSize, totalSamples,
                       const_cast<std::vector<std::unique_ptr<Analyzer>>&>(analyzers), dummyMutex);
        }
    } else {
        // Multi-threaded execution
        std::queue<RunConfig> runQueue;
        std::mutex queueMutex;
        std::mutex analyzerMutex;
        std::atomic<int> completedRuns{0};
        std::atomic<int> currentRunIndex{0};

        // Populate queue
        for (const auto& run : runs) {
            runQueue.push(run);
        }

        // Pre-load all plugin instances sequentially to avoid thread-safety issues
        std::cerr << "[runMeasurementGrid] Pre-loading " << numThreads << " plugin instances..." << std::endl;
        std::vector<std::unique_ptr<juce::AudioPluginInstance>> pluginInstances;
        std::vector<std::map<juce::String, juce::AudioProcessorParameter*>> paramMaps;

        for (int i = 0; i < numThreads; ++i) {
            std::cerr << "[runMeasurementGrid] Loading plugin instance " << (i + 1) << " / " << numThreads << "..."
                      << std::endl;
            juce::String errorMessage;
            auto plugin = loadPluginInstance(juce::File(config.pluginPath), sampleRate, blockSize, errorMessage);
            if (!plugin) {
                std::cerr << "[runMeasurementGrid] Failed to load plugin instance " << i << ": " << errorMessage
                          << std::endl;
                return; // Can't continue without all instances
            }
            auto paramMap = buildParameterMap(*plugin, false);
            pluginInstances.push_back(std::move(plugin));
            paramMaps.push_back(paramMap);
            std::cerr << "[runMeasurementGrid] Plugin instance " << (i + 1) << " ready" << std::endl;
        }
        std::cerr << "[runMeasurementGrid] All plugin instances loaded, starting worker threads..." << std::endl;

        // Worker function
        auto worker = [&](int threadId) {
            auto& threadPlugin = pluginInstances[threadId];
            auto& paramMap = paramMaps[threadId];
            std::cerr << "[runMeasurementGrid] Thread " << threadId << " ready, starting to process runs..."
                      << std::endl;

            while (true) {
                RunConfig run;
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    if (runQueue.empty()) {
                        break;
                    }
                    run = runQueue.front();
                    runQueue.pop();
                }

                int runIdx = currentRunIndex.fetch_add(1);
                if (progressCallback) {
                    progressCallback(run.runId);
                }
                if (runIdx % 10 == 0 || runIdx == 0) {
                    std::cerr << "[runMeasurementGrid] Thread processing run " << run.runId << " / " << runs.size()
                              << std::endl;
                }

                processRun(run, *threadPlugin, paramMap, paramNames, config, sampleRate, blockSize, totalSamples,
                           const_cast<std::vector<std::unique_ptr<Analyzer>>&>(analyzers), analyzerMutex);

                completedRuns.fetch_add(1);
            }
        };

        // Launch threads
        std::cerr << "[runMeasurementGrid] Launching " << numThreads << " worker threads..." << std::endl;
        std::vector<std::thread> threads;
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back(worker, i);
        }
        std::cerr << "[runMeasurementGrid] All threads launched" << std::endl;

        // Wait for all threads
        for (auto& thread : threads) {
            thread.join();
        }

        // Release plugin instances
        for (auto& plugin : pluginInstances) {
            plugin->releaseResources();
        }

        std::cerr << "[runMeasurementGrid] All threads completed. Processed " << completedRuns.load() << " runs"
                  << std::endl;
    }

    // Finish all analyzers
    for (auto& analyzer : analyzers) {
        analyzer->finish(outDir);
    }
}

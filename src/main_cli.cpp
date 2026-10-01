#include "Config.h"
#include "JuceHeader.h"
#include "MeasurementEngine.h"
#include "PluginLoader.h"
#include <iostream>
#include <thread>
#include <vector>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " --config <path> --out <path> [options]\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --config <path>     JSON configuration file (required)\n";
    std::cout << "  --out <path>        Output directory (required)\n";
    std::cout << "  --plugin <path>     Override pluginPath in JSON\n";
    std::cout << "  --seconds N         Override duration in seconds\n";
    std::cout << "  --samplerate SR     Override sample rate\n";
    std::cout << "  --blocksize BS      Override block size\n";
    std::cout << "  --threads N         Override number of threads (default: auto-detect)\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    juce::String configPath;
    juce::String outPath;
    juce::String pluginPathOverride;
    double secondsOverride = -1.0;
    double sampleRateOverride = -1.0;
    int blockSizeOverride = -1;
    int threadsOverride = -1;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        juce::String arg = argv[i];

        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--out" && i + 1 < argc) {
            outPath = argv[++i];
        } else if (arg == "--plugin" && i + 1 < argc) {
            pluginPathOverride = argv[++i];
        } else if (arg == "--seconds" && i + 1 < argc) {
            secondsOverride = juce::String(argv[++i]).getDoubleValue();
        } else if (arg == "--samplerate" && i + 1 < argc) {
            sampleRateOverride = juce::String(argv[++i]).getDoubleValue();
        } else if (arg == "--blocksize" && i + 1 < argc) {
            blockSizeOverride = juce::String(argv[++i]).getIntValue();
        } else if (arg == "--threads" && i + 1 < argc) {
            threadsOverride = juce::String(argv[++i]).getIntValue();
        }
    }

    if (configPath.isEmpty() || outPath.isEmpty()) {
        std::cerr << "Error: --config and --out are required\n";
        printUsage(argv[0]);
        return 1;
    }

    try {
        // Load config
        juce::File configFile(configPath);
        Config config = Config::fromJson(configFile);

        // Apply overrides
        if (!pluginPathOverride.isEmpty())
            config.pluginPath = pluginPathOverride;
        if (secondsOverride > 0.0)
            config.seconds = secondsOverride;
        if (sampleRateOverride > 0.0)
            config.sampleRate = sampleRateOverride;
        if (blockSizeOverride > 0)
            config.blockSize = blockSizeOverride;

        config.validate();

        // Create output directory
        juce::File outDir(outPath);
        if (!outDir.exists()) {
            outDir.createDirectory();
        }

        if (!outDir.isDirectory()) {
            std::cerr << "Error: Output path is not a directory: " << outPath << std::endl;
            return 1;
        }

        // Load plugin
        std::cout << "Loading plugin: " << config.pluginPath << std::endl;
        juce::String errorMessage;
        auto plugin =
            loadPluginInstance(juce::File(config.pluginPath), config.sampleRate, config.blockSize, errorMessage);

        if (plugin == nullptr) {
            std::cerr << (errorMessage.isEmpty() ? "Failed to load plugin" : errorMessage.toStdString()) << std::endl;
            return 1;
        }

        std::cout << "Plugin loaded: " << plugin->getName() << std::endl;

        // List all available parameters
        auto paramMap = buildParameterMap(*plugin, true); // Show only UI-exposed parameters
        std::cout << "\nAvailable parameters (" << paramMap.size() << "):" << std::endl;
        std::cout << "========================================" << std::endl;
        int idx = 0;
        for (const auto& [name, param] : paramMap) {
            // Filter out MIDI CC parameters
            juce::String lowerName = name.toLowerCase();
            if (lowerName.contains("midi") || lowerName.contains("cc ") || lowerName.startsWith("cc")) {
                continue;
            }
            float value = param->getValue();
            float defaultValue = param->getDefaultValue();
            juce::String displayName = param->getName(512);
            std::cout << idx++ << ". " << displayName << " (internal: " << name << ")" << std::endl;
            std::cout << "   Value: " << value << " (default: " << defaultValue << ")" << std::endl;
        }
        std::cout << "========================================\n" << std::endl;

        // Build parameter name list
        std::vector<juce::String> paramNames;
        for (const auto& bucket : config.parameterBuckets) {
            paramNames.push_back(bucket.paramName);
        }

        // Build run grid
        std::cout << "Building measurement grid..." << std::endl;
        auto runs = buildRunGrid(config, paramNames);
        std::cout << "Generated " << runs.size() << " measurement runs" << std::endl;

        // Create analyzers
        std::cout << "Creating analyzers..." << std::endl;
        auto analyzers = createAnalyzers(config, outDir, paramNames);
        std::cout << "Created " << analyzers.size() << " analyzers" << std::endl;

        // Run measurements
        int64_t totalSamples = (int64_t)(config.seconds * config.sampleRate);
        int numThreads;
        if (threadsOverride > 0) {
            numThreads = threadsOverride;
            std::cout << "Using " << numThreads << " thread(s) (override)" << std::endl;
        } else {
            numThreads = (int)std::thread::hardware_concurrency();
            if (numThreads == 0)
                numThreads = 1; // Fallback if detection fails
            std::cout << "Running measurements with " << numThreads << " thread(s) (auto-detected)" << std::endl;
        }
        runMeasurementGrid(*plugin, config.sampleRate, config.blockSize, totalSamples, runs, analyzers, config, outDir,
                           nullptr, numThreads);

        // (runMeasurementGrid finishes the analyzers and writes their output)

        std::cout << "Measurement complete!" << std::endl;

        plugin->releaseResources();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

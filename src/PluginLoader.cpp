#include "PluginLoader.h"
#include <iostream>

std::unique_ptr<juce::AudioPluginInstance> loadPluginInstance(const juce::File& pluginFile, double sampleRate,
                                                              int blockSize, juce::String& errorMessageOut) {
    errorMessageOut.clear();

    std::cerr << "[loadPluginInstance] Starting load for: " << pluginFile.getFullPathName() << std::endl;

    // VST3 plugins on macOS are bundles (directories), not files
    if (!pluginFile.exists()) {
        errorMessageOut = "Plugin file does not exist: " + pluginFile.getFullPathName();
        std::cerr << errorMessageOut << std::endl;
        return nullptr;
    }

    std::cerr << "[loadPluginInstance] Plugin file exists, creating format manager..." << std::endl;

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    std::cerr << "[loadPluginInstance] Format manager created, getting formats..." << std::endl;

    juce::String errorMessage;
    juce::PluginDescription description;

    // For VST3 bundles on macOS, we need to check the bundle structure
    juce::String pluginPath = pluginFile.getFullPathName();

    // If it's a .vst3 bundle, ensure we're pointing to the bundle directory
    if (pluginFile.isDirectory() && pluginPath.endsWithIgnoreCase(".vst3")) {
        // VST3 bundle is correct - use as-is
    } else if (pluginFile.isDirectory()) {
        // Might be pointing inside the bundle, try to find the .vst3 parent
        juce::File current = pluginFile;
        while (!current.isRoot() && !current.getFileName().endsWithIgnoreCase(".vst3")) {
            current = current.getParentDirectory();
        }
        if (current.getFileName().endsWithIgnoreCase(".vst3")) {
            pluginPath = current.getFullPathName();
        }
    }

    // Get number of formats (debug output removed to reduce noise in multi-threaded runs)
    int numFormats = formatManager.getNumFormats();
    std::cerr << "[loadPluginInstance] Got " << numFormats << " formats, scanning for plugin..." << std::endl;

    // Try to find the plugin by scanning
    bool foundFormat = false;

    for (int i = 0; i < numFormats; ++i) {
        auto* format = formatManager.getFormat(i);
        juce::String formatName = format->getName();
        std::cerr << "[loadPluginInstance] Checking format " << i << ": " << formatName << std::endl;

        if (format->fileMightContainThisPluginType(pluginPath)) {
            std::cerr << "[loadPluginInstance] Format " << formatName << " might contain plugin, scanning..." << std::endl;
            foundFormat = true;
            juce::OwnedArray<juce::PluginDescription> found;
            format->findAllTypesForFile(found, pluginPath);
            std::cerr << "[loadPluginInstance] Scan complete, found " << found.size() << " plugin(s)" << std::endl;
            if (found.size() > 0) {
                description = *found[0];
                std::cerr << "[loadPluginInstance] Found plugin: " << description.name << " (" << formatName << ")" << std::endl;
                break;
            } else {
                std::cerr << "[loadPluginInstance] Format " << formatName << " recognized file but found no plugins" << std::endl;
            }
        } else {
            std::cerr << "[loadPluginInstance] Format " << formatName << " does not match file type" << std::endl;
        }
    }

    if (!foundFormat) {
        errorMessageOut = "No plugin format recognized for: " + pluginPath +
                          "\nMake sure the file is a valid VST3 plugin bundle (.vst3 directory).";
        std::cerr << errorMessageOut << std::endl;
        std::cerr << "Available formats: ";
        for (int i = 0; i < numFormats; ++i) {
            std::cerr << formatManager.getFormat(i)->getName();
            if (i < numFormats - 1)
                std::cerr << ", ";
        }
        std::cerr << std::endl;
        return nullptr;
    }

    if (description.name.isEmpty()) {
        errorMessageOut = "Failed to get plugin description for: " + pluginPath +
                          "\nThe file may be corrupted or not a valid plugin.";
        std::cerr << errorMessageOut << std::endl;
        return nullptr;
    }

    std::cerr << "[loadPluginInstance] Found plugin: " << description.name << ", creating instance..." << std::endl;

    auto instance = formatManager.createPluginInstance(description, sampleRate, blockSize, errorMessage);

    if (instance == nullptr) {
        errorMessageOut = "Failed to create plugin instance: " + errorMessage;
        std::cerr << errorMessageOut << std::endl;
        return nullptr;
    }

    std::cerr << "[loadPluginInstance] Instance created, preparing to play..." << std::endl;

    // Rendering is offline: let plugins use their offline/bounce quality (oversampling, lookahead, etc.)
    instance->setNonRealtime(true);
    instance->prepareToPlay(sampleRate, blockSize);

    std::cerr << "[loadPluginInstance] Plugin ready!" << std::endl;

    return instance;
}

std::map<juce::String, juce::AudioProcessorParameter*> buildParameterMap(juce::AudioPluginInstance& plugin,
                                                                         bool uiOnly) {
    std::map<juce::String, juce::AudioProcessorParameter*> paramMap;

    for (auto* param : plugin.getParameters()) {
        if (param == nullptr)
            continue;

        // Filter out non-UI parameters if requested
        if (uiOnly) {
            // Skip meta parameters (internal automation)
            if (param->isMetaParameter())
                continue;

            // Skip non-automatable parameters (typically internal)
            if (!param->isAutomatable())
                continue;

            // Skip MIDI CC parameters (they're not UI-exposed)
            juce::String paramName = param->getName(512);
            if (paramName.containsIgnoreCase("MIDI CC") || paramName.containsIgnoreCase("midi cc"))
                continue;
        }

        juce::String paramName = param->getName(512);
        paramName = paramName.trim().toLowerCase();
        paramMap[paramName] = param;
    }

    return paramMap;
}

void setParameterValue(juce::AudioPluginInstance& plugin,
                       const std::map<juce::String, juce::AudioProcessorParameter*>& paramMap,
                       const juce::String& paramName, float normalizedValue) {
    juce::String searchName = paramName.trim().toLowerCase();
    auto it = paramMap.find(searchName);

    if (it == paramMap.end()) {
        std::cerr << "Warning: Parameter not found: " << paramName << std::endl;
        std::cerr << "Available parameters (excluding MIDI CC):" << std::endl;
        int count = 0;
        for (const auto& [name, param] : paramMap) {
            // Filter out MIDI CC parameters
            juce::String lowerName = name.toLowerCase();
            if (lowerName.contains("midi") || lowerName.contains("cc ") || lowerName.startsWith("cc")) {
                continue;
            }
            std::cerr << "  - '" << name << "'" << std::endl;
            count++;
        }
        std::cerr << "Total: " << count << " parameters (excluding MIDI CC)" << std::endl;
        return;
    }

    it->second->setValueNotifyingHost(normalizedValue);
}

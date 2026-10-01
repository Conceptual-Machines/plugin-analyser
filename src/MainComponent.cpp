#include "MainComponent.h"
#include "MeasurementConfigComponent.h"
#include "ParameterConfigComponent.h"
#include <iostream>
#include <thread>
#include <typeinfo>

MainComponent::MainComponent()
    : pluginPathLabel("Plugin Path:", "Plugin Path:"), pluginInfoLabel("", "No plugin loaded"),
      parametersLabel("Parameters:", "Parameters:"), outputPathLabel("Output Path:", "Output Path:"),
      progressLabel("", "Ready"), progressBar(progress) {
    // Plugin path section
    addAndMakeVisible(pluginPathLabel);
    // Default plugin path - set to dev plugin in debug builds
#ifdef DEBUG
    pluginPathEditor.setText("/Volumes/External SSD/Plug-Ins/VST3/Acustica/GAINSTATION2.vst3",
                             juce::dontSendNotification);
#else
    // Default plugin path - can be set to common VST3 location or left empty
    pluginPathEditor.setText("", juce::dontSendNotification);
#endif
    pluginPathEditor.addListener(this);
    addAndMakeVisible(pluginPathEditor);

    browseButton.setButtonText("Browse...");
    browseButton.addListener(this);
    addAndMakeVisible(browseButton);

    loadPluginButton.setButtonText("Load Plugin");
    loadPluginButton.addListener(this);
    addAndMakeVisible(loadPluginButton);

    addAndMakeVisible(pluginInfoLabel);

    // Parameter list
    addAndMakeVisible(parametersLabel);
    parameterListBox.setModel(this);
    parameterListBox.setRowSelectedOnMouseDown(false); // Disable row selection - we handle clicks ourselves
    parameterListBox.setMultipleSelectionEnabled(false);
    addAndMakeVisible(parameterListBox);

    selectAllButton.setButtonText("Select All");
    selectAllButton.addListener(this);
    addAndMakeVisible(selectAllButton);

    deselectAllButton.setButtonText("Deselect All");
    deselectAllButton.addListener(this);
    addAndMakeVisible(deselectAllButton);

    // Parameter config viewport
    parameterConfigViewport.setViewedComponent(&parameterConfigContainer, false);
    addAndMakeVisible(parameterConfigViewport);

    // Measurement config
    measurementConfig = std::make_unique<MeasurementConfigComponent>();
    addAndMakeVisible(measurementConfig.get());

    // Output path
    addAndMakeVisible(outputPathLabel);
    outputPathEditor.setText(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                                 .getChildFile("plugin_measure_output")
                                 .getFullPathName(),
                             juce::dontSendNotification);
    outputPathEditor.addListener(this);
    addAndMakeVisible(outputPathEditor);

    browseOutputButton.setButtonText("Browse...");
    browseOutputButton.addListener(this);
    addAndMakeVisible(browseOutputButton);

    // Run button
    runMeasurementButton.setButtonText("Run Measurement");
    runMeasurementButton.addListener(this);
    runMeasurementButton.setEnabled(false);
    addAndMakeVisible(runMeasurementButton);

    // Progress
    addAndMakeVisible(progressLabel);
    addAndMakeVisible(progressBar);
    progressBar.setPercentageDisplay(false);

    setSize(1200, 800);
}

MainComponent::~MainComponent() {}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized() {
    auto bounds = getLocalBounds().reduced(10);

    // Plugin path section (top)
    auto pluginSection = bounds.removeFromTop(80);
    pluginPathLabel.setBounds(pluginSection.removeFromTop(25));
    auto pluginRow = pluginSection.removeFromTop(30);
    pluginPathEditor.setBounds(pluginRow.removeFromLeft(600));
    pluginRow.removeFromLeft(10);
    browseButton.setBounds(pluginRow.removeFromLeft(100));
    pluginRow.removeFromLeft(10);
    loadPluginButton.setBounds(pluginRow.removeFromLeft(120));
    pluginInfoLabel.setBounds(pluginSection);
    bounds.removeFromTop(10);

    // Main content area (side by side)
    auto contentArea = bounds.removeFromTop(500);

    // Left: Parameter list and config
    auto leftPanel = contentArea.removeFromLeft(500);
    parametersLabel.setBounds(leftPanel.removeFromTop(25));
    auto buttonRow = leftPanel.removeFromTop(30);
    selectAllButton.setBounds(buttonRow.removeFromLeft(100));
    buttonRow.removeFromLeft(10);
    deselectAllButton.setBounds(buttonRow.removeFromLeft(100));
    parameterListBox.setBounds(leftPanel.removeFromTop(150));
    leftPanel.removeFromTop(10);
    parameterConfigViewport.setBounds(leftPanel);

    // Right: Measurement config
    measurementConfig->setBounds(contentArea);

    bounds.removeFromTop(10);

    // Bottom: Output and run
    auto outputSection = bounds.removeFromTop(50);
    outputPathLabel.setBounds(outputSection.removeFromTop(25));
    auto outputRow = outputSection.removeFromTop(30);
    outputPathEditor.setBounds(outputRow.removeFromLeft(600));
    outputRow.removeFromLeft(10);
    browseOutputButton.setBounds(outputRow.removeFromLeft(100));
    outputRow.removeFromLeft(10);
    runMeasurementButton.setBounds(outputRow.removeFromLeft(150));

    bounds.removeFromTop(10);

    // Progress
    progressLabel.setBounds(bounds.removeFromTop(25));
    progressBar.setBounds(bounds.removeFromTop(30));
}

void MainComponent::buttonClicked(juce::Button* button) {
    if (button == &browseButton) {
        auto chooser = std::make_shared<juce::FileChooser>("Select VST3 Plugin", juce::File(), "*.vst3");
        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        chooser->launchAsync(chooserFlags, [this, chooser](const juce::FileChooser& fc) {
            if (fc.getResults().size() > 0) {
                pluginPathEditor.setText(fc.getResult().getFullPathName(), juce::dontSendNotification);
            }
        });
    } else if (button == &browseOutputButton) {
        auto chooser =
            std::make_shared<juce::FileChooser>("Select Output Directory", juce::File(outputPathEditor.getText()));
        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        chooser->launchAsync(chooserFlags, [this, chooser](const juce::FileChooser& fc) {
            if (fc.getResults().size() > 0) {
                outputPathEditor.setText(fc.getResult().getFullPathName(), juce::dontSendNotification);
            }
        });
    } else if (button == &loadPluginButton) {
        loadPlugin();
    } else if (button == &selectAllButton) {
        selectedParameters.resize(availableParameters.size(), true);
        std::fill(selectedParameters.begin(), selectedParameters.end(), true);
        parameterListBox.updateContent();
        // Repaint all visible rows
        int firstVisible = parameterListBox.getRowContainingPosition(0, 0);
        int lastVisible = parameterListBox.getRowContainingPosition(0, parameterListBox.getHeight());
        for (int i = firstVisible; i <= lastVisible && i >= 0 && i < (int)availableParameters.size(); ++i) {
            parameterListBox.repaintRow(i);
        }
        parameterListBox.repaint(); // Also repaint the whole component
        updateParameterList();
    } else if (button == &deselectAllButton) {
        selectedParameters.resize(availableParameters.size(), false);
        std::fill(selectedParameters.begin(), selectedParameters.end(), false);
        parameterListBox.updateContent();
        // Repaint all visible rows
        int firstVisible = parameterListBox.getRowContainingPosition(0, 0);
        int lastVisible = parameterListBox.getRowContainingPosition(0, parameterListBox.getHeight());
        for (int i = firstVisible; i <= lastVisible && i >= 0 && i < (int)availableParameters.size(); ++i) {
            parameterListBox.repaintRow(i);
        }
        parameterListBox.repaint(); // Also repaint the whole component
        updateParameterList();
    } else if (button == &runMeasurementButton) {
        runMeasurement();
    }
}

void MainComponent::textEditorTextChanged(juce::TextEditor& editor) {
    // Handle text changes if needed
}

int MainComponent::getNumRows() {
    return (int)availableParameters.size();
}

void MainComponent::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) {
    if (rowNumber < 0 || rowNumber >= (int)availableParameters.size())
        return;

    // Background - use subtle alternating colors, ignore rowIsSelected since we disabled selection
    g.fillAll(rowNumber % 2 == 0 ? juce::Colours::white : juce::Colours::lightgrey.withAlpha(0.3f));

    // Draw checkbox
    const int checkboxSize = 18;
    const int checkboxX = 5;
    const int checkboxY = (height - checkboxSize) / 2;
    juce::Rectangle<float> checkboxBounds((float)checkboxX, (float)checkboxY, (float)checkboxSize, (float)checkboxSize);

    bool isChecked = rowNumber < (int)selectedParameters.size() && selectedParameters[rowNumber];

    // Draw checkbox border
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(checkboxBounds, 1.5f);

    // Draw checkbox fill if checked
    if (isChecked) {
        g.setColour(juce::Colours::blue);
        g.fillRect(checkboxBounds.reduced(2.0f));

        // Draw checkmark using a simple path
        g.setColour(juce::Colours::white);
        juce::Path checkmark;
        const float x = checkboxBounds.getX();
        const float y = checkboxBounds.getY();
        const float w = checkboxBounds.getWidth();
        const float h = checkboxBounds.getHeight();

        // Draw checkmark as two connected lines
        checkmark.startNewSubPath(x + w * 0.2f, y + h * 0.5f);
        checkmark.lineTo(x + w * 0.45f, y + h * 0.75f);
        checkmark.lineTo(x + w * 0.8f, y + h * 0.25f);

        g.strokePath(checkmark, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved));
    }

    // Parameter name
    g.setColour(juce::Colours::black);
    g.setFont(14.0f);
    g.drawText(availableParameters[rowNumber], checkboxX + checkboxSize + 10, 0, width - checkboxX - checkboxSize - 10,
               height, juce::Justification::centredLeft);
}

void MainComponent::listBoxItemClicked(int row, const juce::MouseEvent& e) {
    // Ensure selectedParameters is the right size
    if (selectedParameters.size() != availableParameters.size()) {
        selectedParameters.resize(availableParameters.size(), false);
    }

    // Only toggle if clicking within the checkbox area or the row itself
    // This prevents accidental toggles when clicking elsewhere
    const int checkboxArea = 30; // Approximate checkbox area width

    // Toggle if clicking in the left part of the row (where checkbox is) or anywhere on the row
    if (row >= 0 && row < (int)selectedParameters.size() && row < (int)availableParameters.size()) {
        // Always toggle on row click - the checkbox visual will update
        selectedParameters[row] = !selectedParameters[row];
        parameterListBox.updateContent();
        parameterListBox.repaintRow(row); // Repaint the specific row that changed
        parameterListBox.repaint();       // Also repaint the whole component
        updateParameterList();
    }
}

void MainComponent::loadPlugin() {
    juce::String pluginPath = pluginPathEditor.getText();
    if (pluginPath.isEmpty()) {
        showError("Please specify a plugin path");
        return;
    }

    juce::File pluginFile(pluginPath);
    // VST3 plugins on macOS are bundles (directories), not files
    if (!pluginFile.exists()) {
        showError("Plugin file does not exist: " + pluginPath);
        return;
    }

    progressLabel.setText("Loading plugin...", juce::dontSendNotification);

    // Get sample rate and block size from measurement config
    Config tempConfig;
    measurementConfig->fillConfig(tempConfig);

    juce::String errorMessage;
    pluginInstance = loadPluginInstance(pluginFile, tempConfig.sampleRate, tempConfig.blockSize, errorMessage);

    if (pluginInstance == nullptr) {
        showError(errorMessage.isEmpty() ? "Failed to load plugin" : errorMessage);
        return;
    }

    pluginInfoLabel.setText("Loaded: " + pluginInstance->getName(), juce::dontSendNotification);
    scanPluginParameters();
    runMeasurementButton.setEnabled(true);
    progressLabel.setText("Plugin loaded successfully", juce::dontSendNotification);
}

void MainComponent::scanPluginParameters() {
    if (pluginInstance == nullptr)
        return;

    parameterMap = buildParameterMap(*pluginInstance, true); // Only UI-exposed parameters
    availableParameters.clear();
    selectedParameters.clear();

    for (const auto& [name, param] : parameterMap) {
        availableParameters.push_back(name);
        selectedParameters.push_back(false);
    }

    std::sort(availableParameters.begin(), availableParameters.end());
    parameterListBox.updateContent();
    updateParameterList();
}

void MainComponent::updateParameterList() {
    // Remove old config components
    parameterConfigComponents.clear();
    parameterConfigContainer.removeAllChildren();

    // Create config components for selected parameters
    int y = 10;
    int selectedCount = 0;
    std::cerr << "[updateParameterList] Processing " << availableParameters.size() << " available parameters"
              << std::endl;
    for (size_t i = 0; i < availableParameters.size(); ++i) {
        if (i < selectedParameters.size() && selectedParameters[i]) {
            selectedCount++;
            std::cerr << "[updateParameterList] Processing selected parameter #" << selectedCount << ": "
                      << availableParameters[i] << std::endl;

            // Find the actual parameter from the map
            juce::AudioProcessorParameter* param = nullptr;
            auto it = parameterMap.find(availableParameters[i]);
            if (it != parameterMap.end()) {
                param = it->second;
                std::cerr << "[updateParameterList] Found parameter: " << availableParameters[i]
                          << " (type: " << typeid(*param).name() << ")" << std::endl;
            } else {
                std::cerr << "[updateParameterList] WARNING: Parameter not found in map: " << availableParameters[i]
                          << std::endl;
                // Try to find by original name (case-insensitive search)
                for (const auto& [mapName, mapParam] : parameterMap) {
                    if (mapName.equalsIgnoreCase(availableParameters[i])) {
                        param = mapParam;
                        std::cerr << "[updateParameterList] Found by case-insensitive match: " << mapName << std::endl;
                        break;
                    }
                }
            }

            try {
                auto* comp = new ParameterConfigComponent(availableParameters[i], param);
                comp->setBounds(10, y, 480, 180);
                parameterConfigContainer.addAndMakeVisible(comp);
                parameterConfigComponents.push_back(std::unique_ptr<ParameterConfigComponent>(comp));
                y += 190;
                std::cerr << "[updateParameterList] Successfully created component for: " << availableParameters[i]
                          << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[updateParameterList] ERROR creating component for " << availableParameters[i] << ": "
                          << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[updateParameterList] ERROR creating component for " << availableParameters[i]
                          << ": Unknown exception" << std::endl;
            }
        }
    }
    std::cerr << "[updateParameterList] Finished processing. Created " << parameterConfigComponents.size()
              << " components" << std::endl;

    parameterConfigContainer.setSize(500, y);
    parameterConfigViewport.setViewPosition(0, 0);
}

void MainComponent::runMeasurement() {
    if (pluginInstance == nullptr) {
        showError("No plugin loaded");
        return;
    }

    // Count selected parameters
    int selectedCount = 0;
    for (bool selected : selectedParameters) {
        if (selected)
            selectedCount++;
    }

    if (selectedCount == 0) {
        showError("Please select at least one parameter");
        return;
    }

    juce::String outputPath = outputPathEditor.getText();
    if (outputPath.isEmpty()) {
        showError("Please specify an output path");
        return;
    }

    juce::File outDir(outputPath);
    if (!outDir.exists()) {
        outDir.createDirectory();
    }

    if (!outDir.isDirectory()) {
        showError("Output path is not a directory");
        return;
    }

    // Build config from UI
    Config config = buildConfigFromUI();
    config.pluginPath = pluginPathEditor.getText();

    try {
        config.validate();
    } catch (const std::exception& e) {
        showError(e.what());
        return;
    }

    // Run measurement in background thread
    runMeasurementButton.setEnabled(false);
    progressLabel.setText("Running measurement...", juce::dontSendNotification);

    std::thread([this, config, outDir]() {
        try {
            std::cerr << "[Measurement] Starting measurement thread..." << std::endl;
            std::cerr.flush(); // Ensure logs appear immediately

            // Create a separate plugin instance for the measurement thread
            // This is necessary because JUCE plugins should not be accessed from multiple threads
            std::cerr << "[Measurement] Creating plugin instance for measurement thread..." << std::endl;
            juce::File pluginFile(config.pluginPath);
            juce::String errorMessage;
            auto measurementPlugin = loadPluginInstance(pluginFile, config.sampleRate, config.blockSize, errorMessage);

            if (measurementPlugin == nullptr) {
                std::cerr << "[Measurement] Failed to create plugin instance: " << errorMessage << std::endl;
                juce::MessageManager::callAsync([this, errorMessage]() {
                    showError("Failed to create plugin instance for measurement: " + errorMessage);
                    runMeasurementButton.setEnabled(true);
                });
                return;
            }
            std::cerr << "[Measurement] Plugin instance created successfully" << std::endl;
            std::cerr.flush();

            // Build parameter name list
            std::vector<juce::String> paramNames;
            for (size_t i = 0; i < availableParameters.size(); ++i) {
                if (i < selectedParameters.size() && selectedParameters[i]) {
                    paramNames.push_back(availableParameters[i]);
                }
            }
            std::cerr << "[Measurement] Selected " << paramNames.size() << " parameters" << std::endl;
            std::cerr.flush();

            // Build run grid
            std::cerr << "[Measurement] Building run grid..." << std::endl;
            juce::MessageManager::callAsync(
                [this]() { progressLabel.setText("Building run grid...", juce::dontSendNotification); });
            auto runs = buildRunGrid(config, paramNames);
            std::cerr << "[Measurement] Generated " << runs.size() << " measurement runs" << std::endl;

            // Warn if too many runs and estimate time
            if (runs.size() > 100000) {
                std::cerr << "[Measurement] WARNING: " << runs.size()
                          << " runs is very large. This may take a long time." << std::endl;

                // Estimate time: assume ~0.1 seconds per run (very rough estimate)
                double estimatedSeconds = runs.size() * 0.1;
                double estimatedMinutes = estimatedSeconds / 60.0;
                double estimatedHours = estimatedMinutes / 60.0;

                juce::String timeEstimate;
                if (estimatedHours >= 1.0) {
                    timeEstimate = juce::String(estimatedHours, 1) + " hours";
                } else if (estimatedMinutes >= 1.0) {
                    timeEstimate = juce::String(estimatedMinutes, 1) + " minutes";
                } else {
                    timeEstimate = juce::String(estimatedSeconds, 1) + " seconds";
                }

                juce::MessageManager::callAsync([this, runs, timeEstimate]() {
                    progressLabel.setText("WARNING: " + juce::String(runs.size()) + " runs (~" + timeEstimate +
                                              ") - Consider reducing parameters/buckets!",
                                          juce::dontSendNotification);
                });
                std::this_thread::sleep_for(std::chrono::seconds(3)); // Give user time to see warning
            }
            std::cerr.flush();

            // Create analyzers
            std::cerr << "[Measurement] Creating analyzers..." << std::endl;
            auto analyzers = createAnalyzers(config, outDir, paramNames);
            std::cerr << "[Measurement] Created " << analyzers.size() << " analyzers" << std::endl;

            // Run measurements
            int64_t totalSamples = (int64_t)(config.seconds * config.sampleRate);
            std::cerr << "[Measurement] Starting measurement grid (" << runs.size() << " runs, " << totalSamples
                      << " samples per run)..." << std::endl;
            juce::MessageManager::callAsync([this, runs]() {
                progressLabel.setText("Running " + juce::String(runs.size()) + " measurements...",
                                      juce::dontSendNotification);
            });

            // Pass progress callback to update UI with time estimate
            auto startTime = std::chrono::steady_clock::now();
            int numThreads = (int)std::thread::hardware_concurrency();
            if (numThreads == 0)
                numThreads = 1; // Fallback if detection fails
            runMeasurementGrid(
                *measurementPlugin, config.sampleRate, config.blockSize, totalSamples, runs, analyzers, config, outDir,
                [this, runs, startTime](int runIndex) {
                    double progress = (double)(runIndex + 1) / (double)runs.size();
                    auto currentTime = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

                    juce::String statusText = "Run " + juce::String(runIndex + 1) + " / " + juce::String(runs.size());

                    // Estimate remaining time
                    if (runIndex > 0 && elapsed > 0) {
                        double runsPerSecond = (double)(runIndex + 1) / (double)elapsed;
                        int remainingRuns = runs.size() - (runIndex + 1);
                        int estimatedSecondsRemaining = (int)(remainingRuns / runsPerSecond);

                        int hours = estimatedSecondsRemaining / 3600;
                        int minutes = (estimatedSecondsRemaining % 3600) / 60;
                        int seconds = estimatedSecondsRemaining % 60;

                        if (hours > 0) {
                            statusText += " (~" + juce::String(hours) + "h " + juce::String(minutes) + "m remaining)";
                        } else if (minutes > 0) {
                            statusText += " (~" + juce::String(minutes) + "m " + juce::String(seconds) + "s remaining)";
                        } else {
                            statusText += " (~" + juce::String(seconds) + "s remaining)";
                        }
                    }

                    juce::MessageManager::callAsync([this, statusText, progress]() {
                        progressLabel.setText(statusText, juce::dontSendNotification);
                        this->progress = progress;
                        progressBar.repaint();
                    });
                },
                numThreads);
            std::cerr << "[Measurement] Measurement grid complete" << std::endl;

            // (runMeasurementGrid finishes the analyzers and writes their output)

            juce::MessageManager::callAsync([this]() {
                progressLabel.setText("Measurement complete!", juce::dontSendNotification);
                progress = 1.0;
                progressBar.repaint();
                runMeasurementButton.setEnabled(true);
            });
        } catch (const std::exception& e) {
            std::cerr << "[Measurement] Exception: " << e.what() << std::endl;
            juce::MessageManager::callAsync([this, e]() {
                showError("Error: " + juce::String(e.what()));
                runMeasurementButton.setEnabled(true);
            });
        } catch (...) {
            std::cerr << "[Measurement] Unknown exception occurred" << std::endl;
            juce::MessageManager::callAsync([this]() {
                showError("Unknown error occurred during measurement");
                runMeasurementButton.setEnabled(true);
            });
        }
    }).detach();
}

Config MainComponent::buildConfigFromUI() {
    Config config;

    // Fill measurement config
    measurementConfig->fillConfig(config);

    // Add parameter buckets from selected parameters
    for (size_t i = 0; i < availableParameters.size(); ++i) {
        if (i < selectedParameters.size() && selectedParameters[i]) {
            // Find corresponding config component
            for (const auto& comp : parameterConfigComponents) {
                if (comp->getConfig().paramName == availableParameters[i]) {
                    config.parameterBuckets.push_back(comp->getConfig());
                    break;
                }
            }
        }
    }

    return config;
}

void MainComponent::showError(const juce::String& message) {
    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Error", message);
    progressLabel.setText(message, juce::dontSendNotification);
}

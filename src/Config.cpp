#include "Config.h"
#include <cmath>
#include <fstream>
#include <sstream>

Config Config::fromJson(const juce::File& jsonFile) {
    if (!jsonFile.existsAsFile()) {
        throw std::runtime_error("Config file does not exist: " + jsonFile.getFullPathName().toStdString());
    }

    juce::String jsonContent = jsonFile.loadFileAsString();
    return fromJsonString(jsonContent);
}

Config Config::fromJsonString(const juce::String& jsonString) {
    Config config;

    auto json = juce::JSON::parse(jsonString);
    if (!json.isObject()) {
        throw std::runtime_error("Invalid JSON: root must be an object");
    }

    auto root = json.getDynamicObject();

    // Plugin path
    if (root->hasProperty("pluginPath"))
        config.pluginPath = root->getProperty("pluginPath").toString();

    // Audio settings
    if (root->hasProperty("sampleRate"))
        config.sampleRate = (double)root->getProperty("sampleRate");
    if (root->hasProperty("seconds"))
        config.seconds = (double)root->getProperty("seconds");
    if (root->hasProperty("preRollSeconds")) {
        config.preRollSeconds = (double)root->getProperty("preRollSeconds");
        if (!std::isfinite(config.preRollSeconds) || config.preRollSeconds < 0.0 || config.preRollSeconds > 60.0)
            throw std::runtime_error("Invalid preRollSeconds " + std::to_string(config.preRollSeconds) +
                                     " (expected 0-60)");
    }
    if (root->hasProperty("blockSize"))
        config.blockSize = (int)root->getProperty("blockSize");

    // Signal settings
    if (root->hasProperty("signalType"))
        config.signalType = root->getProperty("signalType").toString();
    if (root->hasProperty("sineFrequency"))
        config.sineFrequency = (double)root->getProperty("sineFrequency");
    if (root->hasProperty("sweepStartHz"))
        config.sweepStartHz = (double)root->getProperty("sweepStartHz");
    if (root->hasProperty("sweepEndHz"))
        config.sweepEndHz = (double)root->getProperty("sweepEndHz");

    // Input gain buckets
    if (root->hasProperty("inputGainBucketsDb")) {
        auto gainArray = root->getProperty("inputGainBucketsDb");
        if (gainArray.isArray()) {
            for (int i = 0; i < gainArray.size(); ++i) {
                config.inputGainBucketsDb.push_back((float)gainArray[i]);
            }
        }
    }

    // Parameter buckets
    if (root->hasProperty("parameterBuckets")) {
        auto bucketsArray = root->getProperty("parameterBuckets");
        if (bucketsArray.isArray()) {
            for (int i = 0; i < bucketsArray.size(); ++i) {
                auto bucketObj = bucketsArray[i].getDynamicObject();
                if (bucketObj == nullptr)
                    continue;

                ParameterBucketConfig bucket;
                if (bucketObj->hasProperty("paramName"))
                    bucket.paramName = bucketObj->getProperty("paramName").toString();
                if (bucketObj->hasProperty("strategy"))
                    bucket.strategy = bucketObj->getProperty("strategy").toString();
                if (bucketObj->hasProperty("min"))
                    bucket.min = (float)bucketObj->getProperty("min");
                if (bucketObj->hasProperty("max"))
                    bucket.max = (float)bucketObj->getProperty("max");
                if (bucketObj->hasProperty("numBuckets"))
                    bucket.numBuckets = (int)bucketObj->getProperty("numBuckets");
                if (bucketObj->hasProperty("values")) {
                    auto valuesArray = bucketObj->getProperty("values");
                    if (valuesArray.isArray()) {
                        for (int j = 0; j < valuesArray.size(); ++j) {
                            bucket.values.push_back((float)valuesArray[j]);
                        }
                    }
                }

                config.parameterBuckets.push_back(bucket);
            }
        }
    }

    // Analyzers
    if (root->hasProperty("analyzers")) {
        auto analyzersArray = root->getProperty("analyzers");
        if (analyzersArray.isArray()) {
            for (int i = 0; i < analyzersArray.size(); ++i) {
                config.analyzers.push_back(analyzersArray[i].toString());
            }
        }
    }

    return config;
}

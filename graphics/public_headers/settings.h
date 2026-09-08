#pragma once
//
// Created by pr on 24.07.25.
//

#ifndef SETTINGS_H
#define SETTINGS_H

#include <filesystem> // Include the necessary header for std::filesystem
#include <string>     // Include the necessary header for std::string
#include <cstdlib>    // Include the necessary header for getenv
#include <memory>     // Include the necessary header for free (on Windows)
#include <fstream>
#include <map>
#include <sstream>
#include <functional>

#include "logger.h"

namespace fs = std::filesystem;



namespace tsl::settings {
    class SettingsManager {
    public:
        SettingsManager() = delete;
        // Define the callback type
        using IntChangeCallback = std::function<void(const std::string& key, int value)>;

    private:
        std::map<std::string, std::string> settingsMap;
        std::string filePath;
        IntChangeCallback intChangeCallback; // Store the callback

        std::string settingsPath;

        void loadSettings() {
            filePath = settingsPath + "/"  + "settings.conf";
            std::ifstream file(filePath);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    size_t delimiterPos = line.find('=');
                    if (delimiterPos != std::string::npos) {
                        std::string key = line.substr(0, delimiterPos);
                        std::string value = line.substr(delimiterPos + 1);
                        settingsMap[key] = value;
                    }
                }
                file.close();
            }
        }

        void saveSettings() {
            std::ofstream file(filePath);
            if (file.is_open()) {
                for (const auto& pair : settingsMap) {
                    file << pair.first << "=" << pair.second << std::endl;
                }
                file.close();
            }
        }

    public:
        SettingsManager(const std::string& settingsFolder) : settingsPath(settingsFolder) {
            loadSettings();
        }

        // --- Get methods ---
        std::string Get(const std::string& key, const std::string& defaultValue) {
            auto it = settingsMap.find(key);
            if (it != settingsMap.end()) {
                return it->second;
            }
            return defaultValue;
        }

        int Get(const std::string& key, int defaultValue) {
            auto it = settingsMap.find(key);
            if (it != settingsMap.end()) {
                int value;
                std::istringstream iss(it->second);
                iss >> value;
                // Check if parsing succeeded and consumed the entire string
                if (!iss.fail() && iss.eof()) {
                    return value;
                }
                // Log error if conversion fails (optional)
                // fprintf(stderr, "Warning: Failed to convert setting '%s' value '%s' to int. Using default '%d'.\n",
                //         key.c_str(), it->second.c_str(), defaultValue);
            }
            return defaultValue;
        }

        float Get(const std::string& key, float defaultValue) {
            auto it = settingsMap.find(key);
            if (it != settingsMap.end()) {
                float value;
                std::istringstream iss(it->second);
                iss >> value;
                // Check if parsing succeeded and consumed the entire string
                if (!iss.fail() && iss.eof()) {
                    return value;
                }
                // Log error if conversion fails (optional)
                // fprintf(stderr, "Warning: Failed to convert setting '%s' value '%s' to float. Using default '%f'.\n",
                //         key.c_str(), it->second.c_str(), defaultValue);
            }
            return defaultValue;
        }

        bool Get(const std::string& key, bool defaultValue) {
            auto it = settingsMap.find(key);
            if (it != settingsMap.end()) {
                return it->second == "true"; // Simple string comparison for bool
            }
            return defaultValue;
        }

        // --- Set methods ---
        void Set(const std::string& key, const std::string& value) {
            settingsMap[key] = value;
            saveSettings();
        }

        void Set(const std::string& key, int value) {
            settingsMap[key] = std::to_string(value);
            saveSettings();
            // Invoke the callback if it's set
            if (intChangeCallback) {
                intChangeCallback(key, value);
            }
        }

        void Set(const std::string& key, float value) {
            settingsMap[key] = std::to_string(value);
            saveSettings();
        }

        void Set(const std::string& key, bool value) {
            settingsMap[key] = value ? "true" : "false";
            saveSettings();
            if (intChangeCallback) {
                intChangeCallback(key, value ?  1 : 0);
            }
        }

        // Method to set the int change callback
        void SetIntChangeCallback(IntChangeCallback callback) {
            intChangeCallback = std::move(callback);
        }
    };
}

#endif
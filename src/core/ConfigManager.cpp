/*
 * ConfigManager.cpp
 *
 * Реализация для ConfigManager.
 * Содержит логику парсинга INI-подобных файлов конфигурации.
 *
 * Соответствует: docs/modules/core_config_manager.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.4
 */
#include "core/ConfigManager.h"
#include <sstream>
#include <algorithm>
#include <iterator>
// (Включаем iostream для отладки в native, если нужно, но в prod коде лучше избегать)

// --- Вспомогательные функции (static) ---

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (std::string::npos == first) {
        return "";
    }
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

// --- Конструктор ---

ConfigManager::ConfigManager() {
    loadDefaults();
}

// --- Public API ---

bool ConfigManager::init(IHalStorage* storage) {
    std::string content;
    if (storage->readFile("/settings.cfg", content)) {
        parseConfig(content);
        return true;
    } else {
        // Файл не найден, defaults уже загружены в конструкторе
        return false;
    }
}

// --- Геттеры ---

LogLevel ConfigManager::getLogLevel() const { return m_logLevel; }
int ConfigManager::getAutoOffTimeMin() const { return m_autoOffTimeMin; }
std::string ConfigManager::getLedPin() const { return m_ledPin; }
float ConfigManager::getBasePitchHz() const { return m_basePitchHz; }

int ConfigManager::getLedBlinkDurationMs() const { return m_ledBlinkDurationMs; }
int ConfigManager::getLedBlinkPauseMs() const { return m_ledBlinkPauseMs; }

const std::vector<std::string>& ConfigManager::getPhysicalPins() const { return m_physicalPins; }
int ConfigManager::getSampleRateHz() const { return m_sampleRateHz; }
float ConfigManager::getFilterAlpha() const { return m_filterAlpha; }
int ConfigManager::getMuteThreshold() const { return m_muteThreshold; }
int ConfigManager::getHoleClosedThreshold() const { return m_holeClosedThreshold; }

int ConfigManager::getMuteSensorId() const { return m_muteSensorId; }
const std::vector<int>& ConfigManager::getHoleSensorIds() const { return m_holeSensorIds; }

float ConfigManager::getVibratoFreqMin() const { return m_vibratoFreqMin; }
float ConfigManager::getVibratoFreqMax() const { return m_vibratoFreqMax; }
int ConfigManager::getVibratoAmplitudeMin() const { return m_vibratoAmplitudeMin; }
int ConfigManager::getHalfHoleThreshold() const { return m_halfHoleThreshold; }


// --- Приватные методы ---

void ConfigManager::loadDefaults() {
    // [system]
    m_logLevel = LogLevel::WARN;
    m_autoOffTimeMin = 10;
    m_ledPin = "TBD";
    m_basePitchHz = 440.0f;

    // [led]
    m_ledBlinkDurationMs = 50;
    m_ledBlinkPauseMs = 50;

    // [sensors]
    m_physicalPins = {"T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9"};
    m_sampleRateHz = 50;
    m_filterAlpha = 0.1f;
    m_muteThreshold = 500;
    m_holeClosedThreshold = 400;

    // [app_logic]
    m_muteSensorId = 8;
    m_holeSensorIds = {0, 1, 2, 3, 4, 5, 6, 7};

    // [gestures]
    m_vibratoFreqMin = 2.0f;
    m_vibratoFreqMax = 6.0f;
    m_vibratoAmplitudeMin = 50;
    m_halfHoleThreshold = 300;
}

void ConfigManager::parseConfig(const std::string& fileContent) {
    std::istringstream stream(fileContent);
    std::string line;

    while (std::getline(stream, line)) {
        // 1. Убираем пробелы и комментарии
        std::string cleanLine = trim(line);
        if (cleanLine.empty() || cleanLine[0] == '#' || cleanLine[0] == ';') {
            continue;
        }

        // 2. Пропускаем секции [section] (ключи уникальны, секции пока не важны)
        if (cleanLine[0] == '[' && cleanLine.back() == ']') {
            continue;
        }

        // 3. Парсинг key = value
        size_t eqPos = cleanLine.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(cleanLine.substr(0, eqPos));
        std::string value = trim(cleanLine.substr(eqPos + 1));

        // Удаляем возможный комментарий в конце строки
        size_t commentPos = value.find('#');
        if (commentPos != std::string::npos) {
            value = trim(value.substr(0, commentPos));
        }

        // 4. Присвоение значений
        try {
            // --- [system] ---
            if (key == "log_level") {
                if (value == "DEBUG") m_logLevel = LogLevel::DEBUG;
                else if (value == "INFO") m_logLevel = LogLevel::INFO;
                else if (value == "WARN") m_logLevel = LogLevel::WARN;
                else if (value == "ERROR") m_logLevel = LogLevel::ERROR;
            }
            else if (key == "auto_off_time_min") m_autoOffTimeMin = std::stoi(value);
            else if (key == "led_pin") m_ledPin = value;
            else if (key == "base_pitch_hz") m_basePitchHz = std::stof(value);

            // --- [led] ---
            else if (key == "blink_duration_ms") m_ledBlinkDurationMs = std::stoi(value);
            else if (key == "blink_pause_ms") m_ledBlinkPauseMs = std::stoi(value);

            // --- [sensors] ---
            else if (key == "physical_pins") m_physicalPins = split(value, ',');
            else if (key == "sample_rate_hz") m_sampleRateHz = std::stoi(value);
            else if (key == "filter_alpha") m_filterAlpha = std::stof(value);
            else if (key == "mute_threshold") m_muteThreshold = std::stoi(value);
            else if (key == "hole_closed_threshold") m_holeClosedThreshold = std::stoi(value);

            // --- [app_logic] ---
            else if (key == "mute_sensor_id") m_muteSensorId = std::stoi(value);
            else if (key == "hole_sensor_ids") {
                std::vector<std::string> strIds = split(value, ',');
                m_holeSensorIds.clear();
                for (const auto& s : strIds) m_holeSensorIds.push_back(std::stoi(s));
            }

            // --- [gestures] ---
            else if (key == "vibrato_freq_min_hz") m_vibratoFreqMin = std::stof(value);
            else if (key == "vibrato_freq_max_hz") m_vibratoFreqMax = std::stof(value);
            else if (key == "vibrato_amplitude_min") m_vibratoAmplitudeMin = std::stoi(value);
            else if (key == "half_hole_threshold") m_halfHoleThreshold = std::stoi(value);

        } catch (...) {
            // Игнорируем ошибки конвертации
        }
    }
}
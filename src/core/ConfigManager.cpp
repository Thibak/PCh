/*
 * ConfigManager.cpp
 *
 * Реализация (Заглушка) для ConfigManager
 *
 * Соответствует: docs/modules/core_config_manager.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.2
 */
#include "core/ConfigManager.h"
#include "core/Logger.h" // (Для LOG_INFO и т.д.)

#define TAG "ConfigManager"

// --- Конструктор ---
ConfigManager::ConfigManager() {
    // Инициализируем значения по умолчанию (на случай, если loadDefaults() 
    // будет вызван до init() или если init() не найдет файл)
    loadDefaults();
}

// --- Public API ---

bool ConfigManager::init(IHalStorage* storage) {
    // (TBD в Спринте 2.4: Реализовать логику чтения и парсинга)
    // LOG_INFO(TAG, "Initializing...");
    
    // Пока просто используем defaults
    loadDefaults();
    return true;
}

// --- Геттеры (возвращают значения по умолчанию) ---

LogLevel ConfigManager::getLogLevel() const {
    return m_logLevel;
}

int ConfigManager::getAutoOffTimeMin() const {
    return m_autoOffTimeMin;
}

std::string ConfigManager::getLedPin() const {
    return m_ledPin;
}

float ConfigManager::getBasePitchHz() const {
    return m_basePitchHz;
}

int ConfigManager::getLedBlinkDurationMs() const {
    return m_ledBlinkDurationMs;
}

int ConfigManager::getLedBlinkPauseMs() const {
    return m_ledBlinkPauseMs;
}

const std::vector<std::string>& ConfigManager::getPhysicalPins() const {
    return m_physicalPins;
}

int ConfigManager::getSampleRateHz() const {
    return m_sampleRateHz;
}

float ConfigManager::getFilterAlpha() const {
    return m_filterAlpha;
}

int ConfigManager::getMuteThreshold() const {
    return m_muteThreshold;
}

int ConfigManager::getHoleClosedThreshold() const {
    return m_holeClosedThreshold;
}

int ConfigManager::getMuteSensorId() const {
    return m_muteSensorId;
}

const std::vector<int>& ConfigManager::getHoleSensorIds() const {
    return m_holeSensorIds;
}

float ConfigManager::getVibratoFreqMin() const {
    return m_vibratoFreqMin;
}

float ConfigManager::getVibratoFreqMax() const {
    return m_vibratoFreqMax;
}

int ConfigManager::getVibratoAmplitudeMin() const {
    return m_vibratoAmplitudeMin;
}

int ConfigManager::getHalfHoleThreshold() const {
    return m_halfHoleThreshold;
}

// --- Приватные методы ---

void ConfigManager::parseConfig(const std::string& fileContent) {
    // (TBD в Спринте 2.4)
}

void ConfigManager::loadDefaults() {
    // (Загружаем значения по умолчанию, как описано в CONFIG_SCHEMA.md)
    
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
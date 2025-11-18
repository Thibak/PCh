/*
 * ConfigManager.h
 *
 * Соответствует: docs/modules/core_config_manager.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.1
 */
#pragma once

#include <vector>
#include <string>
#include "interfaces/IHalStorage.h" // Для init()
#include "LogLevel.h"

class ConfigManager {
public:
    ConfigManager();
    
    /**
     * @brief Инициализирует менеджер, читая конфиг из хранилища.
     * @param storage Указатель на реализацию i_hal_storage.
     * @return true, если конфиг успешно загружен и распарсен (или если успешно загружены defaults).
     */
    bool init(IHalStorage* storage);

    // --- [system] ---
    LogLevel getLogLevel() const;
    int getAutoOffTimeMin() const;
    std::string getLedPin() const;
    float getBasePitchHz() const;

    // --- [led] ---
    int getLedBlinkDurationMs() const;
    int getLedBlinkPauseMs() const;

    // --- [sensors] ---
    const std::vector<std::string>& getPhysicalPins() const;
    int getSampleRateHz() const;
    float getFilterAlpha() const;
    int getMuteThreshold() const;
    int getHoleClosedThreshold() const;

    // --- [app_logic] ---
    int getMuteSensorId() const;
    const std::vector<int>& getHoleSensorIds() const;
    
    // --- [gestures] ---
    float getVibratoFreqMin() const;
    float getVibratoFreqMax() const;
    int getVibratoAmplitudeMin() const;
    int getHalfHoleThreshold() const;

private:
    /**
     * @brief Внутренний метод парсинга.
     */
    void parseConfig(const std::string& fileContent);
    
    /**
     * @brief Загружает жестко заданные defaults, если parseConfig не удался.
     */
    void loadDefaults();

    // Приватные поля для хранения распарсенных значений
    LogLevel m_logLevel;
    int m_autoOffTimeMin;
    std::string m_ledPin;
    float m_basePitchHz;
    
    int m_ledBlinkDurationMs;
    int m_ledBlinkPauseMs;

    std::vector<std::string> m_physicalPins;
    int m_sampleRateHz;
    float m_filterAlpha;
    int m_muteThreshold;
    int m_holeClosedThreshold;
    int m_muteSensorId;
    std::vector<int> m_holeSensorIds;
    float m_vibratoFreqMin;
    float m_vibratoFreqMax;
    int m_vibratoAmplitudeMin;
    int m_halfHoleThreshold;
};
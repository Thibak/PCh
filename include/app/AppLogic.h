/*
 * AppLogic.h
 *
 * Соответствует: docs/modules/app_logic.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.10
 */
#pragma once

#include "core/ConfigManager.h"
#include "core/EventDispatcher.h"
#include "interfaces/IEventHandler.h"
#include <vector>
#include <cstdint>

// Forward-declare FreeRTOS типы
typedef void* QueueHandle_t;

// (Определение SensorState и SensorContext)
enum class SensorState { OPEN, HALF_HOLE, CLOSED };

struct SensorContext {
    SensorState state;
    // (Буфер истории значений для анализа вибрато)
    std::vector<int> valueHistory; 
    // (Другие переменные для DSP...)

    SensorContext() : state(SensorState::OPEN) {}
};


class AppLogic : public IEventHandler {
public:
    AppLogic();
    
    /**
     * @brief Загружает все пороги и ID из ConfigManager.
     */
    bool init(ConfigManager* configManager, EventDispatcher* dispatcher);

    /**
     * @brief Запускает задачу FreeRTOS `appLogicTask`.
     */
    void startTask();

    /**
     * @brief Подписывает модуль на SENSOR_VALUE_CHANGED.
     */
    void subscribe(EventDispatcher* dispatcher);

    /**
     * @brief БЫСТРЫЙ обработчик. Только кладет событие во внутреннюю очередь.
     */
    virtual void handleEvent(const Event& event) override;

private:
    /**
     * @brief Статическая обертка для задачи FreeRTOS.
     */
    static void appLogicTask(void* params);
    
    /**
     * @brief "Медленный" цикл, выполняющий всю логику (DSP, FSM).
     */
    void runAppLogicTask();
    
    // Объявление метода обработки
    void processSensorEvent(const Event& event);

    void updateMaskAndPublish();

    /**
     * @brief Реализация алгоритма детекции вибрато (Zero-Crossing).
     * @return float Глубина вибрато (0.0 - 1.0). Если 0.0 - вибрато нет.
     */
    float analyzeVibrato(const std::vector<int>& history);

    EventDispatcher* m_dispatcher;
    ConfigManager* m_configManager;

    // Внутренняя очередь для буферизации событий от hal_sensors
    QueueHandle_t m_sensorQueue;

    // --- Параметры из ConfigManager ---
    int m_muteSensorId;
    std::vector<int> m_holeSensorIds;
    int m_muteThreshold;
    int m_holeClosedThreshold;
    int m_halfHoleThreshold;
    // (и параметры вибрато)
    float m_vibratoFreqMin;
    float m_vibratoFreqMax;
    int m_vibratoAmplitudeMin;

    // --- Состояние (State) ---
    bool m_isMuted;
    uint8_t m_currentMask; // 8-битная маска (только CLOSED)
    SensorContext m_sensorContexts[16]; // Макс. 16 сенсоров
};
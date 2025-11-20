/*
 * AppLogic.cpp
 *
 * Реализация "мозга" устройства.
 * Обрабатывает сырые данные сенсоров, детектирует жесты и управляет состоянием.
 *
 * Соответствует: docs/modules/app_logic.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.10
 */
#include "app/AppLogic.h"
#include "core/Logger.h"
#include <iostream> // Для отладки в Native
#include <numeric>  // Для std::accumulate

#define TAG "AppLogic"

// --- Настройка окружения ---
#if defined(ESP32_TARGET)
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
#elif defined(NATIVE_TEST)
    // Заглушки для тестов
    typedef void* QueueHandle_t;
    #define xQueueCreate(len, size) ((void*)1)
    #define xQueueSend(queue, item, ticks) (true)
    #define xQueueReceive(queue, item, ticks) (true)
    #define xTaskCreate(task, name, stack, params, prio, handle) (pdPASS)
    #define vTaskDelay(ticks) ((void)0)
    #define pdPASS 1
    #define portMAX_DELAY 0
#endif

AppLogic::AppLogic() 
    : m_dispatcher(nullptr), 
      m_configManager(nullptr),
      m_sensorQueue(nullptr),
      m_isMuted(false),
      m_currentMask(0) {
}

bool AppLogic::init(ConfigManager* configManager, EventDispatcher* dispatcher) {
    m_configManager = configManager;
    m_dispatcher = dispatcher;
    
    m_muteSensorId = m_configManager->getMuteSensorId();
    m_holeSensorIds = m_configManager->getHoleSensorIds();
    m_muteThreshold = m_configManager->getMuteThreshold();
    m_holeClosedThreshold = m_configManager->getHoleClosedThreshold();
    m_halfHoleThreshold = m_configManager->getHalfHoleThreshold();

    m_vibratoFreqMin = m_configManager->getVibratoFreqMin();
    m_vibratoFreqMax = m_configManager->getVibratoFreqMax();
    m_vibratoAmplitudeMin = m_configManager->getVibratoAmplitudeMin();

    #if defined(ESP32_TARGET)
    m_sensorQueue = xQueueCreate(20, sizeof(Event));
    if (m_sensorQueue == nullptr) {
        LOG_ERROR(TAG, "Failed to create queue");
        return false;
    }
    #else
    m_sensorQueue = (void*)1; // Fake handle for tests
    #endif
    
    return true;
}

void AppLogic::startTask() {
    #if defined(ESP32_TARGET)
    xTaskCreate(appLogicTask, "appLogicTask", 4096, this, 5, nullptr);
    #endif
}

void AppLogic::subscribe(EventDispatcher* dispatcher) {
    if (dispatcher) {
        dispatcher->subscribe(EventType::SENSOR_VALUE_CHANGED, this);
    }
}

void AppLogic::handleEvent(const Event& event) {
    #if defined(NATIVE_TEST)
        if (event.type == EventType::SENSOR_VALUE_CHANGED) {
             processSensorEvent(event);
        }
    #else
        if (m_sensorQueue) {
            xQueueSend(m_sensorQueue, &event, 0);
        }
    #endif
}

// --- Приватные методы ---

void AppLogic::appLogicTask(void* params) {
    auto* instance = static_cast<AppLogic*>(params);
    instance->runAppLogicTask();
}

void AppLogic::runAppLogicTask() {
    #if defined(ESP32_TARGET)
    Event event(EventType::BLE_CONNECTED); 
    while(true) {
        if (xQueueReceive(m_sensorQueue, &event, portMAX_DELAY) == pdPASS) {
            processSensorEvent(event);
        }
    }
    #endif
}

void AppLogic::processSensorEvent(const Event& event) {
    if (event.type != EventType::SENSOR_VALUE_CHANGED) return;

    int id = event.payload.sensorValue.id;
    int value = event.payload.sensorValue.value;

    // 1. Mute
    if (id == m_muteSensorId) {
        bool newMuteState = (value > m_muteThreshold);
        if (newMuteState != m_isMuted) {
            m_isMuted = newMuteState;
            Event ev(newMuteState ? EventType::MUTE_ENABLED : EventType::MUTE_DISABLED);
            m_dispatcher->postEvent(ev);
        }
        return; 
    }

    // 2. Игровые сенсоры
    int bitIndex = -1;
    for (size_t i = 0; i < m_holeSensorIds.size(); ++i) {
        if (m_holeSensorIds[i] == id) {
            bitIndex = i;
            break;
        }
    }

    if (bitIndex != -1 && id < 16) {
        SensorContext& ctx = m_sensorContexts[id]; 
        SensorState oldState = ctx.state;
        SensorState newState = oldState;

        // --- СБОР ИСТОРИИ ДЛЯ ВИБРАТО ---
        ctx.valueHistory.push_back(value);
        
        // Ограничиваем размер истории (~1 секунда)
        // SampleRateHz обычно 50
        int maxHistorySize = m_configManager->getSampleRateHz(); 
        if (maxHistorySize <= 0) maxHistorySize = 50; // Защита

        if (ctx.valueHistory.size() > (size_t)maxHistorySize) {
            ctx.valueHistory.erase(ctx.valueHistory.begin());
        }

        // --- АНАЛИЗ ВИБРАТО ---
        // Запускаем анализ, только если набрали достаточно данных (например, полсекунды)
        if (ctx.valueHistory.size() >= (size_t)(maxHistorySize / 2)) {
            float vibratoDepth = analyzeVibrato(ctx.valueHistory);
            if (vibratoDepth > 0.0f) {
                Event ev(EventType::VIBRATO_DETECTED, VibratoPayload{id, vibratoDepth});
                m_dispatcher->postEvent(ev);
                
                #if defined(NATIVE_TEST)
                std::cout << "[AppLogic] Vibrato Detected! Depth: " << vibratoDepth << std::endl;
                #endif
            }
        }

        // --- ЛОГИКА ПОРОГОВ ---
        if (value > m_holeClosedThreshold) {
            newState = SensorState::CLOSED;
        } else if (value > m_halfHoleThreshold) {
            newState = SensorState::HALF_HOLE;
        } else {
            newState = SensorState::OPEN;
        }

        if (newState != oldState) {
            ctx.state = newState;

            if (newState == SensorState::HALF_HOLE) {
                Event ev(EventType::HALF_HOLE_DETECTED, HalfHolePayload{id});
                m_dispatcher->postEvent(ev);
            }

            updateMaskAndPublish();
        }
    }
}

void AppLogic::updateMaskAndPublish() {
    uint8_t newMask = 0;
    
    for (size_t i = 0; i < m_holeSensorIds.size(); ++i) {
        int id = m_holeSensorIds[i];
        if (id < 16 && m_sensorContexts[id].state == SensorState::CLOSED) {
            newMask |= (1 << i); 
        }
    }

    if (newMask != m_currentMask) {
        m_currentMask = newMask;
        Event ev(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{newMask});
        m_dispatcher->postEvent(ev);
    }
}

/**
 * @brief Алгоритм Zero-Crossing для детекции частоты вибрато.
 */
float AppLogic::analyzeVibrato(const std::vector<int>& history) {
    if (history.empty()) return 0.0f;

    // 1. Находим Min/Max (Амплитуду)
    int minVal = 4096;
    int maxVal = 0;
    long sum = 0;

    for (int v : history) {
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
        sum += v;
    }

    int amplitude = maxVal - minVal;

    // Если амплитуда слишком мала, это шум -> не вибрато
    if (amplitude < m_vibratoAmplitudeMin) {
        return 0.0f;
    }

    // 2. Считаем среднее (DC offset)
    int avg = sum / history.size();

    // 3. Считаем пересечения среднего (Zero Crossings)
    int crossings = 0;
    bool above = (history[0] > avg);

    for (size_t i = 1; i < history.size(); ++i) {
        bool nowAbove = (history[i] > avg);
        if (nowAbove != above) {
            crossings++;
            above = nowAbove;
        }
    }

    // 4. Вычисляем частоту
    // Freq = (Crossings / 2.0) / Duration
    // Duration = HistorySize / SampleRate
    int sampleRate = m_configManager->getSampleRateHz();
    float durationSec = (float)history.size() / (float)sampleRate;
    
    if (durationSec < 0.1f) return 0.0f; // Защита от деления на ноль

    float freq = ((float)crossings / 2.0f) / durationSec;

    // 5. Проверяем диапазон (2-6 Hz)
    if (freq >= m_vibratoFreqMin && freq <= m_vibratoFreqMax) {
        // Вибрато обнаружено!
        // Вычисляем нормализованную глубину (0.0 - 1.0)
        // Допустим, максимальная амплитуда вибрато ~200-300 "попугаев".
        // Можно просто возвращать амплитуду, масштабированную к MIDI PitchBend.
        // Пусть 1.0 = амплитуда 500.
        float depth = (float)amplitude / 500.0f;
        if (depth > 1.0f) depth = 1.0f;
        return depth;
    }

    return 0.0f;
}
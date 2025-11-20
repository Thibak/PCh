/*
 * AppLogic.cpp
 *
 * Реализация "мозга" устройства.
 *
 * Соответствует: docs/modules/app_logic.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.7
 */
#include "app/AppLogic.h"
#include "core/Logger.h"
#include <iostream> // Для отладки в Native

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
    
    // Загружаем параметры из конфига
    m_muteSensorId = m_configManager->getMuteSensorId();
    m_holeSensorIds = m_configManager->getHoleSensorIds();
    m_muteThreshold = m_configManager->getMuteThreshold();
    m_holeClosedThreshold = m_configManager->getHoleClosedThreshold();
    m_halfHoleThreshold = m_configManager->getHalfHoleThreshold();

    // Параметры вибрато (пока не используются в Спринте 2.7)
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
    // В Native режиме (синхронном) мы вызываем логику напрямую,
    // чтобы тесты были детерминированными и простыми.
    #if defined(NATIVE_TEST)
        // Прямой вызов обработки (симуляция извлечения из очереди)
        if (event.type == EventType::SENSOR_VALUE_CHANGED) {
             // В реальной задаче мы бы делали это в цикле, здесь - один раз
             processSensorEvent(event);
        }
    #else
        // В RTOS - отправляем в очередь
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
    Event event(EventType::BLE_CONNECTED); // Placeholder
    while(true) {
        if (xQueueReceive(m_sensorQueue, &event, portMAX_DELAY) == pdPASS) {
            processSensorEvent(event);
        }
    }
    #endif
}

/**
 * @brief Основная логика обработки сенсора (вынесена для удобства тестирования)
 */
void AppLogic::processSensorEvent(const Event& event) {
    if (event.type != EventType::SENSOR_VALUE_CHANGED) return;

    int id = event.payload.sensorValue.id;
    int value = event.payload.sensorValue.value;

    // 1. Сценарий Mute
    if (id == m_muteSensorId) {
        bool newMuteState = (value > m_muteThreshold);
        if (newMuteState != m_isMuted) {
            m_isMuted = newMuteState;
            Event ev(newMuteState ? EventType::MUTE_ENABLED : EventType::MUTE_DISABLED);
            m_dispatcher->postEvent(ev);
            
            #if defined(NATIVE_TEST)
            std::cout << "[AppLogic] Mute changed: " << newMuteState << std::endl;
            #endif
        }
        return; // Сенсор Mute обработан
    }

    // 2. Игровые сенсоры
    // Проверяем, является ли сенсор игровым и находим его индекс (бит)
    int bitIndex = -1;
    for (size_t i = 0; i < m_holeSensorIds.size(); ++i) {
        if (m_holeSensorIds[i] == id) {
            bitIndex = i;
            break;
        }
    }

    if (bitIndex != -1) {
        // Это игровой сенсор
        SensorContext& ctx = m_sensorContexts[id]; // Используем id как индекс (если id < 16)
        SensorState oldState = ctx.state;
        SensorState newState = oldState;

        // Логика порогов (3 ступени)
        if (value > m_holeClosedThreshold) {
            newState = SensorState::CLOSED;
        } else if (value > m_halfHoleThreshold) {
            newState = SensorState::HALF_HOLE;
        } else {
            newState = SensorState::OPEN;
        }

        // TBD: Логика вибрато (Спринт 2.10)

        // Проверка изменений состояния
        if (newState != oldState) {
            ctx.state = newState;

            #if defined(NATIVE_TEST)
            std::cout << "[AppLogic] Sensor " << id << " state: " << (int)newState << std::endl;
            #endif

            // Событие полузакрытия
            if (newState == SensorState::HALF_HOLE) {
                Event ev(EventType::HALF_HOLE_DETECTED, HalfHolePayload{id});
                m_dispatcher->postEvent(ev);
            }

            // Обновляем маску (в любом случае при смене состояния)
            updateMaskAndPublish();
        }
    }
}

void AppLogic::updateMaskAndPublish() {
    uint8_t newMask = 0;
    
    for (size_t i = 0; i < m_holeSensorIds.size(); ++i) {
        int id = m_holeSensorIds[i];
        // Используем id как индекс массива контекстов. 
        // Важно: предполагается, что id < 16 (размер массива).
        if (m_sensorContexts[id].state == SensorState::CLOSED) {
            newMask |= (1 << i); 
        }
    }

    if (newMask != m_currentMask) {
        m_currentMask = newMask;
        Event ev(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{newMask});
        m_dispatcher->postEvent(ev);
        
        #if defined(NATIVE_TEST)
        std::cout << "[AppLogic] Mask changed: " << (int)newMask << std::endl;
        #endif
    }
}

bool AppLogic::analyzeVibrato(const std::vector<int>& history) {
    // (TBD Спринт 2.10)
    return false;
}
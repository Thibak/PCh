/*
 * Scheduler.cpp
 *
 * Реализация `core/scheduler`.
 *
 * Соответствует: docs/modules/core_scheduler.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.1
 */

#include "core/Scheduler.h"

// (Определение TAG для Логгера)
#define TAG "Scheduler"

Application::Application() {
    // Конструктор
    // Объекты m_configManager, m_eventDispatcher, m_appLogic и т.д.
    // создаются здесь автоматически.
}

/**
 * @brief Инициализирует всю систему
 * (Фаза 1 - 4 из docs/modules/core_scheduler.md)
 */
void Application::init(
    IHalStorage* storage,
    IHalSystem* system,
    IHalUsb* usb,
    IHalSensors* sensors,
    IHalLed* led,
    IHalBle* ble,
    IHalPower* power
) {
    
    // --- Фаза 1: Хранилище, Система и Конфигурация ---
    // (storage и system переданы нам из main.cpp)
    m_configManager.init(storage);

    // --- Фаза 2: USB и Логирование ---
    // (usb передан нам из main.cpp)
    Logger::getInstance()->init(&m_configManager, usb, system);
    
    LOG_INFO(TAG, "Boot: Logger initialized.");

    // --- Фаза 3: Диспетчер Событий ---
    m_eventDispatcher.init(); // Запускает внутреннюю задачу (или ее mock)
    LOG_INFO(TAG, "Boot: EventDispatcher running.");

    // --- Фаза 4: Инициализация HAL и APP (Внедрение зависимостей) ---
    
    // Инициализируем HAL (передаем зависимости)
    // (Мы предполагаем, что HAL init() безопасны для вызова на Host)
    sensors->init(&m_configManager, &m_eventDispatcher);
    led->init(&m_configManager); // (Было пропущено в доке 0.6, но добавлено в 0.9)
    ble->init(&m_eventDispatcher);
    power->init(&m_configManager, &m_eventDispatcher);
    
    // Инициализируем APP (передаем зависимости)
    m_appFingering.init(storage);
    m_appLogic.init(&m_configManager, &m_eventDispatcher);
    
    float basePitch = m_configManager.getBasePitchHz();
    m_appMidi.init(ble, led, basePitch);
    
    LOG_INFO(TAG, "Boot: Modules initialized.");

    // --- Фаза 5: Настройка Подписок ---
    m_appLogic.subscribe(&m_eventDispatcher);
    m_appMidi.subscribe(&m_eventDispatcher);
    m_appFingering.subscribe(&m_eventDispatcher);
    power->subscribe(&m_eventDispatcher); // (halPower - единственный HAL, который слушает)
    
    LOG_INFO(TAG, "Boot: Subscriptions complete.");
}

/**
 * @brief Запускает задачи FreeRTOS
 * (Фаза 6 из docs/modules/core_scheduler.md)
 */
void Application::startTasks(
    IHalSensors* sensors,
    IHalLed* led,
    IHalBle* ble,
    IHalPower* power
) {
    LOG_INFO(TAG, "Boot: Starting tasks...");

    sensors->startTask();
    m_appLogic.startTask();
    led->startTask();
    ble->startTask();
    power->startTask();
    
    LOG_INFO(TAG, "Boot: All tasks started. System running.");
}
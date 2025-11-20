/*
 * Scheduler.cpp
 *
 * Реализация `core/scheduler`.
 * Связывает все модули системы воедино.
 *
 * Соответствует: docs/modules/core_scheduler.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.9
 */

#include "core/Scheduler.h"

// (Определение TAG для Логгера)
#define TAG "Scheduler"

Application::Application() {
    // Конструктор
}

/**
 * @brief Инициализирует всю систему
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
    // Сначала грузим конфиг, так как от него зависят многие HAL модули
    m_configManager.init(storage);

    // --- Фаза 2: USB и Логирование ---
    usb->init(storage);
    Logger::getInstance()->init(&m_configManager, usb, system);
    
    // Теперь логгер инициализирован и безопасен (в Arduino среде)
    LOG_INFO(TAG, "Boot: Logger initialized.");

    // --- Фаза 3: Диспетчер Событий ---
    m_eventDispatcher.init();
    LOG_INFO(TAG, "Boot: EventDispatcher running.");

    // --- Фаза 4: Инициализация HAL и APP ---
    
    // HAL
    sensors->init(&m_configManager, &m_eventDispatcher);
    led->init(&m_configManager);
    ble->init(&m_eventDispatcher);
    power->init(&m_configManager, &m_eventDispatcher);
    
    // APP
    m_appFingering.init(storage);
    m_appLogic.init(&m_configManager, &m_eventDispatcher);
    
    // Для Midi нужна базовая частота из конфига
    float basePitch = m_configManager.getBasePitchHz();
    m_appMidi.init(ble, led, basePitch);
    
    LOG_INFO(TAG, "Boot: Modules initialized.");

    // --- Фаза 5: Настройка Подписок ---
    m_appLogic.subscribe(&m_eventDispatcher);
    m_appMidi.subscribe(&m_eventDispatcher);
    m_appFingering.subscribe(&m_eventDispatcher);
    
    // HAL подписки
    power->subscribe(&m_eventDispatcher);
    
    LOG_INFO(TAG, "Boot sequence complete. Ready.");
}

/**
 * @brief Запускает задачи FreeRTOS
 */
void Application::startTasks(
    IHalSensors* sensors,
    IHalLed* led,
    IHalBle* ble,
    IHalPower* power
) {
    LOG_INFO(TAG, "Boot: Starting FreeRTOS tasks...");

    sensors->startTask();
    m_appLogic.startTask();
    led->startTask();
    ble->startTask();
    power->startTask();
    
    LOG_INFO(TAG, "Boot: All tasks started. System running.");
}
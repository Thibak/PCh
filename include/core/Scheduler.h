/*
 * Scheduler.h
 *
 * (Реализация класса Application)
 *
 * Соответствует: docs/modules/core_scheduler.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.1
 */
#pragma once

// --- Включаем все интерфейсы HAL ---
#include "interfaces/IHalStorage.h"
#include "interfaces/IHalSystem.h"
#include "interfaces/IHalUsb.h"
#include "interfaces/IHalSensors.h"
#include "interfaces/IHalLed.h"
#include "interfaces/IHalBle.h"
#include "interfaces/IHalPower.h"

// --- Включаем все заголовки CORE и APP ---
// (Они должны быть созданы в следующих спринтах,
// но мы включаем их здесь, т.к. Scheduler их создает)
#include "core/ConfigManager.h"
#include "core/EventDispatcher.h"
#include "core/Logger.h"
#include "app/AppFingering.h"
#include "app/AppLogic.h"
#include "app/AppMidi.h"


class Application {
public:
    Application();
    
    /**
     * @brief Инициализирует все CORE и APP модули, внедряя зависимости HAL.
     */
    void init(
        IHalStorage* storage,
        IHalSystem* system,
        IHalUsb* usb,
        IHalSensors* sensors,
        IHalLed* led,
        IHalBle* ble,
        IHalPower* power
    );

    /**
     * @brief Запускает все задачи FreeRTOS (или их mock-эмуляцию).
     */
    void startTasks(
        IHalSensors* sensors,
        IHalLed* led,
        IHalBle* ble,
        IHalPower* power
    );

private:
    // --- CORE (Конкретные классы) ---
    // (ConfigManager, EventDispatcher и Logger - это синглтоны
    // или будут управляться иначе, в зависимости от реализации)
    // Давайте сделаем их членами класса для чистоты.
    
    ConfigManager m_configManager;
    EventDispatcher m_eventDispatcher;
    
    // --- APP (Конкретные классы) ---
    AppLogic m_appLogic;
    AppFingering m_appFingering;
    AppMidi m_appMidi;
};
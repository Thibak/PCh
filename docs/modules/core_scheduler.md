# **Детальное описание модуля: core/scheduler**

**Назначение документа:** \> Этот документ описывает внутреннюю логику и обязанности модуля `core/scheduler`.

## **1\. Назначение**

Scheduler (Планировщик) — это главный "дирижер" системы. Он не является модулем HAL или APP. Его **единственная задача** — в правильной последовательности создать, инициализировать и запустить все остальные модули системы при старте (`main.cpp`).

Он отвечает за:

1. **Создание экземпляров:** `new ConfigManager()`, `new AppLogic()` и т.д.  
2. **Внедрение зависимостей (Dependency Injection):** Передачу указателей `(IHalStorage*, IHalUsb*)` в `ConfigManager` и `EventDispatcher*` во все модули-подписчики.  
3. **Запуск задач:** Вызов `hal_sensors->startTask()` и `app_logic->startTask()` для запуска их потоков `FreeRTOS`.

## **2\. Зависимости**

* **FreeRTOS:** `xTaskCreate` (для запуска задач модулей).  
* **Все модули:** `core/config_manager.h`, `core/event_dispatcher.h`, `hal_interfaces/i_*.h`, `app/logic.h` и т.д. (Так как он их все создает).

## **3\. Логика работы**

`Scheduler` — это не "долгоживущий" класс. Скорее всего, это будет один большой метод `setup()` (или класс `Application`), который вызывается из `main.cpp`.

### **3.1. Последовательность инициализации (Критически важно)**

Последовательность запуска должна быть строго соблюдена, чтобы модули получали валидные зависимости.

<!-- 1. **Фаза 1: Хранилище и Конфигурация (Обязательно до Логирования\!)**  
   * new HalStorage() \-\> m\_storage  
   * new `HalSystem() -> m_halSystem`
   * new ConfigManager() \-\> m\_configManager  
   * m\_configManager-\>init(m\_storage)  
2. **Фаза 2: USB и Логирование**  
   * new HalUsb() \-\> m\_halUsb  
   * m\_halUsb-\>init()  
   * `Logger::getInstance()->init(m_configManager, m_halUsb, m_halSystem)`  
   * *(С этого момента LOG\_INFO работает)*  
   * LOG\_INFO("Scheduler", "Boot: Logger initialized.")  
3. **Фаза 3: Диспетчер Событий (Нервная система)**  
   * new EventDispatcher() \-\> m\_dispatcher  
   * m\_dispatcher-\>init() (Запускает внутреннюю задачу eventLoopTask)  
   * LOG\_INFO("Scheduler", "Boot: EventDispatcher running.")  
4. **Фаза 4: Остальные HAL и APP (Создание экземпляров) (Обновлено)**  
   * `new HalSensors(m_configManager, m_dispatcher) -> m_halSensors`  
   * `new HalLed(m_configManager) -> m_halLed`  
   * `new HalBle(m_dispatcher) -> m_halBle`  
   * `new HalPower(m_dispatcher) -> m_halPower`  
   * `new AppFingering(m_storage) -> m_appFingering`  
   * `new AppLogic(m_configManager, m_dispatcher) -> m_appLogic`  
     * `m_appMidi->subscribe(m_dispatcher)`  
     * `m_appFingering->subscribe(m_dispatcher)`  
     * `m_halBle->subscribe(m_dispatcher)`  
     * `m_halPower->subscribe(m_dispatcher)`  
     * `float basePitch = m_configManager->getBasePitchHz()`
     * `LOG_INFO("Scheduler", "Boot: Subscriptions complete.")`
5. **Фаза 6: Запуск Задач (FreeRTOS Tasks)**  
   * m\_halSensors-\>startTask()  
   * m\_appLogic-\>startTask()  
   * m\_halBle-\>startTask()  
   * m\_halPower-\>startTask()  
   * LOG\_INFO("Scheduler", "Boot: All tasks started. System running.") -->

1. **Фаза 1: Инициализация `CORE (Config, Logger)`**
   * (В `main.cpp`: создаются `halStorage`, `halSystem`, `halUsb`).
   * `m_configManager.init(halStorage)` (Читает `settings.cfg`).
   * `Logger::getInstance()->init(&m_configManager, halUsb, halSystem)`
   * `LOG_INFO("Scheduler", "Boot: Logger initialized.")`

2. **Фаза 2: Диспетчер Событий (Нервная система)**
   * `m_dispatcher.init()` (Запускает внутреннюю задачу `eventLoopTask`)
   * `LOG_INFO("Scheduler", "Boot: EventDispatcher running.")`

3. **Фаза 3: Инициализация HAL и APP (Внедрение зависимостей)**
   * (В `main.cpp`: создаются `halSensors`, `halLed`, `halBle`, `halPower`).
   * `m_appFingering.init(halStorage)`
   * `m_appLogic.init(&m_configManager, &m_dispatcher)`
   * `float basePitch = m_configManager.getBasePitchHz();`
   * `m_appMidi.init(halBle, halLed, basePitch);`
   * `LOG_INFO("Scheduler", "Boot: Modules initialized.")`

4. **Фаза 4: Настройка Подписок (События)**
   * `m_appLogic.subscribe(&m_dispatcher)`
   * `m_appMidi.subscribe(&m_dispatcher)`
   * `m_appFingering.subscribe(&m_dispatcher)`
   * `halPower->subscribe(&m_dispatcher)` (Подписываем halPower на активность)
   * `LOG_INFO("Scheduler", "Boot: Subscriptions complete.")`
   
5. **Фаза 5: Запуск Задач (FreeRTOS Tasks)**
   * `halSensors->startTask()`
   * `m_appLogic.startTask()`
   * `halLed->startTask()`
   * `halBle->startTask()` (TBD: нужен ли startTask для BLE?)
   * `halPower->startTask()`
   * `LOG_INFO("Scheduler", "Boot: All tasks started. System running.")`

## **4\. Публичный API (C++ Header)**

Scheduler может быть реализован как класс Application, который хранит все экземпляры.

```cpp
#pragma once

// (Включает все .h файлы модулей)
#include "core/config_manager.h"
#include "core/event_dispatcher.h"
#include "diagnostics/logger.h"
#include "hal_interfaces/i_hal_storage.h"
#include "hal_interfaces/i_hal_usb.h"
#include "hal_interfaces/i_hal_system.h"
#include "hal_interfaces/i_hal_sensors.h"
#include "hal_interfaces/i_hal_led.h"
#include "hal_interfaces/i_hal_ble.h"
#include "hal_interfaces/i_hal_power.h"
#include "app/AppFingering.h"
#include "app/AppLogic.h"
#include "app/AppMidi.h"

class Application {
public:
    Application();
    
    /**
     * @brief Запускает всю систему. Этот метод не возвращает управление
     * (или возвращает после того, как все задачи созданы).
     */
    void start();

private:
    // --- CORE ---
    ConfigManager m_configManager;
    EventDispatcher m_eventDispatcher;
    // (Logger - это синглтон)

    // --- HAL (указатели на интерфейсы, реализация будет выбрана в main.cpp) ---
    IHalStorage* m_storage;
    IHalUsb* m_usb;
    IHalSystem* m_halSystem;
    IHalSensors* m_sensors;
    IHalLed* m_led;
    IHalBle* m_ble;
    IHalPower* m_power;
    
    // --- APP (конкретные классы) ---
    AppLogic m_appLogic;
    AppFingering m_appFingering;
    AppMidi m_appMidi;
};

/*
 * В main.cpp (для [env:native]) будет:
 *
 * #include "test/mocks/MockHalStorage.h"
 * // ... (и т.д. для всех Mocks)
 *
 * Application app;
 * MockHalStorage mockStorage;
 * // ... (создаем все Mocks)
 * app.start(&mockStorage, &mockUsb, ...); // Передаем Mocks в Scheduler
 *
 * В main.cpp (для [env:esp32s3]) будет:
 *
 * #include "hal/HalStorage.h"
 * // ... (и т.д. для всех реальных HAL)
 *
 * Application app;
 * HalStorage halStorage;
 * // ... (создаем все реальные HAL)
 * app.start(&halStorage, &halUsb, ...); // Передаем реальные HAL в Scheduler
 *
 */
```

## **5\. Тестирование (Host-First)**

* Модуль `Scheduler` (или `Application`) — это клей.  
* Его **главный тест (Спринт 2.1)** — это запуск `[env:native]`.  
* В main.cpp для `[env:native]` мы будем использовать `MockHalStorage`, `MockHalUsb`, `MockHalSensors` и т.д.  
* Тест заключается в том, что `app.start()` успешно отрабатывает, все задачи (Mock-задачи) запускаются, и система не падает.
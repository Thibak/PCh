# **Детальное описание модуля: core/scheduler**

**Назначение документа:** \> Этот документ описывает внутреннюю логику и обязанности модуля core/scheduler.

## **1\. Назначение**

Scheduler (Планировщик) — это главный "дирижер" системы. Он не является модулем HAL или APP. Его **единственная задача** — в правильной последовательности создать, инициализировать и запустить все остальные модули системы при старте (main.cpp).

Он отвечает за:

1. **Создание экземпляров:** new ConfigManager(), new AppLogic() и т.д.  
2. **Внедрение зависимостей (Dependency Injection):** Передачу указателей (IHalStorage\*, IHalUsb\*) в ConfigManager и EventDispatcher\* во все модули-подписчики.  
3. **Запуск задач:** Вызов hal\_sensors-\>startTask() и app\_logic-\>startTask() для запуска их потоков FreeRTOS.

## **2\. Зависимости**

* **FreeRTOS:** xTaskCreate (для запуска задач модулей).  
* **Все модули:** core/config\_manager.h, core/event\_dispatcher.h, hal\_interfaces/i\_\*.h, app/logic.h и т.д. (Так как он их все создает).

## **3\. Логика работы**

Scheduler — это не "долгоживущий" класс. Скорее всего, это будет один большой метод setup() (или класс Application), который вызывается из main.cpp.

### **3.1. Последовательность инициализации (Критически важно)**

Последовательность запуска должна быть строго соблюдена, чтобы модули получали валидные зависимости.

1. **Фаза 1: Хранилище и Конфигурация (Обязательно до Логирования\!)**  
   * new HalStorage() \-\> m\_storage  
   * new ConfigManager() \-\> m\_configManager  
   * m\_configManager-\>init(m\_storage)  
2. **Фаза 2: USB и Логирование**  
   * new HalUsb() \-\> m\_halUsb  
   * m\_halUsb-\>init()  
   * Logger::getInstance()-\>init(m\_configManager, m\_halUsb)  
   * *(С этого момента LOG\_INFO работает)*  
   * LOG\_INFO("Scheduler", "Boot: Logger initialized.")  
3. **Фаза 3: Диспетчер Событий (Нервная система)**  
   * new EventDispatcher() \-\> m\_dispatcher  
   * m\_dispatcher-\>init() (Запускает внутреннюю задачу eventLoopTask)  
   * LOG\_INFO("Scheduler", "Boot: EventDispatcher running.")  
4. **Фаза 4: Остальные HAL и APP (Создание экземпляров) (Обновлено)**  
   * new HalSensors(m\_configManager, m\_dispatcher) \-\> m\_halSensors  
   * new HalLed(m\_configManager) \-\> m\_halLed  
   * new HalBle(m\_dispatcher) \-\> m\_halBle  
   * new HalPower(m\_dispatcher) \-\> m\_halPower  
   * new AppFingering(m\_storage) \-\> m\_appFingering  
   * new AppLogic(m\_configManager, m\_dispatcher) \-\> m\_appLogic  
     * m\_appMidi-\>subscribe(m\_dispatcher)  
     * m\_appFingering-\>subscribe(m\_dispatcher)  
     * m\_halBle-\>subscribe(m\_dispatcher)  
     * m\_halPower-\>subscribe(m\_dispatcher)  
     * LOG\_INFO("Scheduler", "Boot: Subscriptions complete.")  
5. **Фаза 6: Запуск Задач (FreeRTOS Tasks)**  
   * m\_halSensors-\>startTask()  
   * m\_appLogic-\>startTask()  
   * m\_halBle-\>startTask()  
   * m\_halPower-\>startTask()  
   * LOG\_INFO("Scheduler", "Boot: All tasks started. System running.")

## **4\. Публичный API (C++ Header)**

Scheduler может быть реализован как класс Application, который хранит все экземпляры.

\#pragma once

// (Включает все .h файлы модулей)  
\#include "core/config\_manager.h"  
\#include "core/event\_dispatcher.h"  
\#include "diagnostics/logger.h"  
\#include "hal\_interfaces/i\_hal\_storage.h"  
\#include "hal\_interfaces/i\_hal\_usb.h"  
// ... (и т.д.)

class Application {  
public:  
    Application();  
      
    /\*\*  
     \* @brief Запускает всю систему. Этот метод не возвращает управление  
     \* (или возвращает после того, как все задачи созданы).  
     \*/  
    void start();

private:  
    // \--- CORE \---  
    ConfigManager m\_configManager;  
    EventDispatcher m\_eventDispatcher;  
    // (Logger \- это синглтон)

    // \--- HAL \---  
    IHalStorage\* m\_storage;  
    IHalUsb\* m\_usb;  
    IHalSensors\* m\_sensors;  
    IHalLed\* m\_led;  
    IHalBle\* m\_ble;  
    IHalPower\* m\_power;  
      
    // \--- APP \---  
    AppLogic m\_appLogic;  
    AppFingering m\_appFingering;  
    AppMidi m\_appMidi;  
      
    // ... (и другие модули)  
};

/\*  
 \* В main.cpp будет:  
 \*  
 \* Application app;  
 \* app.start();  
 \* // (FreeRTOS vTaskStartScheduler() вызывается где\-то здесь)  
 \*  
 \*/

## **5\. Тестирование (Host-First)**

* Модуль Scheduler (или Application) — это клей.  
* Его **главный тест (Спринт 2.1)** — это запуск \[env:native\].  
* В main.cpp для \[env:native\] мы будем использовать MockHalStorage, MockHalUsb, MockHalSensors и т.д.  
* Тест заключается в том, что app.start() успешно отрабатывает, все задачи (Mock-задачи) запускаются, и система не падает.
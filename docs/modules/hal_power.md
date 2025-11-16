# **Детальное описание модуля: hal/power**

**Назначение документа:** \> Этот документ описывает интерфейс (`IHalPower`) и реализацию (`HalPower`) для управления питанием.

## **1\. Назначение**

Модуль `hal_power` отвечает за реализацию логики энергосбережения, описанной в `PRD.MD` 4 — "Автоматическое полное выключение".

Его задачи:

1. Запустить в отдельной задаче `FreeRTOS` "таймер бездействия" (`powerTask`).  
2. Следить за активностью системы.  
3. Если бездействие превышает `auto_off_time_min` (из `settings.cfg`), отправить событие `SYSTEM_IDLE_TIMEOUT` в `core/event_dispatcher`.  
4. Предоставить аппаратный API `triggerPowerOff()`, который физически выключит устройство (через "soft-latch").

## **2\. Зависимости**

* **`#include "core/config_manager.h"`:** (Критическая) Используется в `init()` для получения `getAutoOffTimeMin()`.  
* **`#include "core/event_dispatcher.h"`:** (Критическая) Используется для отправки `SYSTEM_IDLE_TIMEOUT` и *получения* событий активности (напр., `SENSOR_VALUE_CHANGED`).  
* **`ESP-IDF` / `Arduino`:** `driver/gpio.h` (для управления пином "soft-latch").  
* **`FreeRTOS`:** `xTaskCreate`, `vTaskDelay`, `xTaskGetTickCount`.

## **3\. Логика работы**

### **3.1. Интерфейс (`i_hal_power.h`)**

```cpp
// (i_hal_power.h)
#pragma once

#include "core/config_manager.h"
#include "core/event_dispatcher.h"
#include "i_event_handler.h" // Для подписки на события

class IHalPower : public IEventHandler {
public:
    virtual ~IHalPower() {}

    /**
     * @brief Инициализирует модуль и пин управления питанием.
     * @param configManager Указатель на ConfigManager.
     * @param dispatcher Указатель на EventDispatcher.
     * @return true, если пин питания успешно инициализирован.
     */
    virtual bool init(ConfigManager* configManager, EventDispatcher* dispatcher) = 0;

    /**
     * @brief Запускает задачу FreeRTOS для отслеживания бездействия.
     */
    virtual void startTask() = 0;

    /**
     * @brief Немедленно инициирует аппаратное выключение.
     * * Этот метод вызывается `core/scheduler` после получения
     * * `SYSTEM_IDLE_TIMEOUT` и принятия решения о выключении.
     */
    virtual void triggerPowerOff() = 0;

    // Метод handleEvent() наследуется из IEventHandler
};
```

### **3.2. Реализация (`HalPower.cpp`)**

1. **Фаза `init(ConfigManager* cfg, EventDispatcher* disp)`:**  
   * Сохраняет `m_dispatcher = disp`.  
   * Получает `m_autoOffTimeMs = cfg->getAutoOffTimeMin() * 60 * 1000;` (конвертирует минуты в мс).  
   * Получает `m_powerPin из cfg->getPhysicalPins()` или отдельного `power_pin` (TBD: нужно уточнить в `CONFIG_SCHEMA`, если пин "soft-latch" не TBD).  
   * Настраивает `m_powerPin` как `GPIO_MODE_OUTPUT`.  
   * Вызывает `resetIdleTimer()`.  
2. **Фаза `startTask()`:**  
   * Вызывается `core/scheduler` (Фаза 6).  
   * `xTaskCreate(powerTask, "powerTask", ...)` запускает задачу.  
3. **Фаза `subscribe()` (вызывается core_scheduler):**  
   * Модуль подписывается на *любую* активность, которая сбрасывает таймер.  
   * `m_dispatcher->subscribe(EventType::SENSOR_VALUE_CHANGED, this);`  
   * `m_dispatcher->subscribe(EventType::BLE_CONNECTED, this);`  
4. **Метод `handleEvent(const Event& event)`:**  
   * `switch (event.type)`:  
     * `case EventType::SENSOR_VALUE_CHANGED`:  
     * `case EventType::BLE_CONNECTED`:  
       * `resetIdleTimer();` // Сброс таймера при любой активности  
       * `break;`  
5. **Задача `powerTask()` (FreeRTOS Task):**  
   * Работает в цикле с низкой частотой (напр., `vTaskDelay(1000)` — проверяет раз в секунду).  
   * **Внутри цикла:**  
     * `uint32_t currentTime = xTaskGetTickCount() * portTICK_PERIOD_MS;`  
     * `if (currentTime - m_lastActivityTime > m_autoOffTimeMs)`:  
       * `LOG_WARN(TAG, "Idle timeout reached. Requesting shutdown.")`;  
       * `m_dispatcher-\>postEvent(Event{EventType::SYSTEM_IDLE_TIMEOUT})`;  
       * (Задача может самоуничтожиться или перейти в ожидание `triggerPowerOff`).  
6. **Метод `triggerPowerOff()`:**  
   * `LOG_ERROR(TAG, "Triggering HW Power Off NOW.")`;  
   * `gpio_set_level(m_powerPin, 1);` (Или 0, в зависимости от схемы "soft-latch").  
   * (После этого устройство аппаратно выключается).  
7. **Приватный метод resetIdleTimer():**  
   * `m_lastActivityTime = xTaskGetTickCount() * portTICK_PERIOD_MS;`

## **4\. Публичный API (C++ Header)**

```cpp
// (HalPower.h)
#pragma once
#include "hal_interfaces/i_hal_power.h"
#include "core/config_manager.h"
#include "core/event_dispatcher.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class HalPower : public IHalPower {
public:
    HalPower();
    
    virtual bool init(ConfigManager* configManager, EventDispatcher* dispatcher) override;
    virtual void startTask() override;
    virtual void triggerPowerOff() override;

    /**
     * @brief Обрабатывает события (напр. SENSOR_VALUE_CHANGED) для сброса таймера.
     */
    virtual void handleEvent(const Event& event) override;

private:
    static void powerTask(void* params);
    void runPowerTask();

    /**
     * @brief (Потокобезопасно) сбрасывает счетчик бездействия.
     */
    void resetIdleTimer();

    ConfigManager* m_configManager;
    EventDispatcher* m_dispatcher;
    
    // (Пин управления soft-latch)
    std::string m_powerPin; 

    uint32_t m_autoOffTimeMs;
    uint32_t m_lastActivityTime; // (в мс с момента старта)
};
```

## **5\. Тестирование (Host-First)**

* `HalPower` — это "железный" модуль. Он **не будет** компилироваться в `[env:native]`.  
* Вместо него `[env:native]` будет использовать **`MockHalPower`** (Спринт 1.8).  
* `MockHalPower` будет реализовывать `IHalPower`.

```cpp
// (Пример в test/mocks/MockHalPower.h)
#include "hal_interfaces/i_hal_power.h"
#include "core/event_dispatcher.h"

class MockHalPower : public IHalPower {
public:
    virtual bool init(ConfigManager* configManager, EventDispatcher* dispatcher) override { return true; }
    virtual void startTask() override { /* (Ничего не делаем) */ }
    
    virtual void triggerPowerOff() override {
        m_powerOffTriggered = true;
    }

    virtual void handleEvent(const Event& event) override {
        // Эмулируем сброс таймера
        m_activityEventsReceived++;
    }

    // --- Методы для тестов ---
    bool wasPowerOffTriggered() const { return m_powerOffTriggered; }
    int getActivityEventsReceived() const { return m_activityEventsReceived; }

private:
    bool m_powerOffTriggered = false;
    int m_activityEventsReceived = 0;
};
```
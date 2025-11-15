# **Детальное описание модуля: hal/power**

**Назначение документа:** \> Этот документ описывает интерфейс (IHalPower) и реализацию (HalPower) для управления питанием.

## **1\. Назначение**

Модуль hal\_power отвечает за реализацию логики энергосбережения, описанной в PRD.MD 4 — "Автоматическое полное выключение".

Его задачи:

1. Запустить в отдельной задаче FreeRTOS "таймер бездействия" (powerTask).  
2. Следить за активностью системы.  
3. Если бездействие превышает auto\_off\_time\_min (из settings.cfg), отправить событие SYSTEM\_IDLE\_TIMEOUT в core/event\_dispatcher.  
4. Предоставить аппаратный API triggerPowerOff(), который физически выключит устройство (через "soft-latch").

## **2\. Зависимости**

* **\#include "core/config\_manager.h":** (Критическая) Используется в init() для получения getAutoOffTimeMin().  
* **\#include "core/event\_dispatcher.h":** (Критическая) Используется для отправки SYSTEM\_IDLE\_TIMEOUT и *получения* событий активности (напр., SENSOR\_VALUE\_CHANGED).  
* **ESP-IDF / Arduino:** driver/gpio.h (для управления пином "soft-latch").  
* **FreeRTOS:** xTaskCreate, vTaskDelay, xTaskGetTickCount.

## **3\. Логика работы**

### **3.1. Интерфейс (i\_hal\_power.h)**

// (i\_hal\_power.h)  
\#pragma once

\#include "core/config\_manager.h"  
\#include "core/event\_dispatcher.h"  
\#include "i\_event\_handler.h" // Для подписки на события

class IHalPower : public IEventHandler {  
public:  
    virtual \~IHalPower() {}

    /\*\*  
     \* @brief Инициализирует модуль и пин управления питанием.  
     \* @param configManager Указатель на ConfigManager.  
     \* @param dispatcher Указатель на EventDispatcher.  
     \* @return true, если пин питания успешно инициализирован.  
     \*/  
    virtual bool init(ConfigManager\* configManager, EventDispatcher\* dispatcher) \= 0;

    /\*\*  
     \* @brief Запускает задачу FreeRTOS для отслеживания бездействия.  
     \*/  
    virtual void startTask() \= 0;

    /\*\*  
     \* @brief Немедленно инициирует аппаратное выключение.  
     \* \* Этот метод вызывается \`core/scheduler\` после получения  
     \* \* \`SYSTEM\_IDLE\_TIMEOUT\` и принятия решения о выключении.  
     \*/  
    virtual void triggerPowerOff() \= 0;

    // Метод handleEvent() наследуется из IEventHandler  
};

### **3.2. Реализация (HalPower.cpp)**

1. **Фаза init(ConfigManager\* cfg, EventDispatcher\* disp):**  
   * Сохраняет m\_dispatcher \= disp.  
   * Получает m\_autoOffTimeMs \= cfg-\>getAutoOffTimeMin() \* 60 \* 1000; (конвертирует минуты в мс).  
   * Получает m\_powerPin из cfg-\>getPhysicalPins() или отдельного power\_pin (TBD: нужно уточнить в CONFIG\_SCHEMA, если пин "soft-latch" не TBD).  
   * Настраивает m\_powerPin как GPIO\_MODE\_OUTPUT.  
   * Вызывает resetIdleTimer().  
2. **Фаза startTask():**  
   * Вызывается core/scheduler (Фаза 6).  
   * xTaskCreate(powerTask, "powerTask", ...) запускает задачу.  
3. **Фаза subscribe() (вызывается core\_scheduler):**  
   * Модуль подписывается на *любую* активность, которая сбрасывает таймер.  
   * m\_dispatcher-\>subscribe(EventType::SENSOR\_VALUE\_CHANGED, this);  
   * m\_dispatcher-\>subscribe(EventType::BLE\_CONNECTED, this);  
4. **Метод handleEvent(const Event& event):**  
   * switch (event.type):  
     * case EventType::SENSOR\_VALUE\_CHANGED:  
     * case EventType::BLE\_CONNECTED:  
       * resetIdleTimer(); // Сброс таймера при любой активности  
       * break;  
5. **Задача powerTask() (FreeRTOS Task):**  
   * Работает в цикле с низкой частотой (напр., vTaskDelay(1000) — проверяет раз в секунду).  
   * **Внутри цикла:**  
     * uint32\_t currentTime \= xTaskGetTickCount() \* portTICK\_PERIOD\_MS;  
     * if (currentTime \- m\_lastActivityTime \> m\_autoOffTimeMs):  
       * LOG\_WARN(TAG, "Idle timeout reached. Requesting shutdown.");  
       * m\_dispatcher-\>postEvent(Event{EventType::SYSTEM\_IDLE\_TIMEOUT});  
       * (Задача может самоуничтожиться или перейти в ожидание triggerPowerOff).  
6. **Метод triggerPowerOff():**  
   * LOG\_ERROR(TAG, "Triggering HW Power Off NOW.");  
   * gpio\_set\_level(m\_powerPin, 1); (Или 0, в зависимости от схемы "soft-latch").  
   * (После этого устройство аппаратно выключается).  
7. **Приватный метод resetIdleTimer():**  
   * m\_lastActivityTime \= xTaskGetTickCount() \* portTICK\_PERIOD\_MS;

## **4\. Публичный API (C++ Header)**

// (HalPower.h)  
\#pragma once  
\#include "hal\_interfaces/i\_hal\_power.h"  
\#include "core/config\_manager.h"  
\#include "core/event\_dispatcher.h"  
\#include "freertos/FreeRTOS.h"  
\#include "freertos/task.h"

class HalPower : public IHalPower {  
public:  
    HalPower();  
      
    virtual bool init(ConfigManager\* configManager, EventDispatcher\* dispatcher) override;  
    virtual void startTask() override;  
    virtual void triggerPowerOff() override;

    /\*\*  
     \* @brief Обрабатывает события (напр. SENSOR\_VALUE\_CHANGED) для сброса таймера.  
     \*/  
    virtual void handleEvent(const Event& event) override;

private:  
    static void powerTask(void\* params);  
    void runPowerTask();

    /\*\*  
     \* @brief (Потокобезопасно) сбрасывает счетчик бездействия.  
     \*/  
    void resetIdleTimer();

    ConfigManager\* m\_configManager;  
    EventDispatcher\* m\_dispatcher;  
      
    // (Пин управления soft-latch)  
    std::string m\_powerPin; 

    uint32\_t m\_autoOffTimeMs;  
    uint32\_t m\_lastActivityTime; // (в мс с момента старта)  
};

## **5\. Тестирование (Host-First)**

* HalPower — это "железный" модуль. Он **не будет** компилироваться в \[env:native\].  
* Вместо него \[env:native\] будет использовать **MockHalPower** (Спринт 1.8).  
* MockHalPower будет реализовывать IHalPower.

// (Пример в test/mocks/MockHalPower.h)  
\#include "hal\_interfaces/i\_hal\_power.h"  
\#include "core/event\_dispatcher.h"

class MockHalPower : public IHalPower {  
public:  
    virtual bool init(ConfigManager\* configManager, EventDispatcher\* dispatcher) override { return true; }  
    virtual void startTask() override { /\* (Ничего не делаем) \*/ }  
      
    virtual void triggerPowerOff() override {  
        m\_powerOffTriggered \= true;  
    }

    virtual void handleEvent(const Event& event) override {  
        // Эмулируем сброс таймера  
        m\_activityEventsReceived++;  
    }

    // \--- Методы для тестов \---  
    bool wasPowerOffTriggered() const { return m\_powerOffTriggered; }  
    int getActivityEventsReceived() const { return m\_activityEventsReceived; }

private:  
    bool m\_powerOffTriggered \= false;  
    int m\_activityEventsReceived \= 0;  
};  

# **Детальное описание модуля: hal/led**

**Назначение документа:** \> Этот документ описывает интерфейс (IHalLed) и реализацию (HalLed) для управления светодиодным индикатором.

## **1\. Назначение**

Модуль hal\_led предоставляет абстрактный интерфейс IHalLed и управляет встроенным светодиодом (напр., Neopixel).

Его **единственная задача** — реализовать сложную логику индикации, требуемую PRD.MD и ARCH\_CONTRACT.MD:

1. Управлять *постоянным* состоянием (напр., SOLID при BLE-подключении или FAST\_BLINK при поиске).  
2. Обрабатывать *одноразовые* события (напр., BLINK\_ONCE при отправке MIDI), не "сбивая" постоянное состояние.

Этот модуль **должен** иметь собственную задачу FreeRTOS (ledTask) для управления таймингами мигания, чтобы его API setMode() был неблокирующим.

## **2\. Зависимости (Обновлено)**

* **\#include "core/config\_manager.h":** (Критическая) Используется в init() для получения getLedPin(), getLedBlinkDurationMs() и getLedBlinkPauseMs().  
* **ESP-IDF / Arduino:** Драйвер для управления LED (напр., Adafruit\_NeoPixel.h или простой digitalWrite для одноцветного LED).  
* **FreeRTOS:** xTaskCreate, vTaskDelayUntil.

## **3\. Логика работы**

### **3.1. Интерфейс (i\_hal\_led.h)**

Определяем режимы, которые требуются PRD.MD.

// (i\_hal\_led.h)  
\#pragma once

enum class LedMode {  
    OFF,          // Выключен  
    SOLID,        // Горит (BLE подключен)  
    FAST\_BLINK,   // Частое мигание (Поиск BLE)  
    BLINK\_ONCE    // Моргнуть 1 раз (Отправка MIDI)  
};

class IHalLed {  
public:  
    virtual \~IHalLed() {}

    /\*\*  
     \* @brief Устанавливает желаемый режим работы LED.  
     \* Этот метод неблокирующий.  
     \* @param mode Режим для установки.  
     \*/  
    virtual void setMode(LedMode mode) \= 0;  
};

### **3.2. Реализация (HalLed.cpp) (Обновлено)**

1. **Фаза init(ConfigManager\* cfg):**  
   * Получает led\_pin \= cfg-\>getLedPin().  
   * Получает m\_blinkDurationMs \= cfg-\>getLedBlinkDurationMs().  
   * Получает m\_blinkPauseMs \= cfg-\>getLedBlinkPauseMs().  
   * Инициализирует драйвер (напр., Adafruit\_NeoPixel).  
   * Устанавливает начальное состояние m\_currentState \= LedMode::OFF.  
2. **Фаза startTask():**  
   * Вызывается core/scheduler для запуска ledTask.  
3. **Метод setMode(LedMode mode):**  
   * Этот метод **не** управляет пином напрямую.  
   * Он только обновляет переменную состояния, которая будет прочитана ledTask.  
   * **Логика для BLINK\_ONCE:**  
     * if (mode \== LedMode::BLINK\_ONCE):  
       * Установить флаг m\_blinkOnceRequest \= true; (потокобезопасно, если ledTask тоже его сбрасывает)  
     * else:  
       * m\_currentState \= mode; // Установить "постоянное" состояние  
4. **Задача ledTask() (FreeRTOS Task):**  
   * Это "мозг" модуля. Он работает в бесконечном цикле (напр., каждые 50 мс).  
   * **Цикл:**  
     1. Проверить m\_blinkOnceRequest. Если true:  
        * m\_blinkOnceRequest \= false;  
        * Включить LED.  
        * vTaskDelay(pdMS\_TO\_TICKS(m\_blinkDurationMs)); // Длительность вспышки  
        * Выключить LED.  
        * vTaskDelay(pdMS\_TO\_TICKS(m\_blinkPauseMs)); // Пауза  
        * *Не* выключать m\_currentState.  
     2. Если m\_blinkOnceRequest \== false:  
        * Прочитать m\_currentState.  
        * switch (m\_currentState):  
          * case LedMode::SOLID: Включить LED.  
          * case LedMode::OFF: Выключить LED.  
          * case LedMode::FAST\_BLINK: Переключить состояние LED (Вкл/Выкл) для мигания (напр., каждые 250 мс).

## **4\. Публичный API (C++ Header) (Обновлено)**

// (HalLed.h)  
\#pragma once  
\#include "hal\_interfaces/i\_hal\_led.h"  
\#include "core/config\_manager.h"  
\#include "freertos/FreeRTOS.h"  
\#include "freertos/task.h"

// (Включаем драйвер, напр. Adafruit\_NeoPixel.h)

class HalLed : public IHalLed {  
public:  
    HalLed();  
      
    /\*\*  
     \* @brief Инициализация пина LED  
     \*/  
    bool init(ConfigManager\* configManager);  
      
    /\*\*  
     \* @brief Запускает задачу FreeRTOS для управления LED.  
     \*/  
    void startTask();

    /\*\*  
     \* @brief Устанавливает желаемый режим работы LED. Неблокирующий.  
     \*/  
    virtual void setMode(LedMode mode) override;

private:  
    static void ledTask(void\* params);  
    void runLedTask();

    std::string m\_ledPin;  
    // (Экземпляр драйвера, напр. Adafruit\_NeoPixel)

    // Настройки из ConfigManager  
    int m\_blinkDurationMs;  
    int m\_blinkPauseMs;

    // Переменные состояния  
    LedMode m\_currentState; // Текущее "постоянное" состояние (SOLID или FAST\_BLINK)  
    bool m\_blinkOnceRequest; // Флаг для одноразового моргания  
};

## **5\. Тестирование (Host-First)**

* HalLed — это "железный" модуль. Он **не будет** компилироваться в \[env:native\].  
* Вместо него \[env:native\] будет использовать **MockHalLed** (Спринт 1.5).  
* MockHalLed будет реализовывать IHalLed.

// (Пример в test/mocks/MockHalLed.h)  
\#include "hal\_interfaces/i\_hal\_led.h"

class MockHalLed : public IHalLed {  
public:  
    virtual void setMode(LedMode mode) override {  
        m\_lastSetMode \= mode;  
        if (mode \== LedMode::BLINK\_ONCE) {  
            m\_blinkOnceCount++;  
        } else {  
            m\_currentState \= mode;  
        }  
    }

    // \--- Методы для тестов \---  
    LedMode getLastSetMode() const { return m\_lastSetMode; }  
    LedMode getCurrentState() const { return m\_currentState; }  
    int getBlinkOnceCount() const { return m\_blinkOnceCount; }

private:  
    LedMode m\_lastSetMode;  
    LedMode m\_currentState \= LedMode::OFF;  
    int m\_blinkOnceCount \= 0;  
};  

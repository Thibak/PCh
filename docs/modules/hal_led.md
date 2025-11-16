# **Детальное описание модуля: hal/led**

**Назначение документа:** \> Этот документ описывает интерфейс (IHalLed) и реализацию (HalLed) для управления светодиодным индикатором.

## **1\. Назначение**

Модуль `hal_led` предоставляет абстрактный интерфейс IHalLed и управляет встроенным светодиодом (напр., Neopixel).

Его **единственная задача** — реализовать сложную логику индикации, требуемую PRD.MD и ARCH_CONTRACT.MD:

1. Управлять *постоянным* состоянием (напр., `SOLID` при BLE-подключении или `FAST_BLINK` при поиске).  
2. Обрабатывать *одноразовые* события (напр., `BLINK_ONCE` при отправке `MIDI`), не "сбивая" постоянное состояние.

Этот модуль **должен** иметь собственную задачу `FreeRTOS` (`ledTask`) для управления таймингами мигания, чтобы его API `setMode()` был неблокирующим.

## **2\. Зависимости (Обновлено)**

* **`#include "core/config_manager.h"`:** (Критическая) Используется в `init()` для получения `getLedPin()`, `getLedBlinkDurationMs()` и `getLedBlinkPauseMs()`.  
* **`ESP-IDF` / `Arduino`:** Драйвер для управления `LED` (напр., `Adafruit_NeoPixel.h` или простой `digitalWrite` для одноцветного LED).  
* **`FreeRTOS`:** `xTaskCreate`, `vTaskDelayUntil`.

## **3\. Логика работы**

### **3.1. Интерфейс (`i_hal_led.h`)**

Определяем режимы, которые требуются `PRD.MD`.

```cpp
// (i_hal_led.h)
#pragma once

enum class LedMode {
    OFF,          // Выключен
    SOLID,        // Горит (BLE подключен)
    FAST_BLINK,   // Частое мигание (Поиск BLE)
    BLINK_ONCE    // Моргнуть 1 раз (Отправка MIDI)
};

class IHalLed {
public:
    virtual ~IHalLed() {}

    /**
     * @brief Устанавливает желаемый режим работы LED.
     * Этот метод неблокирующий.
     * @param mode Режим для установки.
     */
    virtual void setMode(LedMode mode) = 0;
};

```

### **3.2. Реализация (HalLed.cpp) (Обновлено)**

1. **Фаза `init(ConfigManager* cfg)`:**  
   * Получает `led_pin = cfg->getLedPin()`.  
   * Получает `m_blinkDurationMs = cfg->getLedBlinkDurationMs()`.  
   * Получает `m_blinkPauseMs = cfg->getLedBlinkPauseMs()`.  
   * Инициализирует драйвер (напр., `Adafruit_NeoPixel`).  
   * Устанавливает начальное состояние `m_currentState = LedMode::OFF`.  
2. **Фаза `startTask()`:**  
   * Вызывается `core/scheduler` для запуска `ledTask`.  
3. **Метод `setMode(LedMode mode)`:**  
   * Этот метод **не** управляет пином напрямую.  
   * Он только обновляет переменную состояния, которая будет прочитана `ledTask`.  
   * **Логика для `BLINK_ONCE`:**  
     * `if (mode == LedMode::BLINK_ONCE)`:  
       * Установить флаг `m_blinkOnceRequest = true`; (потокобезопасно, если `ledTask` тоже его сбрасывает)  
     * `else`:  
       * `m_currentState = mode`; // Установить "постоянное" состояние  
4. **Задача `ledTask()` (FreeRTOS Task):**  
   * Это "мозг" модуля. Он работает в бесконечном цикле (напр., каждые 50 мс).  
   * **Цикл:**  
     1. Проверить `m_blinkOnceRequest`. Если true:  
        * `m_blinkOnceRequest = false`;  
        * Включить `LED`.  
        * `vTaskDelay(pdMS_TO_TICKS(m_blinkDurationMs))`; // Длительность вспышки  
        * Выключить `LED`.  
        * `vTaskDelay(pdMS_TO_TICKS(m_blinkPauseMs))`; // Пауза  
        * *Не* выключать `m_currentState`.  
     2. Если `m_blinkOnceRequest == false`:  
        * Прочитать `m_currentState`.  
        * `switch (m_currentState)`:  
          * `case LedMode::SOLID`: Включить `LED`.  
          * `case LedMode::OFF`: Выключить `LED`.  
          * `case LedMode::FAST_BLINK`: Переключить состояние `LED` (Вкл/Выкл) для мигания (напр., каждые 250 мс).

## **4\. Публичный API (C++ Header) (Обновлено)**

```cpp
// (HalLed.h)
#pragma once
#include "hal_interfaces/i_hal_led.h"
#include "core/config_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// (Включаем драйвер, напр. Adafruit_NeoPixel.h)

class HalLed : public IHalLed {
public:
    HalLed();
    
    /**
     * @brief Инициализация пина LED
     */
    bool init(ConfigManager* configManager);
    
    /**
     * @brief Запускает задачу FreeRTOS для управления LED.
     */
    void startTask();

    /**
     * @brief Устанавливает желаемый режим работы LED. Неблокирующий.
     */
    virtual void setMode(LedMode mode) override;

private:
    static void ledTask(void* params);
    void runLedTask();

    std::string m_ledPin;
    // (Экземпляр драйвера, напр. Adafruit_NeoPixel)

    // Настройки из ConfigManager
    int m_blinkDurationMs;
    int m_blinkPauseMs;

    // Переменные состояния
    LedMode m_currentState; // Текущее "постоянное" состояние (SOLID или FAST_BLINK)
    bool m_blinkOnceRequest; // Флаг для одноразового моргания
};
```

## **5\. Тестирование (Host-First)**

* `HalLed` — это "железный" модуль. Он **не будет** компилироваться в `[env:native]`.  
* Вместо него `[env:native]` будет использовать **`MockHalLed`** (Спринт 1.8).  
* `MockHalLed` будет реализовывать `IHalLed`.

```cpp
// (Пример в test/mocks/MockHalLed.h)
#pragma once
#include "hal_interfaces/i_hal_led.h"
#include <iostream> // Для std::cout

// Helper для преобразования enum в строку
inline const char* ledModeToString(LedMode mode) {
    switch (mode) {
        case LedMode::OFF: return "OFF";
        case LedMode::SOLID: return "SOLID";
        case LedMode::FAST_BLINK: return "FAST_BLINK";
        case LedMode::BLINK_ONCE: return "BLINK_ONCE";
        default: return "UNKNOWN";
    }
}

class MockHalLed : public IHalLed {
public:
    MockHalLed() : m_currentState(LedMode::OFF), m_blinkOnceCount(0) {}

    /**
     * @brief Эмулирует setMode, выводя в консоль (stdout).
     */
    virtual void setMode(LedMode mode) override {
        // Требование Спринта 1.8
        std::cout << "[MockHalLed] setMode: " << ledModeToString(mode) << std::endl;
        
        if (mode == LedMode::BLINK_ONCE) {
            m_blinkOnceCount++;
        } else {
            m_currentState = mode;
        }
    }

    // --- Методы для тестов ---
    LedMode getCurrentState() const { return m_currentState; }
    int getBlinkOnceCount() const { return m_blinkOnceCount; }

private:
    LedMode m_currentState;
    int m_blinkOnceCount;
};
```

# **Детальное описание модуля: hal/system**

**Назначение документа:** \> Этот документ описывает интерфейс (`IHalSystem`) и реализацию (`HalSystem`) для базовых системных утилит.

## **1\. Назначение**

Модуль `hal_system` — это HAL-абстракция для базовых функций "операционной системы" / чипа.

Он необходим для Host-First разработки:

1. **`getSystemTimestampMs()`**: `diagnostics_logging` (в CORE) должен компилироваться на ПК и ESP32. Он не может напрямую вызывать `xTaskGetTickCount()` (`ESP32`) или `std::chrono` (ПК). `IHalSystem` предоставляет эту абстракцию.  
2. **`reboot()`**: (Пока не используется, но является базовой функцией) Позволяет `CORE` или `APP` запросить перезагрузку, не зная, как это делается на "железе".

## **2\. Зависимости**

* **`FreeRTOS` (для `HalSystem`)**: `xTaskGetTickCount()`, `portTICK_PERIOD_MS`.  
* **`ESP-IDF` (для `HalSystem`)**: `esp_restart()`.  
* **C++ \<chrono\> (для `MockHalSystem`)**: `std::chrono` для эмуляции времени на хосте.

## **3\. Логика работы**

### **3.1. Интерфейс (`i_hal_system.h`)**

Чистый абстрактный интерфейс.

```cpp
// (i_hal_system.h)
#pragma once

#include <cstdint> // Для uint32_t

class IHalSystem {
public:
    virtual ~IHalSystem() {}

    /**
     * @brief Возвращает количество миллисекунд с момента старта системы.
     * * На ESP32 это будет `xTaskGetTickCount() * portTICK_PERIOD_MS`.
     * * На Host (Mock) это будет `std::chrono`.
     * @return uint32_t timestamp (ms)
     */
    virtual uint32_t getSystemTimestampMs() = 0;

    /**
     * @brief Выполняет аппаратную перезагрузку устройства.
     */
    virtual void reboot() = 0;
};
```

### **3.2. Реализация (`HalSystem.cpp`)**

"Железная" реализация для `ESP32`.

```cpp
// (Пример в HalSystem.cpp)
#include "HalSystem.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h" // Для esp_restart()

uint32_t HalSystem::getSystemTimestampMs() {
    // Прямой вызов API FreeRTOS
    return (xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void HalSystem::reboot() {
    esp_restart();
}
```
 
## **4\. Тестирование (Host-First)**

* `HalSystem` — это "железный" модуль. Он **не будет** компилироваться в `[env:native]`.  
* Вместо него `[env:native]` будет использовать **`MockHalSystem`** (Спринт 1.9).  
* `MockHalSystem` будет реализовывать `IHalSystem`.

```cpp
// (Пример в test/mocks/MockHalSystem.h)
#include "hal_interfaces/i_hal_system.h"
#include <chrono>

class MockHalSystem : public IHalSystem {
public:
    MockHalSystem() {
        // Запоминаем время старта "эмуляции"
        m_startTime = std::chrono::steady_clock::now();
    }

    virtual uint32_t getSystemTimestampMs() override {
        // Возвращаем время (ms) с момента создания мока
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
    }
    
    virtual void reboot() override {
        // В тесте мы просто запоминаем, что перезагрузку запросили
        m_rebootRequested = true;
    }

    // --- Методы для тестов ---
    bool wasRebootRequested() const { return m_rebootRequested; }

private:
    std::chrono::steady_clock::time_point m_startTime;
    bool m_rebootRequested = false;
};
```

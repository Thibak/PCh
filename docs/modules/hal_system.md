# **Детальное описание модуля: hal/system**

**Назначение документа:** \> Этот документ описывает интерфейс (IHalSystem) и реализацию (HalSystem) для базовых системных утилит.

## **1\. Назначение**

Модуль hal\_system — это HAL-абстракция для базовых функций "операционной системы" / чипа.

Он необходим для Host-First разработки:

1. **getSystemTimestampMs()**: diagnostics\_logging (в CORE) должен компилироваться на ПК и ESP32. Он не может напрямую вызывать xTaskGetTickCount() (ESP32) или std::chrono (ПК). IHalSystem предоставляет эту абстракцию.  
2. **reboot()**: (Пока не используется, но является базовой функцией) Позволяет CORE или APP запросить перезагрузку, не зная, как это делается на "железе".

## **2\. Зависимости**

* **FreeRTOS (для HalSystem)**: xTaskGetTickCount(), portTICK\_PERIOD\_MS.  
* **ESP-IDF (для HalSystem)**: esp\_restart().  
* **C++ \<chrono\> (для MockHalSystem)**: std::chrono для эмуляции времени на хосте.

## **3\. Логика работы**

### **3.1. Интерфейс (i\_hal\_system.h)**

Чистый абстрактный интерфейс.

// (i\_hal\_system.h)  
\#pragma once

\#include \<cstdint\> // Для uint32\_t

class IHalSystem {  
public:  
    virtual \~IHalSystem() {}

    /\*\*  
     \* @brief Возвращает количество миллисекунд с момента старта системы.  
     \* \* На ESP32 это будет \`xTaskGetTickCount() \* portTICK\_PERIOD\_MS\`.  
     \* \* На Host (Mock) это будет \`std::chrono\`.  
     \* @return uint32\_t timestamp (ms)  
     \*/  
    virtual uint32\_t getSystemTimestampMs() \= 0;

    /\*\*  
     \* @brief Выполняет аппаратную перезагрузку устройства.  
     \*/  
    virtual void reboot() \= 0;  
};

### **3.2. Реализация (HalSystem.cpp)**

"Железная" реализация для ESP32.

// (Пример в HalSystem.cpp)  
\#include "HalSystem.h"  
\#include "freertos/FreeRTOS.h"  
\#include "freertos/task.h"  
\#include "esp\_system.h" // Для esp\_restart()

uint32\_t HalSystem::getSystemTimestampMs() {  
    // Прямой вызов API FreeRTOS  
    return (xTaskGetTickCount() \* portTICK\_PERIOD\_MS);  
}

void HalSystem::reboot() {  
    esp\_restart();  
}

## **4\. Тестирование (Host-First)**

* HalSystem — это "железный" модуль. Он **не будет** компилироваться в \[env:native\].  
* Вместо него \[env:native\] будет использовать **MockHalSystem** (Спринт 1.9).  
* MockHalSystem будет реализовывать IHalSystem.

// (Пример в test/mocks/MockHalSystem.h)  
\#include "hal\_interfaces/i\_hal\_system.h"  
\#include \<chrono\>

class MockHalSystem : public IHalSystem {  
public:  
    MockHalSystem() {  
        // Запоминаем время старта "эмуляции"  
        m\_startTime \= std::chrono::steady\_clock::now();  
    }

    virtual uint32\_t getSystemTimestampMs() override {  
        // Возвращаем время (ms) с момента создания мока  
        auto now \= std::chrono::steady\_clock::now();  
        return std::chrono::duration\_cast\<std::chrono::milliseconds\>(now \- m\_startTime).count();  
    }  
      
    virtual void reboot() override {  
        // В тесте мы просто запоминаем, что перезагрузку запросили  
        m\_rebootRequested \= true;  
    }

    // \--- Методы для тестов \---  
    bool wasRebootRequested() const { return m\_rebootRequested; }

private:  
    std::chrono::steady\_clock::time\_point m\_startTime;  
    bool m\_rebootRequested \= false;  
};  

# **Детальное описание модуля: hal/sensors**

**Назначение документа:** \> Этот документ описывает интерфейс (IHalSensors) и реализацию (HalSensors) для подсистемы емкостных сенсоров.

## **1\. Назначение**

Модуль hal\_sensors предоставляет абстрактный интерфейс IHalSensors и инкапсулирует всю логику низкоуровневого считывания емкостных сенсоров ESP32-S3.

Его **единственная задача** — в отдельной задаче FreeRTOS (потоке) циклически опрашивать все физические Touch-пины, применять к ним базовую фильтрацию (для удаления шума) и публиковать "чистые" значения в core/event\_dispatcher с фиксированной частотой.

* hal\_sensors **не знает**, что означают эти значения (это "нажатие" или "вибрато"). Он просто сообщает: Sensor ID 5 now reads 450\.  
* Вся интерпретация (450 \> threshold) происходит в app/logic.

## **2\. Зависимости**

* **\#include "core/config\_manager.h":** (Критическая) Используется в init() для получения:  
  * getPhysicalPins(): Список пинов T1-T14 для инициализации.  
  * getSampleRateHz(): Как часто публиковать события SensorValueChanged.  
  * getFilterAlpha(): Коэффициент сглаживания для EMA-фильтра.  
* **\#include "core/event\_dispatcher.h":** (Критическая) Используется в sensorTask() для публикации событий SensorValueChanged.  
* **ESP-IDF / Arduino:** (Критическая) driver/touch\_pad.h (для IDF) или touchRead() (для Arduino) для считывания емкостных значений.  
* **FreeRTOS:** xTaskCreate, vTaskDelayUntil.

## **3\. Логика работы**

### **3.1. Интерфейс (i\_hal\_sensors.h)**

Интерфейс очень простой. Он, по сути, нужен только для запуска задачи.

// (i\_hal\_sensors.h)  
\#pragma once

\#include "core/config\_manager.h"  
\#include "core/event\_dispatcher.h"

class IHalSensors {  
public:  
    virtual \~IHalSensors() {}

    /\*\*  
     \* @brief Инициализирует драйверы Touch-пинов.  
     \* @param configManager Указатель на ConfigManager для получения списка пинов.  
     \* @param dispatcher Указатель на EventDispatcher для отправки событий.  
     \* @return true, если все пины успешно инициализированы.  
     \*/  
    virtual bool init(ConfigManager\* configManager, EventDispatcher\* dispatcher) \= 0;

    /\*\*  
     \* @brief Запускает задачу FreeRTOS для циклического опроса сенсоров.  
     \*/  
    virtual void startTask() \= 0;  
};

### **3.2. Реализация (HalSensors.cpp)**

#### **3.2.1 Алгоритм Фильтрации (SimpleFilter / EMA)**

Для очистки "дребезга" (jitter) от "сырых" данных сенсора (rawValue) используется **Экспоненциальное сглаживание (EMA)**. Это быстрый IIR-фильтр с низкой задержкой, который идеально подходит для RTOS.

Он использует формулу:  
новое\_значение \= (alpha \* сырое\_значение) \+ ((1 \- alpha) \* старое\_значение)

* **alpha** (коэффициент) загружается из settings.cfg (filter\_alpha).  
* alpha \= 1.0 (нет фильтрации).  
* alpha \= 0.1 (сильная фильтрация).

Этот фильтр (реализованный в SimpleFilter.h) отсекает высокочастотный шум, но пропускает низкочастотный полезный сигнал (2-6 Hz вибрато), который затем будет анализировать app/logic.

#### **3.2.2 Фаза init(ConfigManager\* cfg, EventDispatcher\* disp)**

1. Сохраняет указатели m\_configManager \= cfg и m\_dispatcher \= disp.  
2. Получает список пинов: m\_pinsToRead \= cfg-\>getPhysicalPins().  
3. Получает частоту: m\_sampleRateHz \= cfg-\>getSampleRateHz().  
4. Вычисляет задержку для FreeRTOS: m\_taskDelayMs \= 1000 / m\_sampleRateHz; (напр., 1000 / 50Hz \= 20 мс).  
5. Получает alpha для фильтра: float alpha \= cfg-\>getFilterAlpha().  
6. Инициализирует драйвер ESP32 Touch: touch\_pad\_init().  
7. В цикле проходит по m\_pinsToRead:  
   * Конфигурирует каждый пин (напр., touch\_pad\_config(pin)).  
   * Создает и добавляет экземпляр фильтра: m\_filters.push\_back(SimpleFilter(alpha));  
   * m\_lastValue.push\_back(0);  
8. **Калибровка:** Выполняет начальную калибровку (чтение базовых значений) и сохраняет их. LOG\_INFO(TAG, "Touch calibration complete.").

#### **3.2.3 Фаза startTask()**

1. Вызывается core/scheduler (Фаза 6).  
2. xTaskCreate(sensorTask, "sensorTask", ...) запускает задачу.

#### **3.2.4 Задача sensorTask() (FreeRTOS Task)**

1. Это "мозг" модуля. Работает в бесконечном цикле.  
2. vTaskDelayUntil(\&lastWakeTime, pdMS\_TO\_TICKS(m\_taskDelayMs)); (Гарантирует запуск ровно каждые 20 мс).  
3. **Внутри цикла:**  
   1. Проходит по всем m\_pinsToRead (напр., 9 пинов).  
   2. Для *каждого* пина (с логическим ID i):  
      * uint32\_t rawValue \= touch\_pad\_read(pin); (Получает "грязное" значение).  
      * uint32\_t filteredValue \= m\_filters\[i\].apply(rawValue); (Применяет EMA-фильтр).  
      * **Оптимизация:** (Опционально) Можно отправлять событие, только если filteredValue изменилось по сравнению с m\_lastValue\[i\].  
      * Создает событие: Event ev { EventType::SENSOR\_VALUE\_CHANGED, .payload.sensorValue \= { (int)i, (int)filteredValue } };  
      * Отправляет событие: m\_dispatcher-\>postEvent(ev);  
      * m\_lastValue\[i\] \= filteredValue;  
   3. Возвращается в vTaskDelayUntil.

## **4\. Публичный API (C++ Header)**

// (HalSensors.h)  
\#pragma once  
\#include "hal\_interfaces/i\_hal\_sensors.h"  
\#include "core/config\_manager.h"  
\#include "core/event\_dispatcher.h"  
\#include "freertos/FreeRTOS.h"  
\#include "freertos/task.h"  
\#include "driver/touch\_pad.h" // (ESP-IDF)  
\#include "SimpleFilter.h" // (Класс, реализующий EMA-фильтр)

class HalSensors : public IHalSensors {  
public:  
    HalSensors();  
      
    virtual bool init(ConfigManager\* configManager, EventDispatcher\* dispatcher) override;  
    virtual void startTask() override;

private:  
    static void sensorTask(void\* params);  
    void runSensorTask();

    ConfigManager\* m\_configManager;  
    EventDispatcher\* m\_dispatcher;

    std::vector\<touch\_pad\_t\> m\_pinsToRead; // Физические пины  
    int m\_sampleRateHz;  
    int m\_taskDelayMs;

    std::vector\<SimpleFilter\> m\_filters; // Массив фильтров, по одному на пин  
    std::vector\<int\> m\_lastValue; // Последние отправленные значения  
};

## **5\. Тестирование (Host-First)**

* HalSensors — это "железный" модуль. Он **не будет** компилироваться в \[env:native\].  
* Вместо него \[env:native\] будет использовать **MockHalSensors** (Спринт 1.7).  
* MockHalSensors будет реализовывать IHalSensors.

// (Пример в test/mocks/MockHalSensors.h)  
\#include "hal\_interfaces/i\_hal\_sensors.h"  
\#include "core/event\_dispatcher.h"

class MockHalSensors : public IHalSensors {  
public:  
    virtual bool init(ConfigManager\* configManager, EventDispatcher\* dispatcher) override {   
        m\_dispatcher \= dispatcher;  
        m\_pinCount \= configManager-\>getPhysicalPins().size();  
        return true;   
    }

    // Задачи FreeRTOS не запускаются на хосте,  
    // поэтому startTask() ничего не делает.  
    virtual void startTask() override { }

    // \--- Методы для тестов \---  
      
    /\*\*  
     \* @brief Эмулирует нажатие сенсора (вызывается из теста).  
     \*/  
    void simulateSensorChange(int logicalId, int value) {  
        if (m\_dispatcher) {  
            Event ev { EventType::SENSOR\_VALUE\_CHANGED,   
                       .payload.sensorValue \= { logicalId, value } };  
            m\_dispatcher-\>postEvent(ev);  
        }  
    }  
      
    int getConfiguredPinCount() const { return m\_pinCount; }

private:  
    EventDispatcher\* m\_dispatcher;  
    int m\_pinCount \= 0;  
};  

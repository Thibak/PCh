# **Детальное описание модуля: core/event\_dispatcher**

**Назначение документа:** \> Этот документ описывает внутреннюю логику, API и зависимости модуля core/event\_dispatcher.

## **1\. Назначение**

EventDispatcher (Диспетчер Событий) — это "нервная система" прошивки. Он реализует паттерн "Издатель-Подписчик" (Pub/Sub) поверх очередей FreeRTOS.

Его **единственная задача** — позволить модулям асинхронно общаться друг с другом, не зная о существовании друг друга (слабая связанность / decoupling).

* **Издатель (Publisher)** (напр., hal\_sensors): Создает событие (напр., SensorValueChanged) и "публикует" (отправляет) его в Диспетчер.  
* **Подписчик (Subscriber)** (напр., app/logic): "Подписывается" на определенный тип событий (SENSOR\_VALUE\_CHANGED) при инициализации.  
* **Диспетчер (Dispatcher):** Получает событие от Издателя и немедленно доставляет его всем Подписчикам.

## **2\. Зависимости**

* **FreeRTOS:** (Критическая) xQueueHandle, xQueueSend, xQueueReceive, xTaskCreate.  
* **\#include "i\_event\_handler.h" (Концептуальная):** Интерфейс, который *должен* реализовать любой модуль-подписчик.  
* **\#include "events.h" (Концептуальная):** Файл, определяющий все типы событий и их структуры данных.

## **3\. Логика работы**

### **3.1. Структуры событий (events.h)**

Сначала определяется enum со всеми типами событий и *единая структура Event*, которая использует union для хранения данных. Это эффективно для FreeRTOS (передача по значению).

// (Пример в events.h)

// 1\. Типы событий  
enum class EventType {  
    // HAL \-\> APP  
    SENSOR\_VALUE\_CHANGED,  
    BLE\_CONNECTED,  
    BLE\_DISCONNECTED,  
      
    // APP \-\> APP  
    SENSOR\_MASK\_CHANGED,  
    HALF\_HOLE\_DETECTED,  
    VIBRATO\_DETECTED,  
    MUTE\_ENABLED,  
    MUTE\_DISABLED,  
    NOTE\_PITCH\_SELECTED,  
      
    // CORE \-\> APP  
    SYSTEM\_IDLE\_TIMEOUT  
};

// 2\. Структуры данных для каждого события  
struct SensorValuePayload { int id; int value; };  
struct SensorMaskPayload { uint8\_t mask; };  
struct HalfHolePayload { int id; };  
struct VibratoPayload { int id; float depth; };  
struct NotePitchPayload { int pitch; }; // 0 \= Note Off

// 3\. Единая структура события  
struct Event {  
    EventType type;  
      
    // Объединение (union) для всех возможных данных  
    union {  
        SensorValuePayload sensorValue;  
        SensorMaskPayload sensorMask;  
        HalfHolePayload halfHole;  
        VibratoPayload vibrato;  
        NotePitchPayload notePitch;  
        // ... (другие события без данных не нуждаются в поле)  
    } payload;  
};

### **3.2. Интерфейс подписчика (i\_event\_handler.h)**

Любой класс, который хочет *получать* события, должен унаследовать этот интерфейс.

// (Пример в i\_event\_handler.h)  
\#pragma once  
\#include "events.h"

class IEventHandler {  
public:  
    virtual \~IEventHandler() {}  
      
    /\*\*  
     \* @brief Метод, который будет вызван Диспетчером  
     \* @param event Событие, которое произошло  
     \*/  
    virtual void handleEvent(const Event& event) \= 0;  
};

### **3.3. Логика Диспетчера**

1. **Фаза init():** EventDispatcher создает центральную очередь FreeRTOS (m\_eventQueue) и запускает выделенную задачу FreeRTOS (eventLoopTask).  
2. **Фаза subscribe():** Модуль-подписчик (напр., app/logic) вызывает dispatcher-\>subscribe(EventType::SENSOR\_VALUE\_CHANGED, this). Диспетчер сохраняет эту связь (напр., в std::map).  
3. **Фаза postEvent():** Модуль-издатель (напр., hal\_sensors) создает Event (напр., { EventType::SENSOR\_VALUE\_CHANGED, .payload.sensorValue \= {1, 512} }) и вызывает dispatcher-\>postEvent(event).  
4. Событие (структура Event) копируется в очередь FreeRTOS.  
5. **Фаза eventLoopTask:**  
   * Задача eventLoopTask (внутренняя задача Диспетчера) выходит из ожидания xQueueReceive.  
   * Она получает Event из очереди.  
   * Она смотрит на event.type (напр., SENSOR\_VALUE\_CHANGED).  
   * Находит в своей карте std::map всех подписчиков на этот тип (напр., app/logic).  
   * Вызывает app\_logic-\>handleEvent(event).  
   * Возвращается в ожидание xQueueReceive.

## **4\. Публичный API (C++ Header)**

\#pragma once

\#include \<vector\>  
\#include \<map\>  
\#include "freertos/FreeRTOS.h"  
\#include "freertos/queue.h"  
\#include "events.h"  
\#include "i\_event\_handler.h"

class EventDispatcher {  
public:  
    EventDispatcher();  
      
    /\*\*  
     \* @brief Создает очередь и запускает задачу-обработчик.  
     \*/  
    bool init();

    /\*\*  
     \* @brief Подписывает объект (handler) на получение событий типа (type).  
     \* @param type Тип события (e.g., EventType::SENSOR\_VALUE\_CHANGED)  
     \* @param handler Указатель (this) на объект, реализующий IEventHandler  
     \*/  
    bool subscribe(EventType type, IEventHandler\* handler);  
      
    /\*\*  
     \* @brief Публикует событие в системе. Безопасно для вызова из любого потока/задачи.  
     \* @param event Структура события.  
     \*/  
    bool postEvent(const Event& event);

private:  
    /\*\*  
     \* @brief Статический метод-обертка для запуска задачи FreeRTOS.  
     \*/  
    static void eventLoopTask(void\* params);  
      
    /\*\*  
     \* @brief Внутренний цикл обработки событий.  
     \*/  
    void eventLoop();

    QueueHandle\_t m\_eventQueue;  
      
    // Карта подписчиков  
    std::map\<EventType, std::vector\<IEventHandler\*\>\> m\_subscribers;  
};

## **5\. Тестирование (Host-First)**

* EventDispatcher — это чистая инфраструктура. Его тестирование — это **интеграционное тестирование** модулей, которые его используют.  
* **Тест (Спринт 2.6):**  
  1. Создать EventDispatcher, MockHalSensors, AppLogic.  
  2. AppLogic подписывается на EventType::SENSOR\_VALUE\_CHANGED.  
  3. MockHalSensors вызывает postEvent() с тестовым событием.  
  4. **Ожидаемый результат:** Метод AppLogic::handleEvent() должен быть вызван с правильными данными. Это доказывает, что вся цепочка (Post \-\> Queue \-\> Task \-\> Subscribe \-\> Handle) работает.
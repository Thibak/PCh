/*
 * EventDispatcher.cpp
 *
 * Реализация (Заглушка) для EventDispatcher
 *
 * Соответствует: docs/modules/core_event_dispatcher.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.2
 */
#include "core/EventDispatcher.h"
#include "core/Logger.h" // (Для LOG_INFO и т.д.)

// (Включаем заголовки FreeRTOS или их Mock-заглушки для [env:native])
#if defined(NATIVE_TEST)
    // (Нам нужны mock-заглушки для FreeRTOS API, 
    // чтобы это скомпилировалось. Пока оставляем пустым, 
    // PlatformIO может предоставить `cmocka` или мы можем
    // определить их сами)
    // (Для простой заглушки мы можем просто закомментировать код FreeRTOS)
#else
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
#endif

#define TAG "EventDispatcher"

EventDispatcher::EventDispatcher() : m_eventQueue(nullptr) {
}

bool EventDispatcher::init() {
    // (TBD в Спринте 2.6: Реальная реализация FreeRTOS)
    // LOG_INFO(TAG, "Initializing...");
    #if defined(ESP32_TARGET)
        // m_eventQueue = xQueueCreate(10, sizeof(Event));
        // xTaskCreate(eventLoopTask, "eventTask", 2048, this, 5, nullptr);
    #endif
    return true;
}

bool EventDispatcher::subscribe(EventType type, IEventHandler* handler) {
    // (TBD в Спринте 2.6)
    // m_subscribers[type].push_back(handler);
    return true;
}

bool EventDispatcher::postEvent(const Event& event) {
    // (TBD в Спринте 2.6)
    // if (m_eventQueue) {
    //     xQueueSend(m_eventQueue, &event, 0);
    //     return true;
    // }
    return false;
}

// --- Приватные методы ---

void EventDispatcher::eventLoopTask(void* params) {
    // ((EventDispatcher*)params)->eventLoop();
}

void EventDispatcher::eventLoop() {
    // Event event;
    // while(true) {
    //     if (xQueueReceive(m_eventQueue, &event, portMAX_DELAY)) {
    //         if (m_subscribers.count(event.type)) {
    //             for (IEventHandler* handler : m_subscribers[event.type]) {
    //                 handler->handleEvent(event);
    //             }
    //         }
    //     }
    // }
}
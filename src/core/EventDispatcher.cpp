/*
 * EventDispatcher.cpp
 *
 * Реализация для EventDispatcher.
 * * - ESP32: Использует FreeRTOS Queue + Task для асинхронной обработки.
 * - Native: Использует синхронный вызов (postEvent сразу вызывает handleEvent) для упрощения тестов.
 *
 * Соответствует: docs/modules/core_event_dispatcher.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.6
 */
#include "core/EventDispatcher.h"
#include "core/Logger.h"
#include <iostream> // Для отладки в Native

#define TAG "EventDispatcher"

// --- Проверка флагов сборки ---
#if !defined(ESP32_TARGET) && !defined(NATIVE_TEST)
    #error "Build environment not defined! Please define ESP32_TARGET or NATIVE_TEST."
#endif

// --- Подключение FreeRTOS только для Target ---
#if defined(ESP32_TARGET)
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
#endif

EventDispatcher::EventDispatcher() : m_eventQueue(nullptr) {
}

bool EventDispatcher::init() {
    #if defined(ESP32_TARGET)
        // 1. Создаем очередь на 20 событий
        m_eventQueue = xQueueCreate(20, sizeof(Event));
        if (m_eventQueue == nullptr) {
            LOG_ERROR(TAG, "Failed to create queue");
            return false;
        }

        // 2. Запускаем задачу-обработчик
        // Stack size: 4096 bytes, Priority: 5 (Medium-High)
        BaseType_t res = xTaskCreate(eventLoopTask, "evtLoop", 4096, this, 5, nullptr);
        if (res != pdPASS) {
            LOG_ERROR(TAG, "Failed to create task");
            return false;
        }
        return true;

    #elif defined(NATIVE_TEST)
        // Для тестов инициализация всегда успешна
        std::cout << "[EventDispatcher] Init (Native Sync Mode)" << std::endl;
        return true;
    #else
        return false;
    #endif
}

// (Новое)
void EventDispatcher::reset() {
    m_subscribers.clear();
    #if defined(NATIVE_TEST)
        std::cout << "[EventDispatcher] Reset subscribers." << std::endl;
    #endif
}

bool EventDispatcher::subscribe(EventType type, IEventHandler* handler) {
    if (handler == nullptr) return false;
    
    // Добавляем подписчика в список для данного типа событий
    m_subscribers[type].push_back(handler);
    
    #if defined(NATIVE_TEST)
        std::cout << "[EventDispatcher] Subscribed handler to event type " << (int)type << std::endl;
    #endif
    return true;
}

bool EventDispatcher::postEvent(const Event& event) {
    #if defined(ESP32_TARGET)
        // Асинхронная отправка в очередь
        if (m_eventQueue == nullptr) return false;
        
        // portMAX_DELAY = ждать бесконечно, если очередь полна (блокировка отправителя)
        // Для ISR (прерываний) нужно использовать xQueueSendFromISR (здесь упрощено)
        if (xQueueSend(m_eventQueue, &event, (TickType_t)10) != pdPASS) {
             LOG_WARN(TAG, "Queue full, event dropped: %d", (int)event.type);
             return false;
        }
        return true;

    #elif defined(NATIVE_TEST)
        // Синхронная эмуляция для тестов:
        // Сразу доставляем событие подписчикам
        std::cout << "[EventDispatcher] Posting event type " << (int)event.type << std::endl;
        
        if (m_subscribers.count(event.type)) {
            size_t count = m_subscribers[event.type].size();
            std::cout << "[EventDispatcher] Found " << count << " subscribers." << std::endl;
            
            for (IEventHandler* handler : m_subscribers[event.type]) {
                handler->handleEvent(event);
            }
        } else {
            std::cout << "[EventDispatcher] No subscribers for this event." << std::endl;
        }
        return true;
    #else
        return false;
    #endif
}

// --- Приватные методы (Только для ESP32) ---

void EventDispatcher::eventLoopTask(void* params) {
    auto* dispatcher = static_cast<EventDispatcher*>(params);
    dispatcher->eventLoop();
    // Задача FreeRTOS никогда не должна возвращаться
    // vTaskDelete(nullptr); 
}

void EventDispatcher::eventLoop() {
    #if defined(ESP32_TARGET)
    Event event(EventType::BLE_CONNECTED); // Временная переменная для буфера

    while(true) {
        // Ждем событие из очереди (блокируемся, пока не придет)
        if (xQueueReceive(m_eventQueue, &event, portMAX_DELAY) == pdPASS) {
            
            // Рассылаем подписчикам
            if (m_subscribers.count(event.type)) {
                for (IEventHandler* handler : m_subscribers[event.type]) {
                    handler->handleEvent(event);
                }
            }
        }
    }
    #endif
}
/*
 * EventDispatcher.h
 *
 * Соответствует: docs/modules/core_event_dispatcher.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.1
 */
#pragma once

#include <vector>
#include <map>
#include "events.h"
#include "interfaces/IEventHandler.h"

// Forward-declare FreeRTOS типы, чтобы не включать FreeRTOS.h в .h файл
// (Это хорошая практика, чтобы уменьшить зависимости заголовков)
typedef void* QueueHandle_t;

class EventDispatcher {
public:
    EventDispatcher();
    
    /**
     * @brief Создает очередь и запускает задачу-обработчик.
     */
    bool init();

    /**
     * @brief Подписывает объект (handler) на получение событий типа (type).
     * @param type Тип события (e.g., EventType::SENSOR_VALUE_CHANGED)
     * @param handler Указатель (this) на объект, реализующий IEventHandler
     */
    bool subscribe(EventType type, IEventHandler* handler);
    
    /**
     * @brief Публикует событие в системе. Безопасно для вызова из любого потока/задачи.
     * @param event Структура события.
     */
    bool postEvent(const Event& event);

    // Для тестов
    void reset();

private:
    /**
     * @brief Статический метод-обертка для запуска задачи FreeRTOS.
     */
    static void eventLoopTask(void* params);
    
    /**
     * @brief Внутренний цикл обработки событий.
     */
    void eventLoop();

    QueueHandle_t m_eventQueue;
    
    // Карта подписчиков (EventType -> список IEventHandler*)
    std::map<EventType, std::vector<IEventHandler*>> m_subscribers;
};
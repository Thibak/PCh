/*
 * IEventHandler.h
 *
 * Абстрактный интерфейс (контракт) для любого класса,
 * который хочет *подписаться* на события от EventDispatcher.
 *
 * Соответствует: docs/modules/core_event_dispatcher.md
 */
#pragma once
#include "events.h"

class IEventHandler {
public:
    virtual ~IEventHandler() {}
    
    /**
     * @brief Метод, который будет вызван Диспетчером
     * @param event Событие, которое произошло
     */
    virtual void handleEvent(const Event& event) = 0;
};
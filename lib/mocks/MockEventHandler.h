/*
 * MockEventHandler.h
 *
 * Универсальный Mock-слушатель для тестов.
 * Реализует интерфейс IEventHandler и запоминает последние события.
 *
 * Полезен для проверки того, что EventDispatcher доставляет события,
 * или что модулей (AppLogic) их публикуют.
 */
#pragma once
#include "interfaces/IEventHandler.h"
#include <vector>

class MockEventHandler : public IEventHandler {
public:
    MockEventHandler();
    virtual ~MockEventHandler();

    // Реализация интерфейса
    virtual void handleEvent(const Event& event) override;

    // --- API для тестов ---
    
    /**
     * @brief Сбрасывает счетчики и историю событий.
     */
    void reset();

    /**
     * @brief Возвращает количество полученных событий.
     */
    int getReceivedCount() const;

    /**
     * @brief Возвращает тип последнего полученного события.
     */
    EventType getLastEventType() const;

    /**
     * @brief Возвращает int-значение payload последнего события
     * (удобно для проверки SENSOR_VALUE_CHANGED).
     */
    int getLastIntPayload() const;

private:
    int m_receivedCount;
    EventType m_lastType;
    int m_lastIntPayload;
};
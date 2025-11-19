/*
 * MockEventHandler.cpp
 */
#include "MockEventHandler.h"

MockEventHandler::MockEventHandler() {
    reset();
}

MockEventHandler::~MockEventHandler() {
}

void MockEventHandler::handleEvent(const Event& event) {
    m_receivedCount++;
    m_lastType = event.type;

    // Сохраняем payload для удобства проверок в тестах
    // (Можно расширить логику, если нужны другие типы payload)
    if (event.type == EventType::SENSOR_VALUE_CHANGED) {
        m_lastIntPayload = event.payload.sensorValue.value;
    } else if (event.type == EventType::HALF_HOLE_DETECTED) {
        m_lastIntPayload = event.payload.halfHole.id;
    } else if (event.type == EventType::NOTE_PITCH_SELECTED) {
        m_lastIntPayload = event.payload.notePitch.pitch;
    }
}

void MockEventHandler::reset() {
    m_receivedCount = 0;
    // Инициализируем чем-то нейтральным, но EventType - это enum class,
    // поэтому просто берем первое попавшееся или добавляем UNKNOWN, если нужно.
    // Здесь просто оставляем последнее значение или дефолтное.
    m_lastIntPayload = 0;
}

int MockEventHandler::getReceivedCount() const {
    return m_receivedCount;
}

EventType MockEventHandler::getLastEventType() const {
    return m_lastType;
}

int MockEventHandler::getLastIntPayload() const {
    return m_lastIntPayload;
}
/*
 * AppFingering.cpp
 *
 * Реализация (Заглушка) для AppFingering
 *
 * Соответствует: docs/modules/app_fingering.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.2
 */
#include "app/AppFingering.h"
#include "core/Logger.h" // (Для LOG_INFO и т.д.)

#define TAG "AppFingering"

AppFingering::AppFingering() 
    : m_dispatcher(nullptr), 
      m_currentMask(0), 
      m_lastPublishedNote(0) {
}

bool AppFingering::init(IHalStorage* storage) {
    // (TBD в Спринте 2.10: Реализовать парсинг)
    // LOG_INFO(TAG, "Initializing...");
    return true;
}

void AppFingering::subscribe(EventDispatcher* dispatcher) {
    m_dispatcher = dispatcher;
    // (TBD в Спринте 2.10: 
    // m_dispatcher->subscribe(EventType::SENSOR_MASK_CHANGED, this);
    // m_dispatcher->subscribe(EventType::HALF_HOLE_DETECTED, this);
    // )
}

void AppFingering::handleEvent(const Event& event) {
    // (TBD в Спринте 2.10)
}

// --- Приватные методы ---

void AppFingering::parseFingeringConfig(const std::string& fileContent) {
    // (TBD в Спринте 2.10)
}

int AppFingering::findNote(uint8_t mask, int halfHoleSensorId) {
    // (TBD в Спринте 2.10)
    return 0; // (Пока всегда возвращаем Note Off)
}

void AppFingering::publishNote(int note) {
    // (TBD в Спринте 2.10)
}
/*
 * MockHalLed.cpp
 *
 * Реализация Mock-класса для IHalLed.
 */
#include "MockHalLed.h"

MockHalLed::MockHalLed() 
    : m_currentState(LedMode::OFF), m_blinkOnceCount(0) {
}

MockHalLed::~MockHalLed() {
}

/**
 * @brief Эмулирует setMode, выводя в консоль (stdout).
 * (Реализация требования Спринта 1.8)
 */
void MockHalLed::setMode(LedMode mode) {
    // Выводим в консоль
    std::cout << "[MockHalLed] setMode: " << ledModeToString(mode) << std::endl;
    
    if (mode == LedMode::BLINK_ONCE) {
        m_blinkOnceCount++;
    } else {
        m_currentState = mode;
    }
}

// --- Методы для тестов ---

LedMode MockHalLed::getCurrentState() const {
    return m_currentState;
}

int MockHalLed::getBlinkOnceCount() const {
    return m_blinkOnceCount;
}
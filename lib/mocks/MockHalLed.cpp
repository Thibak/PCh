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
 * @brief (Новое) Mock-реализация init.
 */
bool MockHalLed::init(ConfigManager* configManager) {
    std::cout << "[MockHalLed] Init() called." << std::endl;
    // (В реальном моке мы бы могли что-то прочитать из configManager,
    // но для заглушки это не обязательно)
    return true;
}

/**
 * @brief (Новое) Mock-реализация startTask.
 */
void MockHalLed::startTask() {
    std::cout << "[MockHalLed] startTask() called (no-op)." << std::endl;
    // (На хосте задачи не запускаются)
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
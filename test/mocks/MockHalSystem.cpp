/*
 * MockHalSystem.cpp
 *
 * Реализация Mock-класса для IHalSystem.
 */
#include "MockHalSystem.h"

MockHalSystem::MockHalSystem() 
    : m_rebootRequested(false) {
    // Запоминаем время "старта" (создания мока)
    m_startTime = std::chrono::steady_clock::now();
    std::cout << "[MockHalSystem] Init(). Timer started." << std::endl;
}

MockHalSystem::~MockHalSystem() {
}

/**
 * @brief Возвращает эмулированное время (ms) с момента создания мока.
 * (Реализация требования Спринта 0.16)
 */
uint32_t MockHalSystem::getSystemTimestampMs() {
    auto now = std::chrono::steady_clock::now();
    // Возвращаем разницу в миллисекундах
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_startTime).count();
}

void MockHalSystem::reboot() {
    std::cout << "[MockHalSystem] !!! MOCK REBOOT REQUESTED !!!" << std::endl;
    m_rebootRequested = true;
}

// --- Методы для тестов ---
bool MockHalSystem::wasRebootRequested() const {
    return m_rebootRequested;
}
/*
 * MockHalPower.cpp
 *
 * Реализация Mock-класса для IHalPower.
 */
#include "MockHalPower.h"

MockHalPower::MockHalPower()
    : m_powerOffTriggered(false), m_activityEventsReceived(0) {
}

MockHalPower::~MockHalPower() {
}

bool MockHalPower::init(ConfigManager* configManager, EventDispatcher* dispatcher) {
    std::cout << "[MockHalPower] Init()." << std::endl;
    return true;
}

void MockHalPower::startTask() {
    std::cout << "[MockHalPower] startTask() called (no-op)." << std::endl;
}

void MockHalPower::subscribe(EventDispatcher* dispatcher) {
    std::cout << "[MockHalPower] subscribe() called." << std::endl;
    // (В реальном моке мы бы здесь сохранили dispatcher
    // и подписались на SENSOR_VALUE_CHANGED,
    // но для простой заглушки это не обязательно)
}

void MockHalPower::triggerPowerOff() {
    std::cout << "[MockHalPower] !!! MOCK POWER OFF TRIGGERED !!!" << std::endl;
    m_powerOffTriggered = true;
}

void MockHalPower::handleEvent(const Event& event) {
    // Эмулируем сброс таймера (требование Спринта 0.12)
    m_activityEventsReceived++;
    std::cout << "[MockHalPower] handleEvent() received activity event." << std::endl;
}

// --- Методы для тестов ---
bool MockHalPower::wasPowerOffTriggered() const {
    return m_powerOffTriggered;
}

int MockHalPower::getActivityEventsReceived() const {
    return m_activityEventsReceived;
}
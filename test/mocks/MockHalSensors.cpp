/*
 * MockHalSensors.cpp
 *
 * Реализация Mock-класса для IHalSensors.
 */
#include "MockHalSensors.h"
#include "core/ConfigManager.h" // Нужен для init

MockHalSensors::MockHalSensors()
    : m_dispatcher(nullptr), m_pinCount(0) {
}

MockHalSensors::~MockHalSensors() {
}

bool MockHalSensors::init(ConfigManager* configManager, EventDispatcher* dispatcher) {
    m_dispatcher = dispatcher;
    // Мы не можем получить `physical_pins` напрямую из ConfigManager,
    // так как он еще не инициализирован (это сделает Scheduler).
    // Мы эмулируем, что ConfigManager *будет* загружен.
    // m_pinCount = configManager->getPhysicalPins().size(); // Не можем вызвать
    std::cout << "[MockHalSensors] Init()." << std::endl;
    return true; 
}

void MockHalSensors::startTask() {
    // Ничего не делаем, у мока нет своей задачи
    std::cout << "[MockHalSensors] startTask() called (no-op)." << std::endl;
}

// --- Методы для тестов ---

void MockHalSensors::pushMockSensorValue(int logicalId, int value) {
    if (m_dispatcher) {
        // Создаем и отправляем событие, как это делал бы настоящий HAL
        Event ev(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{logicalId, value});
        m_dispatcher->postEvent(ev);
    } else {
        std::cerr << "[MockHalSensors] ERROR: pushMockSensorValue called but dispatcher is null!" << std::endl;
    }
}

int MockHalSensors::getConfiguredPinCount() const {
    // (Полезно для тестов, чтобы проверить, что Scheduler все настроил)
    return m_pinCount; 
}
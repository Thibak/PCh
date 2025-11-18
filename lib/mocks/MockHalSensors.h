/*
 * MockHalSensors.h
 *
 * Mock-реализация (для Host-тестирования) интерфейса IHalSensors.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 1.10
 * Требование: Должен иметь API pushMockSensorValue()
 */
#pragma once
#include "interfaces/IHalSensors.h"
#include "core/EventDispatcher.h" // Нужен для отправки событий
#include <iostream> // Для std::cout

class MockHalSensors : public IHalSensors {
public:
    MockHalSensors();
    virtual ~MockHalSensors();

    virtual bool init(ConfigManager* configManager, EventDispatcher* dispatcher) override;
    virtual void startTask() override;

    // --- Методы для тестов ---
    
    /**
     * @brief "Вбрасывает" эмулированное значение сенсора в систему.
     * (Реализация требования Спринта 1.10)
     */
    void pushMockSensorValue(int logicalId, int value);
    
    int getConfiguredPinCount() const;

private:
    EventDispatcher* m_dispatcher;
    int m_pinCount;
};
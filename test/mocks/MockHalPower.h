/*
 * MockHalPower.h
 *
 * Mock-реализация (для Host-тестирования) интерфейса IHalPower.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 1.11
 */
#pragma once
#include "interfaces/IHalPower.h"
#include "core/EventDispatcher.h" // Для IEventHandler
#include <iostream>

class MockHalPower : public IHalPower {
public:
    MockHalPower();
    virtual ~MockHalPower();

    virtual bool init(ConfigManager* configManager, EventDispatcher* dispatcher) override;
    virtual void startTask() override;
    virtual void subscribe(EventDispatcher* dispatcher) override;
    virtual void triggerPowerOff() override;
    virtual void handleEvent(const Event& event) override;

    // --- Методы для тестов ---
    bool wasPowerOffTriggered() const;
    int getActivityEventsReceived() const;

private:
    bool m_powerOffTriggered;
    int m_activityEventsReceived;
};
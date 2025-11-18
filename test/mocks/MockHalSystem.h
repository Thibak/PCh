/*
 * MockHalSystem.h
 *
 * Mock-реализация (для Host-тестирования) интерфейса IHalSystem.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 1.11
 */
#pragma once
#include "interfaces/IHalSystem.h"
#include <chrono> // Для эмуляции timestamp
#include <iostream>

class MockHalSystem : public IHalSystem {
public:
    MockHalSystem();
    virtual ~MockHalSystem();

    /**
     * @brief Возвращает эмулированное время (ms) с момента создания мока.
     */
    virtual uint32_t getSystemTimestampMs() override;

    /**
     * @brief Эмулирует перезагрузку (запоминает флаг).
     */
    virtual void reboot() override;

    // --- Методы для тестов ---
    bool wasRebootRequested() const;

private:
    std::chrono::steady_clock::time_point m_startTime;
    bool m_rebootRequested;
};
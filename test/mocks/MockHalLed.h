/*
 * MockHalLed.h
 *
 * Mock-реализация (для Host-тестирования) интерфейса IHalLed.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 1.8
 * Требование: Должен выводить setMode() в stdout.
 */
#pragma once
#include "interfaces/IHalLed.h"
#include "core/ConfigManager.h" // <-- (Новое) для init()
#include <iostream> // Для std::cout

// Helper для преобразования enum в строку
inline const char* ledModeToString(LedMode mode) {
    switch (mode) {
        case LedMode::OFF: return "OFF";
        case LedMode::SOLID: return "SOLID";
        case LedMode::FAST_BLINK: return "FAST_BLINK";
        case LedMode::BLINK_ONCE: return "BLINK_ONCE";
        default: return "UNKNOWN";
    }
}

class MockHalLed : public IHalLed {
public:
    MockHalLed();
    virtual ~MockHalLed();

    /**
     * @brief (Новое) Mock-реализация init.
     */
    virtual bool init(ConfigManager* configManager) override;

    /**
     * @brief (Новое) Mock-реализация startTask.
     */
    virtual void startTask() override;

    /**
     * @brief Эмулирует setMode, выводя в консоль (stdout).
     */
    virtual void setMode(LedMode mode) override;

    // --- Методы для тестов ---
    LedMode getCurrentState() const;
    int getBlinkOnceCount() const;

private:
    LedMode m_currentState;
    int m_blinkOnceCount;
};
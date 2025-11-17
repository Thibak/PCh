/*
 * IHalLed.h
 *
 * Абстрактный интерфейс (контракт) для LED-индикатора.
 *
 * Соответствует: docs/modules/hal_led.md
 */
#pragma once

// Определяем режимы, которые требуются PRD.MD
enum class LedMode {
    OFF,          // Выключен
    SOLID,        // Горит (BLE подключен)
    FAST_BLINK,   // Частое мигание (Поиск BLE)
    BLINK_ONCE    // Моргнуть 1 раз (Отправка MIDI)
};

class IHalLed {
public:
    virtual ~IHalLed() {}

    /**
     * @brief Устанавливает желаемый режим работы LED.
     * Этот метод неблокирующий.
     * @param mode Режим для установки.
     */
    virtual void setMode(LedMode mode) = 0;
};
/*
 * IHalSystem.h
 *
 * Абстрактный интерфейс (контракт) для Системных функций.
 *
 * Соответствует: docs/modules/hal_system.md
 */
#pragma once

#include <cstdint> // Для uint32_t

class IHalSystem {
public:
    virtual ~IHalSystem() {}

    /**
     * @brief Возвращает количество миллисекунд с момента старта системы.
     * @return uint32_t timestamp (ms)
     */
    virtual uint32_t getSystemTimestampMs() = 0;

    /**
     * @brief Выполняет аппаратную перезагрузку устройства.
     */
    virtual void reboot() = 0;
};
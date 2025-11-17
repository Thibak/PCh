/*
 * IHalUsb.h
 *
 * Абстрактный интерфейс (контракт) для USB (Композитное устройство).
 *
 * Соответствует: docs/modules/hal_usb.md
 */
#pragma once

#include <string>
#include "interfaces/IHalStorage.h" // Для init()

class IHalUsb {
public:
    virtual ~IHalUsb() {}

    /**
     * @brief Инициализирует стек USB как композитное устройство.
     * @param storage Указатель на уже инициализированный hal_storage.
     * @return true, если USB-стек успешно запущен.
     */
    virtual bool init(IHalStorage* storage) = 0;

    /**
     * @brief Отправляет строку в буфер USB Serial (CDC).
     * @param line Строка для отправки (уже отформатированная логгером).
     * @return true, если данные успешно помещены в буфер отправки.
     */
    virtual bool serialPrint(const std::string& line) = 0;
};
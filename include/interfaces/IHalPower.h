/*
 * IHalPower.h
 *
 * Абстрактный интерфейс (контракт) для Управления Питанием.
 *
 * Соответствует: docs/modules/hal_power.md
 */
#pragma once

#include "interfaces/IEventHandler.h" // Для подписки на события

// (Forward-declare)
class ConfigManager;
class EventDispatcher;

class IHalPower : public IEventHandler {
public:
    virtual ~IHalPower() {}

    /**
     * @brief Инициализирует модуль и пин управления питанием.
     * @param configManager Указатель на ConfigManager.
     * @param dispatcher Указатель на EventDispatcher.
     * @return true, если пин питания успешно инициализирован.
     */
    virtual bool init(ConfigManager* configManager, EventDispatcher* dispatcher) = 0;

    /**
     * @brief Запускает задачу FreeRTOS для отслеживания бездействия.
     */
    virtual void startTask() = 0;
    
    /**
     * @brief Подписывает модуль на события EventDispatcher.
     */
    virtual void subscribe(EventDispatcher* dispatcher) = 0;

    /**
     * @brief Немедленно инициирует аппаратное выключение.
     */
    virtual void triggerPowerOff() = 0;

    // handleEvent() наследуется от IEventHandler
};
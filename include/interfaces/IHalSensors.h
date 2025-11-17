/*
 * IHalSensors.h
 *
 * Абстрактный интерфейс (контракт) для Сенсоров.
 *
 * Соответствует: docs/modules/hal_sensors.md
 */
#pragma once

// (Forward-declare, чтобы избежать циклической зависимости)
class ConfigManager;
class EventDispatcher;

class IHalSensors {
public:
    virtual ~IHalSensors() {}

    /**
     * @brief Инициализирует драйверы Touch-пинов.
     * @param configManager Указатель на ConfigManager для получения списка пинов.
     * @param dispatcher Указатель на EventDispatcher для отправки событий.
     * @return true, если все пины успешно инициализированы.
     */
    virtual bool init(ConfigManager* configManager, EventDispatcher* dispatcher) = 0;

    /**
     * @brief Запускает задачу FreeRTOS для циклического опроса сенсоров.
     */
    virtual void startTask() = 0;
};
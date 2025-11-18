/*
 * AppFingering.h
 *
 * Соответствует: docs/modules/app_fingering.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.1
 */
#pragma once

#include "interfaces/IHalStorage.h"
#include "core/EventDispatcher.h"
#include "interfaces/IEventHandler.h"
#include <map>
#include <string>
#include <cstdint> 

// (Определение структуры FingeringRule)
struct FingeringRule {
    int mainNote;
    std::map<int, int> halfHoleRules;
    FingeringRule() : mainNote(0) {}
};

class AppFingering : public IEventHandler {
public:
    AppFingering();
    
    /**
     * @brief Читает и парсит fingering.cfg из хранилища.
     * @return true, если конфиг успешно загружен и распарсен.
     */
    bool init(IHalStorage* storage);

    /**
     * @brief Подписывает модуль на события от EventDispatcher.
     */
    void subscribe(EventDispatcher* dispatcher);

    /**
     * @brief Обрабатывает SENSOR_MASK_CHANGED и HALF_HOLE_DETECTED.
     */
    virtual void handleEvent(const Event& event) override;

private:
    /**
     * @brief Внутренний метод парсинга fingering.cfg.
     */
    void parseFingeringConfig(const std::string& fileContent);

    /**
     * @brief Ищет ноту в m_fingeringMap по маске и (опционально) ID сенсора полузакрытия.
     */
    int findNote(uint8_t mask, int halfHoleSensorId = -1);

    /**
     * @brief Публикует событие NOTE_PITCH_SELECTED, если нота изменилась.
     */
    void publishNote(int note);

    EventDispatcher* m_dispatcher;
    std::map<uint8_t, FingeringRule> m_fingeringMap;

    // Переменные состояния
    uint8_t m_currentMask; // Последняя активная маска
    int m_lastPublishedNote; // Последняя отправленная нота (для защиты от "дребезга")
};
/*
 * AppFingering.cpp
 *
 * Реализация транслятора аппликатуры.
 * Читает fingering.cfg и преобразует события маски/жестов в ноты.
 *
 * Соответствует: docs/modules/app_fingering.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.6
 */
#include "app/AppFingering.h"
#include "core/Logger.h"
#include <sstream>
#include <algorithm>
#include <iostream> // Для отладки в Native

#define TAG "AppFingering"

// --- Вспомогательные функции ---

// Парсит строку "0b101" или "0x5" или "5" в int
static int parseNumber(const std::string& str) {
    try {
        if (str.size() > 2 && str.substr(0, 2) == "0b") {
            return std::stoi(str.substr(2), nullptr, 2);
        }
        if (str.size() > 2 && str.substr(0, 2) == "0x") {
            return std::stoi(str.substr(2), nullptr, 16);
        }
        return std::stoi(str);
    } catch (...) {
        return -1; // Ошибка парсинга
    }
}

// --- Конструктор ---

AppFingering::AppFingering() 
    : m_dispatcher(nullptr), 
      m_currentMask(0), 
      m_lastPublishedNote(0),
      m_currentHalfHoleId(-1) {
}

// --- Init ---

bool AppFingering::init(IHalStorage* storage) {

    // СБРОС СОСТОЯНИЯ (Критично для тестов и перезагрузки конфига)
    m_currentMask = 0;
    m_lastPublishedNote = 0;
    m_currentHalfHoleId = -1;

    std::string content;
    if (storage->readFile("/fingering.cfg", content)) {
        parseFingeringConfig(content);
        return true;
    } else {
        LOG_ERROR(TAG, "fingering.cfg not found!");
        return false;
    }
}

// --- Subscribe ---

void AppFingering::subscribe(EventDispatcher* dispatcher) {
    m_dispatcher = dispatcher;
    if (m_dispatcher) {
        m_dispatcher->subscribe(EventType::SENSOR_MASK_CHANGED, this);
        m_dispatcher->subscribe(EventType::HALF_HOLE_DETECTED, this);
    }
}

// --- Handle Event ---

void AppFingering::handleEvent(const Event& event) {
    if (event.type == EventType::SENSOR_MASK_CHANGED) {
        m_currentMask = event.payload.sensorMask.mask;
        // При смене маски (закрытии/открытии отверстий) сбрасываем 
        // состояние полузакрытия, так как пальцы явно сместились.
        m_currentHalfHoleId = -1;
        
        int note = findNote(m_currentMask, m_currentHalfHoleId);
        publishNote(note);
    } 
    else if (event.type == EventType::HALF_HOLE_DETECTED) {
        m_currentHalfHoleId = event.payload.halfHole.id;
        
        int note = findNote(m_currentMask, m_currentHalfHoleId);
        publishNote(note);
    }
}

// --- Приватные методы ---

void AppFingering::parseFingeringConfig(const std::string& fileContent) {
    std::istringstream stream(fileContent);
    std::string line;

    m_fingeringMap.clear();
    int loadedCount = 0;

    while (std::getline(stream, line)) {
        // Убираем комментарии
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        std::istringstream iss(line);
        std::string maskStr, noteStr, hhIdStr, hhNoteStr;

        // Читаем обязательные поля: MASK и NOTE
        if (iss >> maskStr >> noteStr) {
            int mask = parseNumber(maskStr);
            int note = parseNumber(noteStr);

            if (mask < 0 || note < 0) continue;

            // Получаем или создаем правило для этой маски
            FingeringRule& rule = m_fingeringMap[(uint8_t)mask];
            rule.mainNote = note;

            // Читаем опциональные поля: HH_ID и HH_NOTE
            if (iss >> hhIdStr >> hhNoteStr) {
                int hhId = parseNumber(hhIdStr);
                int hhNote = parseNumber(hhNoteStr);
                if (hhId >= 0 && hhNote >= 0) {
                    rule.halfHoleRules[hhId] = hhNote;
                }
            }
            loadedCount++;
        }
    }
    LOG_INFO(TAG, "Loaded %d fingering rules.", loadedCount);
}

int AppFingering::findNote(uint8_t mask, int halfHoleSensorId) {
    // Если маски нет в карте, возвращаем 0 (Note Off / Тишина)
    if (m_fingeringMap.count(mask) == 0) {
        return 0; 
    }

    const FingeringRule& rule = m_fingeringMap.at(mask);

    // Проверяем, есть ли активное полузакрытие И есть ли правило для него
    if (halfHoleSensorId != -1 && rule.halfHoleRules.count(halfHoleSensorId)) {
        return rule.halfHoleRules.at(halfHoleSensorId);
    }

    // Иначе возвращаем основную ноту
    return rule.mainNote;
}

void AppFingering::publishNote(int note) {
    // Дедупликация: не отправляем ту же ноту повторно
    if (note == m_lastPublishedNote) {
        return;
    }

    m_lastPublishedNote = note;

    if (m_dispatcher) {
        Event ev(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{note});
        m_dispatcher->postEvent(ev);
    }
}
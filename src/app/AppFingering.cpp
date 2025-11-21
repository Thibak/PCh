/*
 * AppFingering.cpp
 *
 * Реализация транслятора аппликатуры.
 *
 * Соответствует: docs/modules/app_fingering.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.6
 */

#include "app/AppFingering.h"
#include "core/Logger.h"
#include <sstream>
#include <algorithm>
#include <vector>
#include <iterator>
#include <iostream> // std::cout, std::cerr
#include <cctype>   // isspace
#include <iomanip>  

#define TAG "AppFingering"

// --- Вспомогательные функции ---

static std::string trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

static int parseNumber(const std::string& str) {
    std::string cleanStr = trim(str);
    try {
        if (cleanStr.size() > 2 && cleanStr.substr(0, 2) == "0b") {
            return std::stoi(cleanStr.substr(2), nullptr, 2);
        }
        if (cleanStr.size() > 2 && cleanStr.substr(0, 2) == "0x") {
            return std::stoi(cleanStr.substr(2), nullptr, 16);
        }
        return std::stoi(cleanStr);
    } catch (...) {
        std::cerr << "[AppFingering] ERROR: parseNumber failed for '" << cleanStr << "'" << std::endl;
        return -1; 
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
    m_currentMask = 0;
    m_lastPublishedNote = 0;
    m_currentHalfHoleId = -1;
    
    // Вывод всегда, чтобы точно видеть запуск
    std::cout << "[AppFingering] Init. Loading config..." << std::endl;

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
        m_currentHalfHoleId = -1; 
        
        std::cout << "[AppFingering] Mask Changed -> " << (int)m_currentMask << std::endl;
        
        int note = findNote(m_currentMask, m_currentHalfHoleId);
        publishNote(note);
    } 
    else if (event.type == EventType::HALF_HOLE_DETECTED) {
        m_currentHalfHoleId = event.payload.halfHole.id;
        
        std::cout << "[AppFingering] HalfHole Detected ID: " << m_currentHalfHoleId << std::endl;

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

    std::cout << "[AppFingering] Starting parse loop..." << std::endl;

    while (std::getline(stream, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        line = trim(line);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        // --- ГРЯЗНАЯ ОТЛАДКА (ВЫВОД ВСЕГДА) ---
        std::cout << "[AppFingering] PARSE LINE: '" << line << "' -> Tokens: " << tokens.size();
        for(auto t : tokens) std::cout << " [" << t << "]";
        std::cout << std::endl;
        // --------------------------------------

        if (tokens.size() >= 2) {
            int mask = parseNumber(tokens[0]);
            int note = parseNumber(tokens[1]);

            if (mask < 0 || note < 0) {
                std::cout << "   -> INVALID NUMBERS" << std::endl;
                continue;
            }

            FingeringRule& rule = m_fingeringMap[(uint8_t)mask];
            rule.mainNote = note;

            // Если есть 4 токена
            if (tokens.size() >= 4) {
                int hhId = parseNumber(tokens[2]);
                int hhNote = parseNumber(tokens[3]);
                
                if (hhId >= 0 && hhNote >= 0) {
                    rule.halfHoleRules[hhId] = hhNote;
                    std::cout << "   -> ADDED HH RULE: Sensor=" << hhId << " Note=" << hhNote << std::endl;
                } else {
                    std::cout << "   -> ERROR: HH values invalid" << std::endl;
                }
            } else {
                std::cout << "   -> Added Simple Rule (No HH)" << std::endl;
            }
            loadedCount++;
        } else {
            std::cout << "   -> SKIP: Not enough tokens" << std::endl;
        }
    }
    std::cout << "[AppFingering] Finished parsing. Total rules: " << loadedCount << std::endl;
    LOG_INFO(TAG, "Loaded %d fingering rules.", loadedCount);
}

int AppFingering::findNote(uint8_t mask, int halfHoleSensorId) {
    if (m_fingeringMap.count(mask) == 0) {
        std::cout << "[AppFingering] Mask " << (int)mask << " NOT FOUND in map" << std::endl;
        return 0; 
    }

    const FingeringRule& rule = m_fingeringMap.at(mask);

    if (halfHoleSensorId != -1) {
        // ДЕТАЛЬНАЯ ОТЛАДКА ПОИСКА
        std::cout << "[AppFingering] Checking HH for Mask " << (int)mask 
                  << ", Sensor " << halfHoleSensorId << ". Rules in map: " 
                  << rule.halfHoleRules.size() << std::endl;
        
        if (rule.halfHoleRules.count(halfHoleSensorId)) {
            int hhNote = rule.halfHoleRules.at(halfHoleSensorId);
            std::cout << "[AppFingering] MATCH! HH Note: " << hhNote << std::endl;
            return hhNote;
        } else {
            std::cout << "[AppFingering] NO MATCH. Returning Main Note." << std::endl;
        }
    }

    return rule.mainNote;
}

void AppFingering::publishNote(int note) {
    if (note == m_lastPublishedNote) {
        std::cout << "[AppFingering] Skip duplicate note " << note << std::endl;
        return;
    }

    m_lastPublishedNote = note;
    std::cout << "[AppFingering] Publish Note: " << note << std::endl;

    if (m_dispatcher) {
        Event ev(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{note});
        m_dispatcher->postEvent(ev);
    }
}
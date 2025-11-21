/*
 * AppFingering.cpp
 *
 * Реализация транслятора аппликатуры.
 * Читает fingering.cfg и преобразует события маски/жестов в ноты.
 *
 * Этот модуль отвечает за интерпретацию физического состояния сенсоров
 * (маска нажатий + жесты) в музыкальные намерения (ноты).
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
#include <iostream> // Для отладки в Native
#include <cctype>   // Для isspace

#define TAG "AppFingering"

// --- Вспомогательные функции ---

/**
 * @brief Удаляет пробельные символы (включая \r, \n) с обоих концов строки.
 * Критично для кроссплатформенного парсинга конфигов (Windows/Linux).
 */
static std::string trim(const std::string& str) {
    const char* whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos) return ""; // Строка состоит только из пробелов
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

/**
 * @brief Парсит строку с числом в разных форматах (dec, hex 0x, bin 0b).
 * Используется для чтения масок и нот из конфигурационного файла.
 * @param str Строковое представление числа.
 * @return int Распаршенное число или -1 в случае ошибки.
 */
static int parseNumber(const std::string& str) {
    // Очищаем токен перед парсингом (важно для последнего токена в строке с \r)
    std::string cleanStr = trim(str);
    try {
        // Проверка на бинарный формат (например, 0b101)
        if (cleanStr.size() > 2 && cleanStr.substr(0, 2) == "0b") {
            return std::stoi(cleanStr.substr(2), nullptr, 2);
        }
        // Проверка на шестнадцатеричный формат (например, 0xFE)
        if (cleanStr.size() > 2 && cleanStr.substr(0, 2) == "0x") {
            return std::stoi(cleanStr.substr(2), nullptr, 16);
        }
        // Обычное десятичное число
        return std::stoi(cleanStr);
    } catch (...) {
        #if defined(NATIVE_TEST)
        std::cerr << "[AppFingering] Parse error for token: '" << cleanStr << "'" << std::endl;
        #endif
        return -1; // Возвращаем ошибку при невозможности преобразования
    }
}

// --- Конструктор ---

AppFingering::AppFingering() 
    : m_dispatcher(nullptr), 
      m_currentMask(0), 
      m_lastPublishedNote(0),
      m_currentHalfHoleId(-1) {
    // Инициализация всех переменных состояния безопасными значениями
}

// --- Инициализация ---

bool AppFingering::init(IHalStorage* storage) {
    // 1. СБРОС СОСТОЯНИЯ
    // Критически важно сбрасывать внутреннее состояние при (ре)инициализации,
    // чтобы при перезагрузке конфига "на лету" не оставались "залипшие" ноты
    // или некорректные маски.
    m_currentMask = 0;
    m_lastPublishedNote = 0;
    m_currentHalfHoleId = -1;
    
    #if defined(NATIVE_TEST)
    std::cout << "[AppFingering] Init. Loading config..." << std::endl;
    #endif

    std::string content;
    // Читаем файл конфигурации аппликатур
    if (storage->readFile("/fingering.cfg", content)) {
        // Файл найден, приступаем к парсингу
        parseFingeringConfig(content);
        return true;
    } else {
        // Критическая ошибка: без карты аппликатур инструмент не может играть.
        LOG_ERROR(TAG, "fingering.cfg not found!");
        return false;
    }
}

// --- Подписка на события ---

void AppFingering::subscribe(EventDispatcher* dispatcher) {
    m_dispatcher = dispatcher;
    if (m_dispatcher) {
        // Подписываемся на изменения маски (основной триггер смены нот).
        // Это событие генерируется AppLogic, когда меняется комбинация полностью закрытых отверстий.
        m_dispatcher->subscribe(EventType::SENSOR_MASK_CHANGED, this);
        
        // Подписываемся на жесты полузакрытия (модификатор ноты).
        // Это событие генерируется AppLogic, когда конкретный сенсор переходит в состояние HALF_HOLE.
        m_dispatcher->subscribe(EventType::HALF_HOLE_DETECTED, this);
    }
}

// --- Обработчик событий ---

void AppFingering::handleEvent(const Event& event) {
    if (event.type == EventType::SENSOR_MASK_CHANGED) {
        // Обновляем текущую маску
        m_currentMask = event.payload.sensorMask.mask;
        
        // ВАЖНОЕ АРХИТЕКТУРНОЕ РЕШЕНИЕ:
        // При смене маски (т.е. при полном закрытии/открытии любого отверстия)
        // мы сбрасываем состояние полузакрытия.
        // Это логично, так как положение рук изменилось глобально, и старый жест
        // полузакрытия, скорее всего, уже не актуален или физически невозможен.
        m_currentHalfHoleId = -1;
        
        #if defined(NATIVE_TEST)
        std::cout << "[AppFingering] Mask Changed -> " << (int)m_currentMask << std::endl;
        #endif
        
        // Ищем новую ноту для новой маски (без модификатора)
        int note = findNote(m_currentMask, m_currentHalfHoleId);
        publishNote(note);
    } 
    else if (event.type == EventType::HALF_HOLE_DETECTED) {
        // Обновляем ID сенсора, который находится в полузакрытом состоянии
        m_currentHalfHoleId = event.payload.halfHole.id;
        
        #if defined(NATIVE_TEST)
        std::cout << "[AppFingering] HalfHole Detected ID: " << m_currentHalfHoleId << std::endl;
        #endif

        // Пересчитываем ноту с учетом ТЕКУЩЕЙ маски и НОВОГО модификатора half-hole.
        // Это позволяет изменить ноту (например, сделать бенд на полтона), не меняя основную маску.
        int note = findNote(m_currentMask, m_currentHalfHoleId);
        publishNote(note);
    }
}

// --- Приватные методы ---

/**
 * @brief Парсит содержимое файла fingering.cfg и заполняет карту аппликатур.
 * Формат строки: MASK NOTE [HH_ID HH_NOTE]
 */
void AppFingering::parseFingeringConfig(const std::string& fileContent) {
    std::istringstream stream(fileContent);
    std::string line;

    // Очищаем старую карту перед загрузкой новой
    m_fingeringMap.clear();
    int loadedCount = 0;

    while (std::getline(stream, line)) {
        // 1. Очистка от комментариев (все, что после #)
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        // Очищаем строку от лишних пробелов
        line = trim(line);
        if (line.empty()) continue;

        // 2. Разбиваем строку на токены вручную.
        std::istringstream iss(line);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        // 3. Анализируем токены
        // Минимально должно быть 2 токена: Маска и Нота
        if (tokens.size() >= 2) {
            int mask = parseNumber(tokens[0]);
            int note = parseNumber(tokens[1]);

            // Пропускаем некорректные строки (или ошибки парсинга)
            if (mask < 0 || note < 0) continue;

            // Получаем или создаем правило для этой маски в карте
            FingeringRule& rule = m_fingeringMap[(uint8_t)mask];
            rule.mainNote = note;

            // Если есть 4 токена - значит это расширенное правило с полузакрытием
            // Формат: MASK NOTE HH_ID HH_NOTE
            if (tokens.size() >= 4) {
                int hhId = parseNumber(tokens[2]);
                int hhNote = parseNumber(tokens[3]);
                
                #if defined(NATIVE_TEST)
                std::cout << "[AppFingering] Loaded HH Rule: Mask=" << mask 
                          << " Note=" << note 
                          << " HH_ID=" << hhId 
                          << " HH_Note=" << hhNote << std::endl;
                #endif

                if (hhId >= 0 && hhNote >= 0) {
                    rule.halfHoleRules[hhId] = hhNote;
                }
            } else {
                #if defined(NATIVE_TEST)
                std::cout << "[AppFingering] Loaded Simple Rule: Mask=" << mask 
                          << " Note=" << note << std::endl;
                #endif
            }
            loadedCount++;
        }
    }
    LOG_INFO(TAG, "Loaded %d fingering rules.", loadedCount);
}

/**
 * @brief Ищет ноту в карте аппликатур.
 * @param mask Текущая битовая маска закрытых отверстий.
 * @param halfHoleSensorId ID сенсора в состоянии полузакрытия (-1 если нет).
 * @return int MIDI-нота (0-127).
 */
int AppFingering::findNote(uint8_t mask, int halfHoleSensorId) {
    // 1. Ищем правило для текущей битовой маски
    if (m_fingeringMap.count(mask) == 0) {
        // Маска не найдена в таблице -> Тишина (Note Off)
        #if defined(NATIVE_TEST)
        std::cout << "[AppFingering] Mask " << (int)mask << " NOT FOUND in map -> Note 0" << std::endl;
        #endif
        return 0; 
    }

    const FingeringRule& rule = m_fingeringMap.at(mask);

    // 2. Приоритет: Проверяем модификатор Полузакрытия.
    // Если есть активное полузакрытие И для этой маски есть правило для этого сенсора
    if (halfHoleSensorId != -1) {
        if (rule.halfHoleRules.count(halfHoleSensorId)) {
            int hhNote = rule.halfHoleRules.at(halfHoleSensorId);
            
            #if defined(NATIVE_TEST)
            std::cout << "[AppFingering] Matched HH Rule for ID " << halfHoleSensorId 
                      << " -> " << hhNote << std::endl;
            #endif
            
            return hhNote;
        } else {
            #if defined(NATIVE_TEST)
            // Отладка отсутствия правила
            std::cout << "[AppFingering] No HH Rule for ID " << halfHoleSensorId 
                      << " in Mask " << (int)mask << ". Available rules: ";
            if (rule.halfHoleRules.empty()) {
                std::cout << "None";
            } else {
                for (auto const& [key, val] : rule.halfHoleRules) {
                    std::cout << "{" << key << "->" << val << "} ";
                }
            }
            std::cout << std::endl;
            #endif
        }
    }

    // 3. Иначе возвращаем основную ноту для этой маски
    #if defined(NATIVE_TEST)
    std::cout << "[AppFingering] Using Main Note: " << rule.mainNote << std::endl;
    #endif
    
    return rule.mainNote;
}

/**
 * @brief Публикует событие NOTE_PITCH_SELECTED.
 * Реализует дедупликацию, чтобы не спамить одинаковыми нотами.
 */
void AppFingering::publishNote(int note) {
    // Дедупликация: если нота не изменилась, ничего не делаем.
    if (note == m_lastPublishedNote) {
        #if defined(NATIVE_TEST)
        std::cout << "[AppFingering] Skip duplicate note " << note << std::endl;
        #endif
        return;
    }

    m_lastPublishedNote = note;

    #if defined(NATIVE_TEST)
    std::cout << "[AppFingering] Publish Note: " << note << std::endl;
    #endif

    if (m_dispatcher) {
        Event ev(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{note});
        m_dispatcher->postEvent(ev);
    }
}
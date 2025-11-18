/*
 * Logger.h
 *
 * Соответствует: docs/modules/diagnostics_logging.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.1
 */
#pragma once 

#include "core/ConfigManager.h"
#include "interfaces/IHalUsb.h"
#include "interfaces/IHalSystem.h"
#include "LogLevel.h"

// Forward-declare FreeRTOS типы
typedef void* SemaphoreHandle_t;

class Logger {
public:
    /**
     * @brief Получение единственного экземпляра (Singleton).
     */
    static Logger* getInstance();

    /**
     * @brief Инициализация логгера.
     */
    void init(ConfigManager* configManager, IHalUsb* halUsb, IHalSystem* halSystem);

    /**
     * @brief Основной метод логирования (обычно вызывается через макросы).
     */
    void log(LogLevel level, const char* tag, const char* format, ...);

private:
    Logger(); // Приватный конструктор
    ~Logger(); // Приватный деструктор

    // Запрет копирования
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static Logger* m_instance;
    ConfigManager* m_configManager;
    IHalUsb* m_halUsb;
    IHalSystem* m_halSystem;

    LogLevel m_logLevel;
    SemaphoreHandle_t m_logMutex; // Mutex для защиты m_halUsb
};

// --- Глобальные Макросы ---
// (Они должны быть здесь, чтобы быть доступными глобально)

#define LOG_DEBUG(TAG, format, ...) Logger::getInstance()->log(LogLevel::DEBUG, TAG, format, ##__VA_ARGS__)
#define LOG_INFO(TAG, format, ...)  Logger::getInstance()->log(LogLevel::INFO,  TAG, format, ##__VA_ARGS__)
#define LOG_WARN(TAG, format, ...)  Logger::getInstance()->log(LogLevel::WARN,  TAG, format, ##__VA_ARGS__)
#define LOG_ERROR(TAG, format, ...) Logger::getInstance()->log(LogLevel::ERROR, TAG, format, ##__VA_ARGS__)
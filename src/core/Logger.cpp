/*
 * Logger.cpp
 *
 * Реализация для Logger (Singleton).
 * Отвечает за форматирование логов, добавление timestamp и отправку в HAL.
 *
 * Соответствует: docs/modules/diagnostics_logging.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.5
 */

#include "core/Logger.h"
#include <cstdio>   // vsnprintf
#include <cstdarg>  // va_list
#include <cstring>  // strlen
#include <vector>
#include <string>

// --- Настройка окружения (RTOS vs Native) ---
#if defined(ESP32_TARGET)
    #include "freertos/FreeRTOS.h"
    #include "freertos/semphr.h"
#elif defined(NATIVE_TEST)
    // Эмуляция типов и функций FreeRTOS для тестов на ПК
    typedef void* SemaphoreHandle_t;
    #define xSemaphoreCreateMutex() ((void*)1) // Возвращаем не-null
    #define xSemaphoreTake(mutex, ticks) (true) // Всегда успешно
    #define xSemaphoreGive(mutex) (true)
    #define vSemaphoreDelete(mutex) ((void)0)
    #define portMAX_DELAY 0xFFFFFFFF
    
    // Для отладки тестов можно раскомментировать:
    // #include <iostream> 
#endif

// --- Статические члены ---
Logger* Logger::m_instance = nullptr;

// --- Вспомогательная функция для преобразования уровня в строку ---
static const char* levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

// --- Реализация Синглтона ---
Logger* Logger::getInstance() {
    if (m_instance == nullptr) {
        m_instance = new Logger();
    }
    return m_instance;
}

// --- Конструктор/Деструктор ---
Logger::Logger() 
    : m_configManager(nullptr), 
      m_halUsb(nullptr), 
      m_halSystem(nullptr),
      m_logLevel(LogLevel::INFO), // Дефолтный уровень (пока не init)
      m_logMutex(nullptr) {
}

Logger::~Logger() {
    if (m_logMutex) {
        vSemaphoreDelete(m_logMutex);
    }
}

// --- Инициализация ---
void Logger::init(ConfigManager* configManager, IHalUsb* halUsb, IHalSystem* halSystem) {
    m_configManager = configManager;
    m_halUsb = halUsb;
    m_halSystem = halSystem;

    if (m_configManager) {
        m_logLevel = m_configManager->getLogLevel();
    }

    // Создаем мьютекс для защиты serialPrint от одновременного доступа разных задач
    if (!m_logMutex) {
        m_logMutex = xSemaphoreCreateMutex();
    }
}

// --- Главный метод логгирования ---
void Logger::log(LogLevel level, const char* tag, const char* format, ...) {
    // 1. Быстрая проверка уровня (фильтрация)
    if (level < m_logLevel) {
        return;
    }

    // Если логгер не инициализирован, мы не можем работать (или падаем, или игнорируем)
    // Для безопасности игнорируем, но в debug-сборке можно ассертить.
    if (!m_halUsb || !m_halSystem) return;

    // 2. Захват Мьютекса (Thread Safety)
    if (xSemaphoreTake(m_logMutex, portMAX_DELAY)) {
        
        // 3. Получение времени
        uint32_t timestamp = m_halSystem->getSystemTimestampMs();

        // 4. Форматирование сообщения пользователя
        // Используем статический буфер для экономии кучи, но осторожно с переполнением
        // Размер 256 байт обычно достаточен для строки лога
        char msgBuffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(msgBuffer, sizeof(msgBuffer), format, args);
        va_end(args);

        // 5. Форматирование финальной строки: "TIMESTAMP - TAG - LEVEL - MESSAGE"
        // Пример: "12345 - app/logic - INFO - SensorMaskChanged: 0b1101"
        char finalBuffer[320]; // Чуть больше для метаданных
        snprintf(finalBuffer, sizeof(finalBuffer), "%lu - %s - %s - %s", 
                 (unsigned long)timestamp, 
                 tag, 
                 levelToString(level), 
                 msgBuffer);

        // 6. Отправка в HAL (USB Serial)
        m_halUsb->serialPrint(std::string(finalBuffer));

        // 7. Освобождение Мьютекса
        xSemaphoreGive(m_logMutex);
    }
}
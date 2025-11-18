/*
 * Logger.cpp
 *
 * Реализация (Заглушка) для Logger (Singleton)
 *
 * Соответствует: docs/modules/diagnostics_logging.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.2
 */
#include "core/Logger.h"
#include <iostream> // Для std::cout в native
#include <cstdarg>  // Для va_list

// (Включаем заголовки FreeRTOS или их Mock-заглушки для [env:native])
#if defined(NATIVE_TEST)
    // (Заглушки для Mutex)
    typedef void* SemaphoreHandle_t;
    #define xSemaphoreCreateMutex() (void*)1 // Просто возвращаем не-null
    #define xSemaphoreTake(mutex, ticks) (true)
    #define xSemaphoreGive(mutex) (true)
    #define portMAX_DELAY 0
#else
    #include "freertos/FreeRTOS.h"
    #include "freertos/semphr.h"
#endif

// --- Реализация Синглтона ---
Logger* Logger::m_instance = nullptr;

Logger* Logger::getInstance() {
    if (m_instance == nullptr) {
        m_instance = new Logger();
    }
    return m_instance;
}

Logger::Logger() 
    : m_configManager(nullptr), 
      m_halUsb(nullptr), 
      m_halSystem(nullptr),
      m_logLevel(LogLevel::NONE),
      m_logMutex(nullptr) {
}

Logger::~Logger() {
    // (TBD: vSemaphoreDelete(m_logMutex))
}

// --- Public API ---

void Logger::init(ConfigManager* configManager, IHalUsb* halUsb, IHalSystem* halSystem) {
    m_configManager = configManager;
    m_halUsb = halUsb;
    m_halSystem = halSystem;
    m_logLevel = LogLevel::DEBUG; // (Заглушка до Спринта 2.8)
    
    // (TBD в Спринте 2.8: m_logLevel = m_configManager->getLogLevel();)
    
    m_logMutex = xSemaphoreCreateMutex();
}

void Logger::log(LogLevel level, const char* tag, const char* format, ...) {
    // (TBD в Спринте 2.8: Реальная реализация)
    
    // --- Временная заглушка (просто выводим в stdout, если native) ---
    #if defined(NATIVE_TEST)
        // (Простая реализация для native, чтобы видеть, что что-то происходит)
        if (level >= m_logLevel) {
            char buffer[256];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            
            // (Эмулируем получение timestamp)
            uint32_t ts = (m_halSystem != nullptr) ? m_halSystem->getSystemTimestampMs() : 0;
            
            std::cout << ts << " - " << tag << " - " << buffer << std::endl;

            // (Эмулируем вызов HAL USB)
            if (m_halUsb != nullptr) {
                m_halUsb->serialPrint(buffer);
            }
        }
    #endif
    // --- Конец временной заглушки ---
}
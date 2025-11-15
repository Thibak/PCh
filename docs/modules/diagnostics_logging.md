# **Детальное описание модуля: diagnostics & logging**

**Назначение документа:** \> Этот документ описывает внутреннюю логику, API и зависимости централизованной службы логирования.

## **1\. Назначение**

Служба Logger (Логгер) предоставляет единый, потокобезопасный механизм для сбора, фильтрации и вывода лог-сообщений от *всех* модулей системы.

Ее задачи:

1. Предоставить глобальные макросы (напр. LOG\_INFO, LOG\_ERROR) для легкого вызова из любого места.  
2. Фильтровать сообщения на основе уровня log\_level, полученного из core/config\_manager.  
3. Форматировать сообщения в единый строковый формат (согласно ARCH\_CONTRACT.MD).  
4. Направлять отформатированные строки в hal\_usb для вывода в Serial (CDC).

## **2\. Зависимости**

* **\#include "core/config\_manager.h":** (Критическая) Используется для получения getLogLevel().  
* **\#include "hal\_interfaces/i\_hal\_usb.h":** (Критическая) Используется для вывода отформатированной строки.
* **`#include "hal_interfaces/i_hal_system.h"`:** (Критическая, Новое) Используется для получения `getSystemTimestampMs()`.
* **FreeRTOS:** Mutex (для обеспечения потокобезопасности доступа к hal\_usb из разных задач).  
* **stdarg.h:** Для обработки printf-подобных аргументов в макросах.

## **3\. Логика работы**

### **3.1. Глобальные Макросы (Logger.h)**

Модули не должны напрямую вызывать класс Logger. Вместо этого они используют макросы. Макросы автоматически подставляют имя модуля (напр. TAG) и вызывают синглтон Logger.

// (Пример в Logger.h)

// Каждый .cpp файл определяет свой TAG  
// (Пример в app\_logic.cpp)  
// \#define TAG "app/logic"

\#define LOG\_DEBUG(TAG, format, ...) Logger::getInstance()-\>log(LogLevel::DEBUG, TAG, format, \#\#\_\_VA\_ARGS\_\_)  
\#define LOG\_INFO(TAG, format, ...)  Logger::getInstance()-\>log(LogLevel::INFO,  TAG, format, \#\#\_\_VA\_ARGS\_\_)  
\#define LOG\_WARN(TAG, format, ...)  Logger::getInstance()-\>log(LogLevel::WARN,  TAG, format, \#\#\_\_VA\_ARGS\_\_)  
\#define LOG\_ERROR(TAG, format, ...) Logger::getInstance()-\>log(LogLevel::ERROR, TAG, format, \#\#\_\_VA\_ARGS\_\_)

### **3.2. Логика класса Logger (Singleton)**

1. **Фаза init():**  
   * Logger (который является синглтоном) получает указатели на ConfigManager, IHalUsb и IHalSystem.  
   * Logger::init(ConfigManager\* cfg, IHalUsb\* usb, IHalSystem\* system)
   * Он сохраняет эти указатели у себя.  
   * Он создает Mutex (мьютекс) FreeRTOS для защиты m\_halUsb от одновременного доступа.  
   * Он получает и кэширует текущий m\_logLevel \= cfg-\>getLogLevel().  
2. **Фаза log() (Работа):**  
   * Любой модуль (напр. app/logic) вызывает LOG\_INFO("app/logic", "Mask changed: 0b%s", mask\_str).  
   * Макрос вызывает Logger::getInstance()-\>log(...).  
   * **Внутри log():**  
     1. Метод немедленно проверяет: if (level \< m\_logLevel) return; (Быстрая отсечка, если уровень лога недостаточен).  
     2. **Захват Mutex:** xSemaphoreTake(m\_logMutex, portMAX\_DELAY). (Теперь только этот поток может выводить в USB).  
     3. Форматирует строку message (напр. "Mask changed: 0b1101") с помощью vsnprintf.  
     4. Форматирует финальную строку вывода logLine (используя ts) согласно ARCH\_CONTRACT.MD 4.1:  
        * \[timestamp\] \- \[TAG\] \- \[Level\] \- \[message\]  
        * (напр. 12345 \- app/logic \- INFO \- Mask changed: 0b1101)  
     5. Вызывает m\_halUsb-\>serialPrint(logLine).  
     6. **Освобождение Mutex:** xSemaphoreGive(m\_logMutex).

## **4\. Публичный API (C++ Header)**

```cpp
#pragma once

#include "core/config_manager.h"  
#include "hal_interfaces/i_hal_usb.h"  
#include "LogLevel.h"  
#include "freertos/FreeRTOS.h"  
#include "freertos/semphr.h" // Для Mutex

class Logger {  
public:  
    /**  
     * @brief Получение единственного экземпляра (Singleton).  
     */  
    static Logger* getInstance();

/**
     * @brief Инициализация логгера. (Обновлено)
     */
    void init(ConfigManager* configManager, IHalUsb* halUsb, IHalSystem* halSystem);

    /**  
     * @brief Основной метод логирования (обычно вызывается через макросы).  
     */  
    void log(LogLevel level, const char* tag, const char* format, ...);

private:  
    Logger(); // Приватный конструктор  
    ~Logger(); // Приватный деструктор

    static Logger* m_instance;  
    ConfigManager* m_configManager;  
    IHalUsb* m_halUsb;
    IHalSystem* m_halSystem;

    LogLevel m_logLevel;  
    SemaphoreHandle_t m_logMutex; // Mutex для защиты m_halUsb  
};

// \--- Глобальные Макросы \---  
// (см. Раздел 3.1)
```

## **5\. Тестирование (Host-First)**

* Unit-тест (Спринт 2.7) должен проверить:  
  1. **Сценарий 1 (Уровень достаточен):**  
     * MockConfigManager возвращает LogLevel::INFO.  
     * Logger инициализируется с MockHalUsb и MockConfigManager.  
     * Тест вызывает LOG\_INFO("test", "Message").  
     * **Ожидание:** MockHalUsb-\>serialPrint() был вызван 1 раз с отформатированной строкой.  
  2. **Сценарий 2 (Уровень недостаточен):**  
     * MockConfigManager возвращает LogLevel::WARN.  
     * Logger инициализируется.  
     * Тест вызывает LOG\_INFO("test", "Message").  
     * **Ожидание:** MockHalUsb-\>serialPrint() не был вызван.  
  3. **Сценарий 3 (Уровень достаточен):**  
     * MockConfigManager возвращает LogLevel::WARN.  
     * Logger инициализируется.  
     * Тест вызывает LOG\_WARN("test", "Warning\!").  
     * **Ожидание:** MockHalUsb-\>serialPrint() был вызван 1 раз.
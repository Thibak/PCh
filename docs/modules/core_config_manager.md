# **Детальное описание модуля: core/config\_manager**

**Назначение документа:** \> Этот документ описывает внутреннюю логику, API и зависимости модуля core/config\_manager.

## **1\. Назначение**

ConfigManager — это **центральный поставщик** настроек в системе.

Его **единственная задача** — при инициализации (на старте) прочитать файл settings.cfg (используя i\_hal\_storage), распарсить его и хранить все настройки в виде типизированных переменных в RAM.

Все остальные модули (app/logic, hal\_sensors, diagnostics\_logging) обращаются к ConfigManager за настройками, а не читают файл settings.cfg напрямую. Это гарантирует единый источник правды.

## **2\. Зависимости**

* **\#include "i\_hal\_storage.h":** Используется во время init() для чтения файла settings.cfg.  
* **\#include "docs/CONFIG\_SCHEMA.md" (Концептуальная):** Логика парсинга в init() основана на структуре settings.cfg, описанной в этом документе.

## **3\. Логика работы**

### **3.1. Инициализация (Фаза init())**

1. Модуль core/scheduler (или main.cpp) создает экземпляр ConfigManager.  
2. Вызывается метод configManager-\>init(IHalStorage\* storage).  
3. ConfigManager вызывает storage-\>readFile("/settings.cfg").  
4. **Сценарий 1 (Успех):** Файл найден.  
   * ConfigManager построчно парсит INI-подобный текст.  
   * Он распознает секции \[system\], \[sensors\] и т.д.  
   * Он преобразует строковые значения ("INFO", "50", "T1,T2,T3") в их *типы* (enum, int, std::vector\<string\>) и сохраняет в приватных полях.  
   * **Пример парсинга hole\_sensor\_ids:** hole\_sensor\_ids \= 0,1,2,3,4,5,6,7 будет прочитано, разделено по запятой и сохранено как std::vector\<int\>{0, 1, 2, 3, 4, 5, 6, 7}.  
5. **Сценарий 2 (Файл не найден):**  
   * storage-\>readFile возвращает ошибку.  
   * ConfigManager **не падает**. Он логирует LOG\_WARN ("settings.cfg не найден, используются значения по умолчанию").  
   * Модуль загружает в приватные поля **жестко заданные значения по умолчанию**, как описано в CONFIG\_SCHEMA.md.  
6. init() завершается. Модуль готов к работе.

### **3.2. Работа (Фаза run())**

После init() модуль не выполняет активных действий, а только отвечает на вызовы API (геттеры).

## **4\. Публичный API (C++ Header) (Обновлено)**

\#pragma once

\#include \<vector\>  
\#include \<string\>  
\#include "i\_hal\_storage.h" // Для init()  
\#include "LogLevel.h" // enum

class ConfigManager {  
public:  
    ConfigManager();  
      
    /\*\*  
     \* @brief Инициализирует менеджер, читая конфиг из хранилища.  
     \* @param storage Указатель на реализацию i\_hal\_storage.  
     \* @return true, если конфиг успешно загружен и распарсен (или если успешно загружены defaults).  
     \*/  
    bool init(IHalStorage\* storage);

    // \--- \[system\] \---  
    LogLevel getLogLevel() const;  
    int getAutoOffTimeMin() const;  
    std::string getLedPin() const; // \<-- (Новый)

    // \--- \[sensors\] \---  
    const std::vector\<std::string\>& getPhysicalPins() const;  
    int getSampleRateHz() const;  
    int getMuteThreshold() const;

    // \--- \[app\_logic\] \---  
    int getMuteSensorId() const;  
    const std::vector\<int\>& getHoleSensorIds() const;  
      
    // \--- \[gestures\] \---  
    float getVibratoFreqMin() const;  
    float getVibratoFreqMax() const;  
    int getVibratoAmplitudeMin() const;  
    int getHalfHoleThreshold() const;

private:  
    /\*\*  
     \* @brief Внутренний метод парсинга.  
     \*/  
    void parseConfig(const std::string& fileContent);  
      
    /\*\*  
     \* @brief Загружает жестко заданные defaults, если parseConfig не удался.  
     \*/  
    void loadDefaults();

    // Приватные поля для хранения распарсенных значений  
    LogLevel m\_logLevel;  
    int m\_autoOffTimeMin;  
    std::string m\_ledPin; // \<-- (Новый)  
    std::vector\<std::string\> m\_physicalPins;  
    int m\_sampleRateHz;  
    int m\_muteThreshold;  
    int m\_muteSensorId;  
    std::vector\<int\> m\_holeSensorIds;  
    float m\_vibratoFreqMin;  
    float m\_vibratoFreqMax;  
    int m\_vibratoAmplitudeMin;  
    int m\_halfHoleThreshold;  
};

## **5\. Тестирование (Host-First)**

* В \[env:native\] ConfigManager будет передан MockHalStorage.  
* Unit-тест (Спринт 2.2) должен проверить:  
  1. **Сценарий 1 (Успех):**  
     * MockHalStorage возвращает тестовый settings.cfg.  
     * Тест вызывает init().  
     * Тест проверяет, что getLogLevel() возвращает LogLevel::INFO, getHoleSensorIds() возвращает \[0, 1, 2, ...\] и т.д.  
  2. **Сценарий 2 (Файл не найден):**  
     * MockHalStorage возвращает false (ошибка чтения).  
     * Тест вызывает init().  
     * Тест проверяет, что getLogLevel() возвращает LogLevel::WARN (значение по умолчанию).
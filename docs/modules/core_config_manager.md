# **Детальное описание модуля: `core/config_manager`**

**Назначение документа:** \> Этот документ описывает внутреннюю логику, API и зависимости модуля `core/config_manager`.

## **1\. Назначение**

`ConfigManager` — это **центральный поставщик** настроек в системе.

Его **единственная задача** — при инициализации (на старте) прочитать файл settings.cfg (используя `i_hal_storage`), распарсить его и хранить все настройки в виде типизированных переменных в RAM.

Все остальные модули (`app/logic`, `hal_sensors`, `diagnostics_logging`) обращаются к `ConfigManager` за настройками, а не читают файл `settings.cfg` напрямую. Это гарантирует единый источник правды.

## **2\. Зависимости**

* **`#include "i_hal_storage.h"`:** Используется во время `init()` для чтения файла `settings.cfg`.  
* **Концептуальная зависимость:** Логика парсинга в `init()` основана на структуре, описанной в `docs/CONFIG_SCHEMA.md`.

## **3\. Логика работы**

### **3.1. Инициализация (Фаза `init()`)**

1. Модуль `core/scheduler` (или `main.cpp`) создает экземпляр `ConfigManager`.  
2. Вызывается метод `configManager->init(IHalStorage* storage)`.  
3. `ConfigManager` вызывает `storage->readFile("/settings.cfg")`.  
4. **Сценарий 1 (Успех):** Файл найден.  
   * `ConfigManager` построчно парсит INI-подобный текст.  
   * Он распознает секции `[system]`, `[led]`, `[sensors]` и т.д.  
   * Он преобразует строковые значения `("INFO", "50", "T1,T2,T3")` в их *типы* `(enum, int, std::vector<string>)` и сохраняет в приватных полях.  
   * **Пример парсинга hole_sensor_ids:** `hole_sensor_ids = 0,1,2,3,4,5,6,7` будет прочитано, разделено по запятой и сохранено как `std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7}`.  
5. **Сценарий 2 (Файл не найден):**  
   * `storage->readFile` возвращает ошибку.  
   * `ConfigManager` **не падает**. Он логирует `LOG_WARN ("settings.cfg не найден, используются значения по умолчанию")`.  
   * Модуль загружает в приватные поля **жестко заданные значения по умолчанию**, как описано в `CONFIG_SCHEMA.md`.  
6. `init()` завершается. Модуль готов к работе.

### **3.2. Работа (Фаза `run()`)**

После `init()` модуль не выполняет активных действий, а только отвечает на вызовы API (геттеры).

## **4\. Публичный API (C++ Header)** 

```cpp
#pragma once

#include <vector>  
#include <string>  
#include "i_hal_storage.h" // Для init()  
#include "LogLevel.h" // enum

class ConfigManager {  
public:  
    ConfigManager();  
      
    /**  
     * @brief Инициализирует менеджер, читая конфиг из хранилища.  
     * @param storage Указатель на реализацию i_hal_storage.  
     * @return true, если конфиг успешно загружен и распарсен (или если успешно загружены defaults).  
     */  
    bool init(IHalStorage* storage);

    // --- [system] ---  
    LogLevel getLogLevel() const;  
    int getAutoOffTimeMin() const;  
    std::string getLedPin() const;
    float getBasePitchHz() const;

    // --- [led] ---  
    int getLedBlinkDurationMs() const;  
    int getLedBlinkPauseMs() const;

    // --- [sensors] ---  
    const std::vector<std::string>& getPhysicalPins() const;  
    int getSampleRateHz() const;  
    float getFilterAlpha() const;  
    int getMuteThreshold() const;  
    int getHoleClosedThreshold() const; 

    // --- [app_logic] ---  
    int getMuteSensorId() const;  
    const std::vector<int>& getHoleSensorIds() const;  
      
    // --- [gestures] ---  
    float getVibratoFreqMin() const;  
    float getVibratoFreqMax() const;  
    int getVibratoAmplitudeMin() const;  
    int getHalfHoleThreshold() const;

private:  
    /**  
     * @brief Внутренний метод парсинга.  
     */  
    void parseConfig(const std::string& fileContent);  
      
    /**  
     * @brief Загружает жестко заданные defaults, если parseConfig не удался.  
     */  
    void loadDefaults();

    // Приватные поля для хранения распарсенных значений  
    LogLevel m_logLevel;  
    int m_autoOffTimeMin;  
    std::string m_ledPin; 
    float m_basePitchHz; 
      
    int m_ledBlinkDurationMs;  
    int m_ledBlinkPauseMs;

    std::vector<std::string> m_physicalPins;  
    int m_sampleRateHz;  
    float m_filterAlpha;  
    int m_muteThreshold;  
    int m_holeClosedThreshold;
    int m_muteSensorId;  
    std::vector<int> m_holeSensorIds;  
    float m_vibratoFreqMin;  
    float m_vibratoFreqMax;  
    int m_vibratoAmplitudeMin;  
    int m_halfHoleThreshold;  
};
```

## **5\. Тестирование (Host-First)**

* В `[env:native]` `ConfigManager` будет передан `MockHalStorage`.  
* Unit-тест (Спринт 2.2) должен проверить:  
  1. **Сценарий 1 (Успех):**  
     * `MockHalStorage` возвращает тестовый `settings.cfg`.  
     * Тест вызывает `init()`.  
     * Тест проверяет, что `getLogLevel()` возвращает `LogLevel::INFO`, `getHoleClosedThreshold()` возвращает `400`; `LogLevel::INFO`, `getBasePitchHz()` возвращает `440.0` и т.д.  
  2. **Сценарий 2 (Файл не найден):**  
     * `MockHalStorage` возвращает `false` (ошибка чтения).  
     * Тест вызывает `init()`.  
     * Тест проверяет, что `getLogLevel()` возвращает `LogLevel::WARN` (default) и `getHoleClosedThreshold()` возвращает `400` (default).
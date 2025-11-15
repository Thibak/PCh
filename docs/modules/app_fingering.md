# **Детальное описание модуля: app/fingering**

**Назначение документа:** \> Этот документ описывает внутреннюю логику, API и зависимости модуля AppFingering.

## **1\. Назначение**

Модуль AppFingering (Транслятор Аппликатуры) — это "словарь" системы.

Его **единственная задача** — транслировать (переводить) события `SENSOR_MASK_CHANGED` (какие сенсоры *сейчас* нажаты) и `HALF_HOLE_DETECTED` (жест "полузакрытие") в конкретную MIDI-ноту, используя правила из файла `fingering.cfg`.

* AppFingering **не знает** о физических пинах (T1), порогах срабатывания или вибрато.  
* Он получает `SensorMaskChanged(0b11111110)` от `app/logic`.  
* Он ищет эту маску в своей карте, которую он загрузил из `fingering.cfg` при старте.  
* Он публикует `NotePitchSelected(62)`.

## **2\. Зависимости (Обновлено)**

* **\#include "hal\_interfaces/i\_hal\_storage.h":** (Критическая) Используется в init() для *однократного* чтения fingering.cfg.  
* **\#include "core/event\_dispatcher.h":** (Критическая) Используется для *подписки* на SENSOR\_MASK\_CHANGED / HALF\_HOLE\_DETECTED и *публикации* NOTE\_PITCH\_SELECTED.  
* **Концептуальная зависимость:** Логика парсинга в init() основана на структуре, описанной в docs/CONFIG\_SCHEMA.md.  
* **\#include "diagnostics/logger.h":** Для логирования (LOG\_INFO, LOG\_ERROR).

## **3\. Логика работы**

### **3.1. Структуры данных (Карта аппликатур)**

Для быстрой трансляции AppFingering будет использовать std::map (или std::unordered\_map для скорости), который загружается в RAM при старте.

```cpp
// (Примерная структура данных в AppFingering.cpp)

// Структура для хранения одного правила (обычная нота + опциональное полузакрытие)  
struct FingeringRule {  
    int mainNote; // MIDI-нота (0-127), 0 = Note Off  
      
    // Карта <sensor_id, note> для правил полузакрытия  
    // (напр., m_halfHoleRules[1] = 63)  
    std::map\<int, int\> halfHoleRules;   
      
    FingeringRule() : mainNote(0) {}  
};

// Главная карта аппликатур  
// Key: 8-битная маска (0b11111110)  
// Value: Правило (нота 62 + правила полузакрытия)  
std::map<uint8_t, FingeringRule> m_fingeringMap;
```

### **3.2. Фаза init(IHalStorage\* storage)**

1. Вызывается core/scheduler (Фаза 4).  
2. AppFingering вызывает storage-\>readFile("/fingering.cfg").  
3. **Сценарий 1 (Успех):** Файл найден.  
   * Вызывается внутренний метод parseFingeringConfig(fileContent).  
   * LOG\_INFO(TAG, "Loaded %d fingering rules.", m\_fingeringMap.size()).  
4. **Сценарий 2 (Файл не найден):**  
   * LOG\_ERROR(TAG, "fingering.cfg not found\! No notes will be played.")  
   * m\_fingeringMap остается пустым.

### **3.3. Внутренний parseFingeringConfig(content)**

1. Идет по content строка за строкой.  
2. Пропускает пустые строки и строки, начинающиеся с \#.  
3. Разбивает строку на токены (напр., 0b11111110, 62, 1, 63).  
4. Парсит MASK (напр., 0b11111110) в uint8\_t mask \= 0xFE;.  
5. Парсит NOTE (напр., 62\) в int note \= 62;.  
6. Создает FingeringRule rule; rule.mainNote \= note;.  
7. Если есть 3-й и 4-й токены (1, 63):  
   * Парсит int sensorId \= 1;  
   * Парсит int hhNote \= 63;  
   * rule.halfHoleRules\[sensorId\] \= hhNote;  
8. Сохраняет правило: m\_fingeringMap\[mask\] \= rule;  
9. (Повторяет для всех строк)

### **3.4. Фаза handleEvent(const Event& event)**

1. AppFingering является IEventHandler и получает события от core/event\_dispatcher.  
2. switch (event.type):  
   * case EventType::SENSOR\_MASK\_CHANGED:  
     * m\_currentMask \= event.payload.sensorMask.mask;  
     * int note \= findNote(m\_currentMask);  
     * publishNote(note);  
     * break;  
   * case EventType::HALF\_HOLE\_DETECTED:  
     * int sensorId \= event.payload.halfHole.id;  
     * int note \= findNote(m\_currentMask, sensorId); // Ищем ноту с учетом полузакрытия  
     * publishNote(note);  
     * break;

### **3.5. Внутренние методы findNote() и publishNote()**

1. **int AppFingering::findNote(uint8\_t mask, int halfHoleSensorId \= \-1):**  
   * if (m\_fingeringMap.count(mask) \== 0):  
     * return 0; // NOTE\_OFF (Тишина), если маска не найдена  
   * const FingeringRule& rule \= m\_fingeringMap\[mask\];  
   * if (halfHoleSensorId \!= \-1 && rule.halfHoleRules.count(halfHoleSensorId) \> 0):  
     * return rule.halfHoleRules.at(halfHoleSensorId); // Найдена нота полузакрытия  
   * return rule.mainNote; // Возвращаем обычную ноту  
2. **void AppFingering::publishNote(int note):**  
   * **Защита от "дребезга" нот:**  
   * if (note \== m\_lastPublishedNote) return; // Не спамим, если нота та же  
   * m\_lastPublishedNote \= note;  
   * Event ev { EventType::NOTE\_PITCH\_SELECTED, .payload.notePitch \= { note } };  
   * m\_dispatcher-\>postEvent(ev);

## **4\. Публичный API (C++ Header)**

```cpp
// (AppFingering.h)  
#pragma once

#include "hal_interfaces/i_hal_storage.h"  
#include "core/event_dispatcher.h"  
#include "i_event_handler.h"  
#include <map>

// (Определение структуры FingeringRule)  
struct FingeringRule {  
    int mainNote;  
    std::map<int, int> halfHoleRules;  
    FingeringRule() : mainNote(0) {}  
};

class AppFingering : public IEventHandler {  
public:  
    /**  
     * @param storage Указатель на HAL Storage, откуда будет прочитан fingering.cfg  
     */  
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
```

## **5\. Тестирование (Host-First)**

* **Unit-тест (Спринт 2.5):**  
  1. Создать MockHalStorage (Спринт 1.3).  
  2. mockStorage-\>writeFile("/fingering.cfg", "0b11111110 62 1 63\\n0b11111100 64");  
  3. Создать AppFingering, вызвать appFingering-\>init(mockStorage). Проверить true.  
  4. Создать EventDispatcher, appFingering-\>subscribe(dispatcher). (Нужен Mock-диспетчер или реальный).  
  5. Вызвать appFingering-\>handleEvent(SensorMaskChanged(0b11111110)).  
  6. *Проверить*, что EventDispatcher получил NotePitchSelected(62).  
  7. Вызвать appFingering-\>handleEvent(HalfHoleDetected(1)). (Маска не менялась).  
  8. *Проверить*, что EventDispatcher получил NotePitchSelected(63).  
  9. Вызвать appFingering-\>handleEvent(SensorMaskChanged(0b11111100)).  
  10. *Проверить*, что EventDispatcher получил NotePitchSelected(64).
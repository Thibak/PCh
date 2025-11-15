# **Детальное описание модуля: app/midi**

**Назначение документа:** \> Этот документ описывает внутреннюю логику, API и зависимости модуля AppMidi.

## **1\. Назначение**

Модуль AppMidi (Формирователь MIDI) — это "исполнитель" и "переводчик" на язык MIDI.

Его **единственная задача** — слушать высокоуровневые события-намерения (такие как NOTE\_PITCH\_SELECTED, VIBRATO\_DETECTED, MUTE\_ENABLED) и преобразовывать их в конкретные вызовы API для HAL.

* AppMidi не знает о сенсорах, масках или порогах.  
* Он получает NotePitchSelected(62).  
* Он вызывает m\_halBle-\>sendNoteOn(62) и m\_halLed-\>setMode(BLINK\_ONCE).

Он также отвечает за правильную "музыкальную" логику: отправку NoteOff для предыдущей ноты перед отправкой NoteOn для новой, чтобы избежать "залипших" нот.

## **2\. Зависимости**

* **\#include "hal\_interfaces/i\_hal\_ble.h":** (Критическая) Используется для отправки MIDI-сообщений.  
* **\#include "hal\_interfaces/i\_hal\_led.h":** (Критическая) Используется для индикации BLINK\_ONCE при отправке ноты.  
* **\#include "core/event\_dispatcher.h":** (Критическая) Для подписки на события от app\_logic и app\_fingering.  
* **\#include "diagnostics/logger.h":** Для логирования.

## **3\. Логика работы**

AppMidi — это простой модуль, управляемый событиями. У него нет своей задачи FreeRTOS.

### **3.1. Фаза init(IHalBle\* ble, IHalLed\* led)**

1. Вызывается core/scheduler (Фаза 4).  
2. Сохраняет указатели m\_halBle \= ble и m\_halLed \= led.  
3. Инициализирует состояние: m\_currentNote \= 0; (0 \= Note Off).  
4. Инициализирует состояние: m\_isMuted \= false;.

### **3.2. Фаза handleEvent(const Event& event)**

1. AppMidi является IEventHandler и получает события от core/event\_dispatcher.  
2. switch (event.type):  
   * **case EventType::NOTE\_PITCH\_SELECTED:**  
     * int newNote \= event.payload.notePitch.pitch;  
     * handleNoteChange(newNote);  
     * break;  
   * **case EventType::VIBRATO\_DETECTED:**  
     * if (m\_isMuted) return; // Не отправляем вибрато, если звук выключен  
     * float depth \= event.payload.vibrato.depth;  
     * m\_halBle-\>sendPitchBend(depth);  
     * break;  
   * **case EventType::MUTE\_ENABLED:**  
     * m\_isMuted \= true;  
     * handleNoteChange(0); // Отправляем NoteOff для текущей ноты  
     * m\_halBle-\>sendAllNotesOff(); // Дополнительно посылаем "паническое" CC 123  
     * break;  
   * **case EventType::MUTE\_DISABLED:**  
     * m\_isMuted \= false;  
     * // Ничего не делаем, ждем следующего NOTE\_PITCH\_SELECTED  
     * break;

### **3.3. Внутренний handleNoteChange(int newNote)**

Это ключевая логика, предотвращающая "залипание" нот.

1. **if (m\_isMuted && newNote \> 0):**  
   * return; // Игнорируем новые ноты, если Mute включен  
2. **if (newNote \== m\_currentNote):**  
   * return; // Нота не изменилась, ничего не делаем  
3. **Отправка Note Off (если нужно):**  
   * if (m\_currentNote \> 0):  
     * m\_halBle-\>sendNoteOff(m\_currentNote);  
     * LOG\_DEBUG(TAG, "Note Off: %d", m\_currentNote);  
4. **Отправка Note On (если нужно):**  
   * if (newNote \> 0):  
     * m\_halBle-\>sendNoteOn(newNote);  
     * m\_halLed-\>setMode(LedMode::BLINK\_ONCE); // Моргнуть LED  
     * LOG\_DEBUG(TAG, "Note On: %d", newNote);  
5. **Обновление состояния:**  
   * m\_currentNote \= newNote;

## **4\. Публичный API (C++ Header)**

```cpp
// (AppMidi.h)  
#pragma once

#include "hal_interfaces/i_hal_ble.h"  
#include "hal_interfaces/i_hal_led.h"  
#include "core/event_dispatcher.h"  
#include "i_event_handler.h"

class AppMidi : public IEventHandler {  
public:  
    AppMidi();  
      
    /**  
     * @brief Сохраняет указатели на HAL модули.  
     */  
    bool init(IHalBle* halBle, IHalLed* halLed);

    /**  
     * @brief Подписывает модуль на события от EventDispatcher.  
     */  
    void subscribe(EventDispatcher* dispatcher);

    /**  
     * @brief Обрабатывает NOTE_PITCH_SELECTED, VIBRATO_DETECTED, MUTE_ENABLED/DISABLED.  
     */  
    virtual void handleEvent(const Event& event) override;

private:  
    /**  
     * @brief Реализует логику (NoteOff -> NoteOn) для предотвращения "залипания".  
     */  
    void handleNoteChange(int newNote);

    // Указатели на HAL (внедряются)  
    IHalBle* m_halBle;  
    IHalLed* m_halLed;  
      
    // Переменные состояния  
    int m_currentNote; // Последняя нота, которую мы отправили (0 = Note Off)  
    bool m_isMuted;  
};
```

## **5\. Тестирование (Host-First)**

* **Интеграционный тест (Спринт 2.7):**  
  1. Создать AppMidi, MockHalBle (Спринт 1.6), MockHalLed (Спринт 1.5).  
  2. appMidi-\>init(mockBle, mockLed).  
  3. Создать EventDispatcher, appMidi-\>subscribe(dispatcher).  
  4. **Тест 1 (Простая нота):**  
     * dispatcher-\>postEvent(NotePitchSelected(60)).  
     * **Ожидание:** mockBle-\>getLastNoteOn() \== 60\. mockLed-\>getBlinkOnceCount() \== 1\.  
  5. **Тест 2 (Смена ноты):**  
     * dispatcher-\>postEvent(NotePitchSelected(62)).  
     * **Ожидание:** mockBle-\>getLastNoteOff() \== 60 (сначала выключили старую).  
     * **Ожидание:** mockBle-\>getLastNoteOn() \== 62 (потом включили новую).  
     * **Ожидание:** mockLed-\>getBlinkOnceCount() \== 2\.  
  6. **Тест 3 (Mute):**  
     * dispatcher-\>postEvent(NotePitchSelected(64)). (Играем ноту 64).  
     * dispatcher-\>postEvent(MuteEnabled()).  
     * **Ожидание:** mockBle-\>getLastNoteOff() \== 64\.  
     * **Ожидание:** mockBle-\>getAllNotesOffCount() \> 0\.  
  7. **Тест 4 (Игнор при Mute):**  
     * dispatcher-\>postEvent(NotePitchSelected(65)).  
     * **Ожидание:** mockBle-\>getLastNoteOn() \== 64 (осталась старая, 65 не отправили).
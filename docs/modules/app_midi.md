# **Детальное описание модуля: app/midi**

**Назначение документа:** \> Этот документ описывает внутреннюю логику, API и зависимости модуля `AppMidi`.

## **1\. Назначение**

Модуль `AppMidi` (Формирователь MIDI) — это "исполнитель" и "переводчик" на язык MIDI.

Его **единственная задача** — слушать высокоуровневые события-намерения (такие как `NOTE_PITCH_SELECTED`, `VIBRATO_DETECTED`, `MUTE_ENABLED`) и преобразовывать их в конкретные вызовы API для HAL.

* `AppMidi` не знает о сенсорах, масках или порогах.  
* Он получает `NotePitchSelected(62)`.  
* Он вызывает `m_halBle->sendNoteOn(62)` и `m_halLed->setMode(BLINK_ONCE)`.

Он также отвечает за правильную "музыкальную" логику: отправку NoteOff для предыдущей ноты перед отправкой NoteOn для новой, чтобы избежать "залипших" нот.

## **2\. Зависимости**

* **`#include "hal_interfaces/i_hal_ble.h"`:** (Критическая) Используется для отправки MIDI-сообщений.  
* **`#include "hal_interfaces/i_hal_led.h"`:** (Критическая) Используется для индикации `BLINK_ONCE` при отправке ноты.  
* **`#include "core/event_dispatcher.h"`:** (Критическая) Для подписки на события от `app_logic` и `app_fingering`.  
* **`#include "diagnostics/logger.h"`:** Для логирования.

## **3\. Логика работы**

`AppMidi` — это простой модуль, управляемый событиями. У него нет своей задачи `FreeRTOS`.

### **3.1. Фаза `init(IHalBle* ble, IHalLed* led, float basePitchHz)`**

1. Вызывается `core/scheduler` (Фаза 4).  
2. Сохраняет указатели `m_halBle = ble и m_halLed = led`.  
3. Сохраняет `m_basePitchHz = basePitchHz`.
4. Инициализирует состояние: `m_currentNote = 0;` `(0 = Note Off)`.  
5. Инициализирует состояние: `m_isMuted = false;`.
6. (TBD: Логика для Спринта 2.10) Если `m_basePitchHz` не `440.0`, подготовить RPN/MTS сообщение.

### **3.2. Фаза `handleEvent(const Event& event)`**

1. `AppMidi` является `IEventHandler` и получает события от `core/event_dispatcher`.  
2. `switch (event.type)`:  
   * **`case EventType::NOTE_PITCH_SELECTED`:**  
     * `int newNote = event.payload.notePitch.pitch`;  
     * `handleNoteChange(newNote)`;  
     * `break`;  
   * **`case EventType::VIBRATO_DETECTED`:**  
     * `if (m_isMuted) return`; // Не отправляем вибрато, если звук выключен  
     * `float depth = event.payload.vibrato.depth`;  
     * `m_halBle->sendPitchBend(depth)`;  
     * `break`;  
   * **`case EventType::MUTE_ENABLED`:**  
     * `m_isMuted = true`;  
     * `handleNoteChange(0)`; // Отправляем `NoteOff` для текущей ноты  
     * `m_halBle->sendAllNotesOff()`; // Дополнительно посылаем "паническое" `CC` `123`  
     * `break`;  
   * **`case EventType::MUTE_DISABLED`:**  
     * `m_isMuted = false`;  
     * // Ничего не делаем, ждем следующего `NOTE_PITCH_SELECTED`  
     * `break`;

### **3.3. Внутренний handleNoteChange(int newNote)**

Это ключевая логика, предотвращающая "залипание" нот.

1. **`if (m_isMuted && newNote > 0)`:**  
   * `return`; // Игнорируем новые ноты, если `Mute` включен  
2. **`if (newNote == m_currentNote)`:**  
   * `return`; // Нота не изменилась, ничего не делаем  
3. **Отправка `Note Off` (если нужно):**  
   * `if (m_currentNote > 0)`:  
     * `m_halBle->sendNoteOff(m_currentNote)`;  
     * `LOG_DEBUG(TAG, "Note Off: %d", m_currentNote)`;  
4. **Отправка Note On (если нужно):**  
   * `if (newNote > 0)`:  
     * `m_halBle->sendNoteOn(newNote)`;  
     * `m_halLed->setMode(LedMode::BLINK_ONCE)`; // Моргнуть LED  
     * `LOG_DEBUG(TAG, "Note On: %d", newNote)`;  
5. **Обновление состояния:**  
   * `m_currentNote = newNote`;

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
    bool init(IHalBle* halBle, IHalLed* halLed, float basePitchHz);

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
    float m_basePitchHz;
};
```

## **5\. Тестирование (Host-First)**

* **Интеграционный тест (Спринт 2.7):**  
  1. Создать `AppMidi`, `MockHalBle` (Спринт 1.6), `MockHalLed` (Спринт 1.5).  
  2. `appMidi->init(mockBle, mockLed, 440.0f)`.  
  3. Создать `EventDispatcher`, `appMidi->subscribe(dispatcher)`.  
  4. **Тест 1 (Простая нота):**  
     * `dispatcher->postEvent(NotePitchSelected(60))`.  
     * **Ожидание:** `mockBle->getLastNoteOn() == 60`. `mockLed->getBlinkOnceCount() == 1`.  
  5. **Тест 2 (Смена ноты):**  
     * `dispatcher->postEvent(NotePitchSelected(62))`.  
     * **Ожидание:** `mockBle->getLastNoteOff() == 60` (сначала выключили старую).  
     * **Ожидание:** `mockBle->getLastNoteOn() == 62` (потом включили новую).  
     * **Ожидание:** `mockLed->getBlinkOnceCount() == 2`.  
  6. **Тест 3 (Mute):**  
     * `dispatcher->postEvent(NotePitchSelected(64))`. (Играем ноту `64`).  
     * `dispatcher->postEvent(MuteEnabled())`.  
     * **Ожидание:** `mockBle->getLastNoteOff() == 64`.  
     * **Ожидание:** `mockBle->getAllNotesOffCount() > 0`.  
  7. **Тест 4 (Игнор при `Mute`):**  
     * `dispatcher->postEvent(NotePitchSelected(65))`.  
     * **Ожидание:** `mockBle->getLastNoteOn() == 64` (осталась старая, `65` не отправили).

  8. **Тест 5 для Спринта 2.10:**
     * `appMidi->init(mockBle, mockLed, 415.0f)`;
     * `mockBle->simulateConnect()`;
     * **Ожидание:** `mockBle->getTuningMessageSent() == 415.0f`.
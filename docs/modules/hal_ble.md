# **Детальное описание модуля: hal/ble**

**Назначение документа:** \> Этот документ описывает интерфейс (`IHalBle`) и реализацию (`HalBle`) для `BLE MIDI`.

## **1\. Назначение**

Модуль `hal_ble` предоставляет абстрактный интерфейс `IHalBle` для работы с Bluetooth Low Energy.

Его **единственная задача** — реализовать транспортный уровень для MIDI-сообщений. Он должен:

1. Инициализировать BLE-стек и рекламировать (advertise) устройство как **стандартное BLE-MIDI устройство**.  
2. Предоставлять простой API для `app/midi` (напр., `sendNoteOn`, `sendPitchBend`).  
3. Отправлять (publish) события `BLE_CONNECTED` и `BLE_DISCONNECTED` в `core/event_dispatcher`, чтобы `hal_led` мог реагировать на изменение статуса.

## **2\. Зависимости**

* **`ESP-IDF` / `Arduino`:** (Критическая) `BLEDevice.h` (или `NimBLEDevice.h`), `BLEServer.h`, `BLEUtils.h`.  
* **`Arduino-BLE-MIDI.h`:** (Критическая) Внешняя библиотека, которая реализует MIDI-сервис поверх `BLE`.  
* **`#include "core/event_dispatcher.h"`:** (Критическая) Используется для отправки событий `BLE_CONNECTED` / `BLE_DISCONNECTED`.  
* **`#include "diagnostics/logger.h"`:** Для логирования статуса (напр., "Client connected").

## **3\. Логика работы**

### **3.1. Интерфейс (`i_hal_ble.h`)**

Интерфейс должен быть высокоуровневым (MIDI-ориентированным), а не низкоуровневым (byte-oriented), чтобы `app/midi` не нужно было вручную собирать байты MIDI-сообщений.

```cpp
// (i_hal_ble.h)
#pragma once

#include "core/event_dispatcher.h"

// (Определения MIDI-каналов и velocity)
const int MIDI_CHANNEL = 1;
const int MIDI_VELOCITY = 127;

class IHalBle {
public:
    virtual ~IHalBle() {}

    /**
     * @brief Инициализирует BLE-стек, MIDI-сервис и начинает рекламу.
     * @param dispatcher Указатель на EventDispatcher для отправки событий.
     * @return true, если BLE-стек успешно запущен.
     */
    virtual bool init(EventDispatcher* dispatcher) = 0;

    // --- API для app/midi ---

    /**
     * @brief Отправляет MIDI-сообщение Note On.
     * @param pitch MIDI-нота (0-127).
     */
    virtual void sendNoteOn(int pitch) = 0;
    
    /**
     * @brief Отправляет MIDI-сообщение Note Off.
     * @param pitch MIDI-нота (0-127).
     */
    virtual void sendNoteOff(int pitch) = 0;

    /**
     * @brief Отправляет MIDI-сообщение Pitch Bend.
     * @param bend Значение (0.0 - 1.0, где 0.5 - центр).
     */
    virtual void sendPitchBend(float bend) = 0;

    /**
     * @brief Отправляет MIDI-сообщение "All Notes Off" (CC 123).
     * * Используется для Mute.
     */
    virtual void sendAllNotesOff() = 0;
};
```

### **3.2. Реализация (HalBle.cpp)**

1. **Класс `MyServerCallbacks`:**  
   * Внутри `HalBle.cpp` будет определен приватный класс, наследующий `BLEServerCallbacks`.  
   * `MyServerCallbacks(EventDispatcher* dispatcher)`: Сохраняет указатель на `dispatcher`.  
   * `onConnect(BLEServer* pServer)`:  
     * `LOG_INFO(TAG, "BLE Client Connected")`  
     * `m_dispatcher->postEvent(Event{EventType::BLE_CONNECTED});`  
   * `onDisconnect(BLEServer* pServer):`  
     * `LOG_INFO(TAG, "BLE Client Disconnected")`  
     * `m_dispatcher->postEvent(Event{EventType::BLE_DISCONNECTED});`  
     * (Здесь же можно перезапустить рекламу, если требуется).  
2. **Фаза `init(EventDispatcher* dispatcher)`:**  
   * Сохраняет `m_dispatcher = dispatcher`.  
   * `BLEDevice::init("Bagpipe Emulator");` (Устанавливает имя).  
   * `BLEServer* pServer = BLEDevice::createServer();`
   * `pServer->setCallbacks(new MyServerCallbacks(m_dispatcher));` (Регистрирует колбэки).  
   * `m_midiService = new BLEMIDI_NAMESPACE::BLE_MIDI(pServer);` (Создает сервис BLE-MIDI).  
   * `m_midiService->begin();`  
   * `BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();`  
   * `pAdvertising->addServiceUUID(m_midiService->getServiceUUID());`  
   * `pAdvertising->start();`  
   * `LOG_INFO(TAG, "BLE advertising started.")`  
   * Отправляет `BLE_DISCONNECTED` при старте, чтобы `LED` начал мигать.  
   * `m_dispatcher->postEvent(Event{EventType::BLE_DISCONNECTED});`  
3. **Реализация API (`sendNoteOn` и др.):**  
   * `void HalBle::sendNoteOn(int pitch)`:  
     * `m_midiService->noteOn(MIDI_CHANNEL, pitch, MIDI_VELOCITY);`  
   * `void HalBle::sendNoteOff(int pitch)`:  
     * `m_midiService->noteOff(MIDI_CHANNEL, pitch, 0);`  
   * `void HalBle::sendPitchBend(float bend)`:  
     * `int bendValue = (bend - 0.5f) * 16383;` // Преобразование (0.0-1.0) в (-8191...8192)  
     * `m_midiService->pitchBend(MIDI_CHANNEL, bendValue);`  
   * `void HalBle::sendAllNotesOff()`:  
     * `m_midiService->controlChange(MIDI_CHANNEL, 123, 0);`

## **4\. Тестирование (Host-First)**

* `HalBle` — это "железный" модуль. Он **не будет** компилироваться в `[env:native]`.  
* Вместо него `[env:native]` будет использовать **`MockHalBle`** (Спринт 1.6).  
* `MockHalBle` будет реализовывать `IHalBle`.

```cpp
// (Пример в test/mocks/MockHalBle.h)
#include "hal_interfaces/i_hal_ble.h"
#include "core/event_dispatcher.h"

class MockHalBle : public IHalBle {
public:
    virtual bool init(EventDispatcher* dispatcher) override { 
        m_dispatcher = dispatcher;
        m_dispatcher->postEvent(Event{EventType::BLE_DISCONNECTED}); // Эмуляция старта
        return true; 
    }

    virtual void sendNoteOn(int pitch) override { m_lastNoteOn = pitch; }
    virtual void sendNoteOff(int pitch) override { m_lastNoteOff = pitch; }
    virtual void sendPitchBend(float bend) override { m_lastPitchBend = bend; }
    virtual void sendAllNotesOff() override { m_allNotesOffCount++; }

    // --- Методы для тестов ---
    void simulateConnect() {
        m_dispatcher->postEvent(Event{EventType::BLE_CONNECTED});
    }
    void simulateDisconnect() {
        m_dispatcher->postEvent(Event{EventType::BLE_DISCONNECTED});
    }

    int getLastNoteOn() const { return m_lastNoteOn; }
    // ... (и другие геттеры)

private:
    EventDispatcher* m_dispatcher;
    int m_lastNoteOn = -1;
    int m_lastNoteOff = -1;
    float m_lastPitchBend = 0.5f;
    int m_allNotesOffCount = 0;
};

```
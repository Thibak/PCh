# **Детальное описание модуля: hal/ble**

**Назначение документа:** \> Этот документ описывает интерфейс (IHalBle) и реализацию (HalBle) для BLE MIDI.

## **1\. Назначение**

Модуль hal\_ble предоставляет абстрактный интерфейс IHalBle для работы с Bluetooth Low Energy.

Его **единственная задача** — реализовать транспортный уровень для MIDI-сообщений. Он должен:

1. Инициализировать BLE-стек и рекламировать (advertise) устройство как **стандартное BLE-MIDI устройство**.  
2. Предоставлять простой API для app/midi (напр., sendNoteOn, sendPitchBend).  
3. Отправлять (publish) события BLE\_CONNECTED и BLE\_DISCONNECTED в core/event\_dispatcher, чтобы hal\_led мог реагировать на изменение статуса.

## **2\. Зависимости**

* **ESP-IDF / Arduino:** (Критическая) BLEDevice.h (или NimBLEDevice.h), BLEServer.h, BLEUtils.h.  
* **Arduino-BLE-MIDI.h:** (Критическая) Внешняя библиотека, которая реализует MIDI-сервис поверх BLE.  
* **\#include "core/event\_dispatcher.h":** (Критическая) Используется для отправки событий BLE\_CONNECTED / BLE\_DISCONNECTED.  
* **\#include "diagnostics/logger.h":** Для логирования статуса (напр., "Client connected").

## **3\. Логика работы**

### **3.1. Интерфейс (i\_hal\_ble.h)**

Интерфейс должен быть высокоуровневым (MIDI-ориентированным), а не низкоуровневым (byte-oriented), чтобы app/midi не нужно было вручную собирать байты MIDI-сообщений.

// (i\_hal\_ble.h)  
\#pragma once

\#include "core/event\_dispatcher.h"

// (Определения MIDI-каналов и velocity)  
const int MIDI\_CHANNEL \= 1;  
const int MIDI\_VELOCITY \= 127;

class IHalBle {  
public:  
    virtual \~IHalBle() {}

    /\*\*  
     \* @brief Инициализирует BLE-стек, MIDI-сервис и начинает рекламу.  
     \* @param dispatcher Указатель на EventDispatcher для отправки событий.  
     \* @return true, если BLE-стек успешно запущен.  
     \*/  
    virtual bool init(EventDispatcher\* dispatcher) \= 0;

    // \--- API для app/midi \---

    /\*\*  
     \* @brief Отправляет MIDI-сообщение Note On.  
     \* @param pitch MIDI-нота (0-127).  
     \*/  
    virtual void sendNoteOn(int pitch) \= 0;  
      
    /\*\*  
     \* @brief Отправляет MIDI-сообщение Note Off.  
     \* @param pitch MIDI-нота (0-127).  
     \*/  
    virtual void sendNoteOff(int pitch) \= 0;

    /\*\*  
     \* @brief Отправляет MIDI-сообщение Pitch Bend.  
     \* @param bend Значение (0.0 \- 1.0, где 0.5 \- центр).  
     \*/  
    virtual void sendPitchBend(float bend) \= 0;

    /\*\*  
     \* @brief Отправляет MIDI-сообщение "All Notes Off" (CC 123).  
     \* \* Используется для Mute.  
     \*/  
    virtual void sendAllNotesOff() \= 0;  
};

### **3.2. Реализация (HalBle.cpp)**

1. **Класс MyServerCallbacks:**  
   * Внутри HalBle.cpp будет определен приватный класс, наследующий BLEServerCallbacks.  
   * MyServerCallbacks(EventDispatcher\* dispatcher): Сохраняет указатель на dispatcher.  
   * onConnect(BLEServer\* pServer):  
     * LOG\_INFO(TAG, "BLE Client Connected")  
     * m\_dispatcher-\>postEvent(Event{EventType::BLE\_CONNECTED});  
   * onDisconnect(BLEServer\* pServer):  
     * LOG\_INFO(TAG, "BLE Client Disconnected")  
     * m\_dispatcher-\>postEvent(Event{EventType::BLE\_DISCONNECTED});  
     * (Здесь же можно перезапустить рекламу, если требуется).  
2. **Фаза init(EventDispatcher\* dispatcher):**  
   * Сохраняет m\_dispatcher \= dispatcher.  
   * BLEDevice::init("Bagpipe Emulator"); (Устанавливает имя).  
   * BLEServer\* pServer \= BLEDevice::createServer();  
   * pServer-\>setCallbacks(new MyServerCallbacks(m\_dispatcher)); (Регистрирует колбэки).  
   * m\_midiService \= new BLEMIDI\_NAMESPACE::BLE\_MIDI(pServer); (Создает сервис BLE-MIDI).  
   * m\_midiService-\>begin();  
   * BLEAdvertising\* pAdvertising \= BLEDevice::getAdvertising();  
   * pAdvertising-\>addServiceUUID(m\_midiService-\>getServiceUUID());  
   * pAdvertising-\>start();  
   * LOG\_INFO(TAG, "BLE advertising started.")  
   * Отправляет BLE\_DISCONNECTED при старте, чтобы LED начал мигать.  
   * m\_dispatcher-\>postEvent(Event{EventType::BLE\_DISCONNECTED});  
3. **Реализация API (sendNoteOn и др.):**  
   * void HalBle::sendNoteOn(int pitch):  
     * m\_midiService-\>noteOn(MIDI\_CHANNEL, pitch, MIDI\_VELOCITY);  
   * void HalBle::sendNoteOff(int pitch):  
     * m\_midiService-\>noteOff(MIDI\_CHANNEL, pitch, 0);  
   * void HalBle::sendPitchBend(float bend):  
     * int bendValue \= (bend \- 0.5f) \* 16383; // Преобразование (0.0-1.0) в (-8191...8192)  
     * m\_midiService-\>pitchBend(MIDI\_CHANNEL, bendValue);  
   * void HalBle::sendAllNotesOff():  
     * m\_midiService-\>controlChange(MIDI\_CHANNEL, 123, 0);

## **4\. Тестирование (Host-First)**

* HalBle — это "железный" модуль. Он **не будет** компилироваться в \[env:native\].  
* Вместо него \[env:native\] будет использовать **MockHalBle** (Спринт 1.6).  
* MockHalBle будет реализовывать IHalBle.

// (Пример в test/mocks/MockHalBle.h)  
\#include "hal\_interfaces/i\_hal\_ble.h"  
\#include "core/event\_dispatcher.h"

class MockHalBle : public IHalBle {  
public:  
    virtual bool init(EventDispatcher\* dispatcher) override {   
        m\_dispatcher \= dispatcher;  
        m\_dispatcher-\>postEvent(Event{EventType::BLE\_DISCONNECTED}); // Эмуляция старта  
        return true;   
    }

    virtual void sendNoteOn(int pitch) override { m\_lastNoteOn \= pitch; }  
    virtual void sendNoteOff(int pitch) override { m\_lastNoteOff \= pitch; }  
    virtual void sendPitchBend(float bend) override { m\_lastPitchBend \= bend; }  
    virtual void sendAllNotesOff() override { m\_allNotesOffCount++; }

    // \--- Методы для тестов \---  
    void simulateConnect() {  
        m\_dispatcher-\>postEvent(Event{EventType::BLE\_CONNECTED});  
    }  
    void simulateDisconnect() {  
        m\_dispatcher-\>postEvent(Event{EventType::BLE\_DISCONNECTED});  
    }

    int getLastNoteOn() const { return m\_lastNoteOn; }  
    // ... (и другие геттеры)

private:  
    EventDispatcher\* m\_dispatcher;  
    int m\_lastNoteOn \= \-1;  
    int m\_lastNoteOff \= \-1;  
    float m\_lastPitchBend \= 0.5f;  
    int m\_allNotesOffCount \= 0;  
};  

/*
 * IHalBle.h
 *
 * Абстрактный интерфейс (контракт) для BLE-MIDI.
 *
 * Соответствует: docs/modules/hal_ble.md
 */
#pragma once

// (Forward-declare EventDispatcher, чтобы избежать циклической зависимости)
class EventDispatcher;

// Определения MIDI-каналов и velocity
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

    /**
     * @brief Запускает задачу(и) FreeRTOS, если они необходимы для BLE.
     * (TBD в Спринте 0.6 / 3.6)
     */
    virtual void startTask() = 0;

    /**
     * @brief Подписывает этот модуль на события (если необходимо).
     */
    virtual void subscribe(EventDispatcher* dispatcher) = 0;

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

    /**
     * @brief (TBD Спринт 2.17) Отправляет сообщение о смене строя (RPN/MTS).
     */
    virtual void sendTuningMessage(float basePitchHz) = 0;
};
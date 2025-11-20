/*
 * events.h
 *
 * Определяет все типы событий и структуры данных (payloads)
 * для core/event_dispatcher.
 *
 * Соответствует: docs/modules/core_event_dispatcher.md
 */
#pragma once
#include <cstdint>

// 1. Типы событий
enum class EventType {
    // HAL -> APP
    SENSOR_VALUE_CHANGED, // (payload: sensorValue)
    BLE_CONNECTED,        // (no payload)
    BLE_DISCONNECTED,     // (no payload)
    
    // APP -> APP
    SENSOR_MASK_CHANGED,  // (payload: sensorMask)
    HALF_HOLE_DETECTED,   // (payload: halfHole)
    VIBRATO_DETECTED,     // (payload: vibrato)
    MUTE_ENABLED,         // (no payload)
    MUTE_DISABLED,        // (no payload)
    NOTE_PITCH_SELECTED,  // (payload: notePitch)
    
    // CORE -> APP
    SYSTEM_IDLE_TIMEOUT   // (no payload)
};

// 2. Структуры данных (Payloads)
struct SensorValuePayload { int id; int value; };
struct SensorMaskPayload { uint8_t mask; };
struct HalfHolePayload { int id; };
struct VibratoPayload { int id; float depth; };
struct NotePitchPayload { int pitch; }; // 0 = Note Off

// 3. Единая структура события
struct Event {
    EventType type;
    
    // Объединение (union) для всех возможных данных
    union {
        SensorValuePayload sensorValue;
        SensorMaskPayload sensorMask;
        HalfHolePayload halfHole;
        VibratoPayload vibrato;
        NotePitchPayload notePitch;
    } payload;

    // Конструкторы
    
    // 1. Для событий без данных
    Event(EventType t) : type(t) {}

    // 2. Для SENSOR_VALUE_CHANGED
    Event(EventType t, SensorValuePayload p) : type(t), payload{.sensorValue = p} {}

    // 3. Для SENSOR_MASK_CHANGED
    Event(EventType t, SensorMaskPayload p) : type(t), payload{.sensorMask = p} {}

    // 4. Для HALF_HOLE_DETECTED
    Event(EventType t, HalfHolePayload p) : type(t), payload{.halfHole = p} {}

    // 5. Для VIBRATO_DETECTED
    Event(EventType t, VibratoPayload p) : type(t), payload{.vibrato = p} {}

    // 6. Для NOTE_PITCH_SELECTED (Именно его не хватало)
    Event(EventType t, NotePitchPayload p) : type(t), payload{.notePitch = p} {}
};
/*
 * AppMidi.cpp
 *
 * Реализация формирователя MIDI.
 * Преобразует события (ноты, жесты) в команды HAL BLE и управляет индикацией.
 *
 * Соответствует: docs/modules/app_midi.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.8
 */

#include "app/AppMidi.h"
#include "core/Logger.h"
#include <iostream> // Для отладки в Native

#define TAG "AppMidi"

AppMidi::AppMidi()
    : m_halBle(nullptr),
      m_halLed(nullptr),
      m_currentNote(0),
      m_isMuted(false),
      m_basePitchHz(440.0f) {
}

bool AppMidi::init(IHalBle* halBle, IHalLed* halLed, float basePitchHz) {
    m_halBle = halBle;
    m_halLed = halLed;
    m_basePitchHz = basePitchHz;
    m_currentNote = 0;
    m_isMuted = false;

    // (TBD в Спринте 2.11: Отправка Tuning Message при старте/подключении)
    
    return true;
}

void AppMidi::subscribe(EventDispatcher* dispatcher) {
    if (dispatcher) {
        dispatcher->subscribe(EventType::NOTE_PITCH_SELECTED, this);
        dispatcher->subscribe(EventType::VIBRATO_DETECTED, this);
        dispatcher->subscribe(EventType::MUTE_ENABLED, this);
        dispatcher->subscribe(EventType::MUTE_DISABLED, this);
        dispatcher->subscribe(EventType::BLE_CONNECTED, this);
    }
}

void AppMidi::handleEvent(const Event& event) {
    switch (event.type) {
        case EventType::BLE_CONNECTED: {
             // Спринт 2.11: Отправка настройки строя при подключении
             // Отправляем только если частота отличается от стандарта 440Hz (с небольшим допуском ??? )
             if (m_halBle && std::abs(m_basePitchHz - 440.0f) > 0.01f) {
                 m_halBle->sendTuningMessage(m_basePitchHz);
                 LOG_INFO(TAG, "Sent tuning message: %.2f Hz", m_basePitchHz);
                 
                 #if defined(NATIVE_TEST)
                 std::cout << "[AppMidi] BLE Connected -> Sending Tuning: " << m_basePitchHz << std::endl;
                 #endif
             }
             break;
        }

        case EventType::NOTE_PITCH_SELECTED: {
            int newNote = event.payload.notePitch.pitch;
            handleNoteChange(newNote);
            break;
        }

        case EventType::VIBRATO_DETECTED: {
            if (m_isMuted) return;
            // Пропускаем вибрато, если HAL не инициализирован
            if (m_halBle) {
                float depth = event.payload.vibrato.depth;
                m_halBle->sendPitchBend(depth);
                
                #if defined(NATIVE_TEST)
                std::cout << "[AppMidi] PitchBend: " << depth << std::endl;
                #endif
            }
            break;
        }

        case EventType::MUTE_ENABLED: {
            m_isMuted = true;
            // Срочно выключаем текущую ноту
            handleNoteChange(0); 
            // И посылаем "Panic" (All Notes Off) для надежности
            if (m_halBle) {
                m_halBle->sendAllNotesOff();
            }
            LOG_INFO(TAG, "Mute Enabled");
            break;
        }

        case EventType::MUTE_DISABLED: {
            m_isMuted = false;
            LOG_INFO(TAG, "Mute Disabled");
            break;
        }

        default:
            break;
    }
}

void AppMidi::handleNoteChange(int newNote) {
    // Если включен Mute, мы можем только ВЫКЛЮЧАТЬ ноты (newNote=0),
    // но не включать новые.
    if (m_isMuted && newNote > 0) {
        return;
    }

    // Если нота та же самая, ничего не делаем (дедупликация)
    if (newNote == m_currentNote) {
        return;
    }

    if (!m_halBle) return;

    // 1. Выключаем старую ноту (если она была)
    if (m_currentNote > 0) {
        m_halBle->sendNoteOff(m_currentNote);
        
        #if defined(NATIVE_TEST)
        std::cout << "[AppMidi] Note OFF: " << m_currentNote << std::endl;
        #endif
    }

    // 2. Включаем новую ноту (если это не пауза 0)
    if (newNote > 0) {
        m_halBle->sendNoteOn(newNote);
        
        // Моргаем светодиодом
        if (m_halLed) {
            m_halLed->setMode(LedMode::BLINK_ONCE);
        }

        #if defined(NATIVE_TEST)
        std::cout << "[AppMidi] Note ON: " << newNote << std::endl;
        #endif
    }

    // 3. Запоминаем состояние
    m_currentNote = newNote;
}
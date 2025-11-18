/*
 * AppMidi.cpp
 *
 * Реализация (Заглушка) для AppMidi
 *
 * Соответствует: docs/modules/app_midi.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.2
 */
#include "app/AppMidi.h"
#include "core/Logger.h"

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
    // (TBD в Спринте 2.14 и 2.18)
    return true;
}

void AppMidi::subscribe(EventDispatcher* dispatcher) {
    // (TBD в Спринте 2.14: Подписаться на NOTE_PITCH_SELECTED, MUTE_*, VIBRATO_*)
}

void AppMidi::handleEvent(const Event& event) {
    // (TBD в Спринте 2.14 и 2.18)
}

// --- Приватные методы ---

void AppMidi::handleNoteChange(int newNote) {
    // (TBD в Спринте 2.14)
}
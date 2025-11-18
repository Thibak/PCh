/*
 * AppMidi.h
 * 
 * Соответствует: docs/modules/app_midi.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.1
 */
#pragma once

#include "interfaces/IHalBle.h"
#include "interfaces/IHalLed.h"
#include "core/EventDispatcher.h"
#include "interfaces/IEventHandler.h"

class AppMidi : public IEventHandler {
public:
    AppMidi();
    
    /**
     * @brief Сохраняет указатели на HAL модули и базовую частоту.
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
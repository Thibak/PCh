/*
 * MockHalBle.h
 *
 * Mock-реализация (для Host-тестирования) интерфейса IHalBle.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 1.9
 * Требование: Должен записывать вызовы API в переменные для проверки тестами.
 */
#pragma once
#include "interfaces/IHalBle.h"
#include "core/EventDispatcher.h" // Нужен для эмуляции connect/disconnect

class MockHalBle : public IHalBle {
public:
    MockHalBle();
    virtual ~MockHalBle();

    virtual bool init(EventDispatcher* dispatcher) override;
    virtual void startTask() override;
    virtual void subscribe(EventDispatcher* dispatcher) override;

    // --- API для app/midi ---
    virtual void sendNoteOn(int pitch) override;
    virtual void sendNoteOff(int pitch) override;
    virtual void sendPitchBend(float bend) override;
    virtual void sendAllNotesOff() override;
    virtual void sendTuningMessage(float basePitchHz) override;

    // --- Методы для тестов ---
    
    /**
     * @brief Эмулирует подключение клиента (телефона).
     */
    void simulateConnect();

    /**
     * @brief Эмулирует отключение клиента (телефона).
     */
    void simulateDisconnect();

    // (Новое) Сброс состояния
    void reset();

    // Геттеры для проверки тестами (Спринт 2.14)
    int getLastNoteOn() const;
    int getLastNoteOff() const;
    float getLastPitchBend() const;
    int getAllNotesOffCount() const;
    float getTuningMessageSent() const;

private:
    EventDispatcher* m_dispatcher; // Указатель на диспетчер для отправки событий
    
    // Переменные состояния для тестов
    int m_lastNoteOn;
    int m_lastNoteOff;
    float m_lastPitchBend;
    int m_allNotesOffCount;
    float m_tuningMessagePitch;
};

/*
 * MockHalBle.cpp
 *
 * Реализация Mock-класса для IHalBle.
 */
#include "MockHalBle.h"
#include <iostream> // Для std::cout

MockHalBle::MockHalBle() 
    : m_dispatcher(nullptr),
      m_lastNoteOn(-1),
      m_lastNoteOff(-1),
      m_lastPitchBend(0.5f),
      m_allNotesOffCount(0),
      m_tuningMessagePitch(0.0f) {
}

MockHalBle::~MockHalBle() {
}

bool MockHalBle::init(EventDispatcher* dispatcher) {
    m_dispatcher = dispatcher;
    std::cout << "[MockHalBle] Init()." << std::endl;
    // Эмулируем старт в отключенном состоянии
    m_dispatcher->postEvent(Event(EventType::BLE_DISCONNECTED));
    return true; 
}

void MockHalBle::startTask() {
    // Ничего не делаем, у мока нет своей задачи
    std::cout << "[MockHalBle] startTask() called (no-op)." << std::endl;
}

void MockHalBle::subscribe(EventDispatcher* dispatcher) {
    // HalBle (реальный) не является подписчиком, но мок может
    // (оставляем пустым, т.к. мы это исправили в Спринте 0.17)
}

// --- API для app/midi ---

void MockHalBle::sendNoteOn(int pitch) {
    std::cout << "[MockHalBle] sendNoteOn: " << pitch << std::endl;
    m_lastNoteOn = pitch;
}

void MockHalBle::sendNoteOff(int pitch) {
    std::cout << "[MockHalBle] sendNoteOff: " << pitch << std::endl;
    m_lastNoteOff = pitch;
}

void MockHalBle::sendPitchBend(float bend) {
    std::cout << "[MockHalBle] sendPitchBend: " << bend << std::endl;
    m_lastPitchBend = bend;
}

void MockHalBle::sendAllNotesOff() {
    std::cout << "[MockHalBle] sendAllNotesOff (CC 123)" << std::endl;
    m_allNotesOffCount++;
}

void MockHalBle::sendTuningMessage(float basePitchHz) {
    std::cout << "[MockHalBle] sendTuningMessage: " << basePitchHz << " Hz" << std::endl;
    m_tuningMessagePitch = basePitchHz;
}

// --- Методы для тестов ---

void MockHalBle::simulateConnect() {
    if (m_dispatcher) {
        std::cout << "[MockHalBle] Simulating BLE Connect..." << std::endl;
        m_dispatcher->postEvent(Event(EventType::BLE_CONNECTED));
    }
}

void MockHalBle::simulateDisconnect() {
    if (m_dispatcher) {
        std::cout << "[MockHalBle] Simulating BLE Disconnect..." << std::endl;
        m_dispatcher->postEvent(Event(EventType::BLE_DISCONNECTED));
    }
}

void MockHalBle::reset() {
    m_lastNoteOn = -1;
    m_lastNoteOff = -1;
    m_lastPitchBend = 0.5f;
    m_allNotesOffCount = 0;
    m_tuningMessagePitch = 0.0f;
}

// Геттеры
int MockHalBle::getLastNoteOn() const { return m_lastNoteOn; }
int MockHalBle::getLastNoteOff() const { return m_lastNoteOff; }
float MockHalBle::getLastPitchBend() const { return m_lastPitchBend; }
int MockHalBle::getAllNotesOffCount() const { return m_allNotesOffCount; }
float MockHalBle::getTuningMessageSent() const { return m_tuningMessagePitch; }
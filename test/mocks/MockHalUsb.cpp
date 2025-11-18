/*
 * MockHalUsb.cpp
 *
 * Реализация Mock-класса для IHalUsb.
 */
#include "MockHalUsb.h"
#include <iostream> // Для std::cout и std::endl

MockHalUsb::MockHalUsb() 
    : m_serialPrintCount(0), m_storagePassed(false) {
    // Конструктор
}

MockHalUsb::~MockHalUsb() {
    // Деструктор
}

bool MockHalUsb::init(IHalStorage* storage) {
    m_storagePassed = (storage != nullptr);
    std::cout << "[MockHalUsb] Init() called. Storage valid: " 
              << (m_storagePassed ? "true" : "false") << std::endl;
    return true; 
}

/**
 * @brief Эмулирует serialPrint, выводя лог в консоль (stdout).
 * (Реализация требования Спринта 1.7)
 */
bool MockHalUsb::serialPrint(const std::string& line) {
    // Выводим в консоль, чтобы разработчик (мы) мог видеть логи
    std::cout << "[SERIAL] " << line << std::endl;
    
    // Сохраняем для будущих тестов
    m_lastSerialLine = line;
    m_serialPrintCount++;
    return true;
}

// --- Методы для тестов ---

std::string MockHalUsb::getLastSerialLine() const {
    return m_lastSerialLine;
}

int MockHalUsb::getSerialPrintCount() const {
    return m_serialPrintCount;
}

bool MockHalUsb::wasInitCalledWithStorage() const {
    return m_storagePassed;
}
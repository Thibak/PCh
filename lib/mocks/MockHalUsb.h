/*
 * MockHalUsb.h
 *
 * Mock-реализация (для Host-тестирования) интерфейса IHalUsb.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 1.7
 * Требование: Должен выводить serialPrint() в stdout.
 */
#pragma once
#include "interfaces/IHalUsb.h"
#include <string>
#include <vector>

class MockHalUsb : public IHalUsb {
public:
    MockHalUsb();
    virtual ~MockHalUsb();

    virtual bool init(IHalStorage* storage) override;
    virtual bool serialPrint(const std::string& line) override;

    // --- Методы для тестов ---

    /**
     * @brief Получает последнюю строку, которая была "напечатана" в stdout.
     */
    std::string getLastSerialLine() const;
    
    /**
     * @brief Получает общее количество строк, "напечатанных" в stdout.
     */
    int getSerialPrintCount() const;
    
    /**
     * @brief Проверяет, был ли `init()` вызван с валидным указателем.
     */
    bool wasInitCalledWithStorage() const;

private:
    std::string m_lastSerialLine;
    int m_serialPrintCount;
    bool m_storagePassed;
};
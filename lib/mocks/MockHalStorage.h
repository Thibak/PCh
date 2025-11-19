/*
 * MockHalStorage.h
 *
 * Mock-реализация (для Host-тестирования) интерфейса IHalStorage.
 *
 * Эта реализация *читает/пишет* в реальную файловую систему ПК,
 * используя папку data/ в корне проекта.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 1.6
 */
#pragma once
#include "interfaces/IHalStorage.h"
#include <string>

class MockHalStorage : public IHalStorage {
public:
    MockHalStorage(); // Добавлен конструктор

    virtual bool init() override;
    virtual bool readFile(const std::string& path, std::string& content) override;
    virtual bool writeFile(const std::string& path, const std::string& content) override;
    virtual bool fileExists(const std::string& path) override;

    // --- API для тестов ---
    
    /**
     * @brief Если true, readFile всегда будет возвращать false (файл не найден),
     * не обращаясь к реальной файловой системе.
     */
    void setSimulateReadError(bool simulate);

private:
    /**
     * @brief Преобразует "/settings.cfg" в "data/settings.cfg"
     */
    std::string getHostPath(const std::string& path);
    bool m_simulateReadError; // Флаг симуляции
};
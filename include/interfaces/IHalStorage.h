/*
 * IHalStorage.h
 *
 * Абстрактный интерфейс (контракт) для Файловой Системы.
 *
 * Соответствует: docs/modules/hal_storage.md
 */
#pragma once
#include <string>

class IHalStorage {
public:
    virtual ~IHalStorage() {}

    /**
     * @brief Монтирует файловую систему. Вызывается один раз при старте.
     * @return true, если ФС успешно смонтирована.
     */
    virtual bool init() = 0;

    /**
     * @brief Читает файл из ФС и возвращает его содержимое.
     * @param path Полный путь к файлу (напр. "/settings.cfg").
     * @param content [out] Строка, в которую будет записано содержимое файла.
     * @return true, если файл успешно прочитан.
     */
    virtual bool readFile(const std::string& path, std::string& content) = 0;

    /**
     * @brief Записывает (или перезаписывает) файл в ФС.
     * @param path Полный путь к файлу (напр. "/settings.cfg").
     * @param content Строка с данными для записи.
     * @return true, если файл успешно записан.
     */
    virtual bool writeFile(const std::string& path, const std::string& content) = 0;

    /**
     * @brief Проверяет, существует ли файл.
     * @param path Полный путь к файлу.
     * @return true, если файл существует.
     */
    virtual bool fileExists(const std::string& path) = 0;
};
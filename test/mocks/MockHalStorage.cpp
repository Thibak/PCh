/*
 * MockHalStorage.cpp
 *
 * Реализация Mock-класса для IHalStorage.
 */
#include "MockHalStorage.h"
#include <fstream>   // Для std::ifstream, std::ofstream
#include <sstream>   // Для std::stringstream
#include <iostream>  // Для std::cout (логирование)

bool MockHalStorage::init() {
    // В native-окружении монтировать нечего,
    // просто сообщаем, что мы "готовы".
    std::cout << "[MockHalStorage] Init(). Ready to read from 'data/'." << std::endl;
    return true;
}

/**
 * @brief Преобразует "/settings.cfg" (путь SPIFFS) 
 * в "data/settings.cfg" (относительный путь на хосте).
 */
std::string MockHalStorage::getHostPath(const std::string& path) {
    // `path` приходит как "/settings.cfg"
    // Мы ожидаем, что папка `data/` находится в
    // корневой директории, откуда запускаются тесты.
    if (path.rfind('/', 0) == 0) {
        return "data" + path;
    }
    return "data/" + path;
}

/**
 * @brief Читает реальный файл с диска ПК (из папки data/).
 */
bool MockHalStorage::readFile(const std::string& path, std::string& content) {
    std::string hostPath = getHostPath(path);
    std::ifstream file(hostPath);
    if (!file.is_open()) {
        std::cerr << "[MockHalStorage] FAILED to read file: " << hostPath << std::endl;
        return false; // Файл не найден
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();
    std::cout << "[MockHalStorage] Read file: " << hostPath << std::endl;
    return true;
}

/**
 * @brief Пишет в реальный файл на диске ПК (в папку data/).
 * (Это полезно для тестов, которые хотят создать "виртуальный" конфиг)
 */
bool MockHalStorage::writeFile(const std::string& path, const std::string& content) {
    std::string hostPath = getHostPath(path);
    std::ofstream file(hostPath);
    if (!file.is_open()) {
        std::cerr << "[MockHalStorage] FAILED to write file: " << hostPath << std::endl;
        return false;
    }
    file << content;
    file.close();
    std::cout << "[MockHalStorage] Wrote file: " << hostPath << std::endl;
    return true;
}

bool MockHalStorage::fileExists(const std::string& path) {
    std::string hostPath = getHostPath(path);
    std::ifstream file(hostPath);
    return file.good();
}
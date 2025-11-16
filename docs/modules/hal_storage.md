# **Детальное описание модуля: `hal/storage`**

**Назначение документа:** \> Этот документ описывает интерфейс (`IHalStorage`) и реализацию (`HalStorage`) для уровня абстракции файловой системы.

## **1\. Назначение**

Модуль `hal_storage` предоставляет абстрактный интерфейс `IHalStorage` для доступа к файловой системе (ФС) на чипе.

Его **единственная задача** — скрыть от остальной системы (особенно от `core/config_manager` и `app/fingering`) *конкретную* реализацию ФС (будь то `SPIFFS`, `LittleFS` или другая).

* `core/config_manager` просто вызывает `storage->readFile("/settings.cfg")`, не зная, как именно этот файл читается.

## **2\. Зависимости**

* **ESP-IDF / Arduino:** Внутренняя реализация (`HalStorage.cpp`) будет напрямую использовать C-функции `esp_spiffs.h` (для IDF) или C++ класс `SPIFFS` (для Arduino) для монтирования, чтения и записи.

## **3\. Логика работы**

### **3.1. Интерфейс (`i_hal_storage.h`)**

Мы определяем чистый "контракт" (абстрактный класс), который будет использоваться во всей системе.

```cpp
// (i_hal_storage.h)
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
```

### **3.2. Реализация (HalStorage.cpp)**

Конкретная реализация будет "оберткой" над функциями `SPIFFS`.

```cpp
// (Пример в HalStorage.cpp)
#include "HalStorage.h"
#include "SPIFFS.h" // (Если используется Arduino)
#include "diagnostics/logger.h" // (Для логирования ошибок)

#define TAG "hal/storage"

bool HalStorage::init() {
    if (!SPIFFS.begin(true /* formatOnFail */)) {
        LOG_ERROR(TAG, "SPIFFS Mount Failed!");
        return false;
    }
    LOG_INFO(TAG, "SPIFFS Mounted.");
    return true;
}

bool HalStorage::readFile(const std::string& path, std::string& content) {
    File file = SPIFFS.open(path.c_str(), "r");
    if (!file) {
        LOG_WARN(TAG, "Failed to open file for reading: %s", path.c_str());
        return false;
    }
    
    content = file.readString();
    file.close();
    return true;
}

// ... (и так далее для writeFile, fileExists)
```

## **4\. Тестирование (Host-First)**

* `HalStorage` — это "железный" модуль. Он **не будет** компилироваться в `[env:native]`.  
* Вместо него `[env:native]` будет использовать **`MockHalStorage`** (Спринт 1.3).  
* `MockHalStorage` будет реализовывать тот же интерфейс `IHalStorage`, но хранить файлы в `std::map<string, string>` (в RAM), эмулируя файловую систему.

```cpp
// (Пример в test/mocks/MockHalStorage.h)
#include "hal_interfaces/i_hal_storage.h"
#include <map>

class MockHalStorage : public IHalStorage {
public:
    virtual bool init() override { return true; } // Ничего не делаем

    virtual bool readFile(const std::string& path, std::string& content) override {
        if (m_mockFs.count(path)) {
            content = m_mockFs[path];
            return true;
        }
        return false; // Файл не найден
    }
    
    virtual bool writeFile(const std::string& path, const std::string& content) override {
        m_mockFs[path] = content;
        return true;
    }
    
    // ...
private:
    std::map<std::string, std::string> m_mockFs;
};
```

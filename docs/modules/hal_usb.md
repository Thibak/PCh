# **Детальное описание модуля: `hal/usb`**

**Назначение документа:** \> Этот документ описывает интерфейс (`IHalUsb`) и реализацию (`HalUsb`) для композитного USB-устройства.

## **1\. Назначение**

Модуль `hal_usb` предоставляет абстрактный интерфейс `IHalUsb` и реализует сложную логику, необходимую для работы USB OTG.

Согласно `ARCH_CONTRACT.MD`, его **основная задача** — при подключении к ПК представить себя как **композитное (составное) USB-устройство**, которое одновременно предоставляет:

1. **Mass Storage Class (MSC):** "Флешка" для прямого доступа к `hal_storage` (для редактирования `.cfg` файлов).  
2. **Communications Device Class (CDC):** "COM-порт" (Serial) для вывода логов из `diagnostics_logging`.

## **2\. Зависимости**

* **ESP-IDF / TinyUSB:** (Критическая) Реализация (`HalUsb.cpp`) будет напрямую использовать API TinyUSB (стандарт для ESP32-S3) для настройки дескрипторов композитного устройства и обработки колбэков.  
* **`#include "hal_interfaces/i_hal_storage.h"`:** (Критическая) Модуль `hal_usb` *не* работает с `SPIFFS` напрямую. Он получает указатель на `IHalStorage` во время `init()` и использует его для обслуживания запросов чтения/записи от ОС (через MSC).  
* **`#include "diagnostics/logger.h"`:** Для логирования собственных ошибок (напр., "TinyUSB stack failed to start").

## **3\. Логика работы**

### **3.1. Интерфейс (`i_hal_usb.h`)**

Интерфейс `IHalUsb` очень "тонкий". Большинство модулей (кроме `Logger`) вообще не знают о его существовании.

```cpp
// (i_hal_usb.h)
#pragma once

#include <string>
#include "i_hal_storage.h" // Для init()

class IHalUsb {
public:
    virtual ~IHalUsb() {}

    /**
     * @brief Инициализирует стек TinyUSB как композитное устройство.
     * @param storage Указатель на уже инициализированный hal_storage.
     * @return true, если USB-стек успешно запущен.
     */
    virtual bool init(IHalStorage* storage) = 0;

    /**
     * @brief Отправляет строку в буфер USB Serial (CDC).
     * * Этот метод должен быть потокобезопасным (использовать внутренний
     * мьютекс), но быстрым (не блокирующим надолго).
     * * @param line Строка для отправки (уже отформатированная логгером).
     * @return true, если данные успешно помещены в буфер отправки.
     */
    virtual bool serialPrint(const std::string& line) = 0;
};
```

### **3.2. Реализация (`HalUsb.cpp`)**

Вся сложность скрыта здесь.

1. **Фаза `init(IHalStorage* storage)`:**  
   * Сохраняет указатель `m_storage = storage`.  
   * Инициализирует стек `TinyUSB` (`tusb_init()`).  
   * Настраивает **дескрипторы** композитного устройства (сообщает ОС, что у него есть 1x MSC и 1x CDC).  
   * Регистрирует *колбэки (callbacks)* для MSC.  
   * Запускает задачу `FreeRTOS` `tud_task()` (главная задача `TinyUSB`), если это не делается автоматически.  
2. **Колбэки Mass Storage (MSC):**  
   * **`tud_msc_read_cb(lun, lba, offset, buffer, bufsize)`:**  
     * Этот колбэк вызывается ОС, когда она хочет *прочитать* данные с "флешки".  
     * `HalUsb` должен транслировать lba (Logical Block Address) в вызов `m_storage->read(...)` (это нетривиальная задача, требующая эмуляции FAT16/FAT32 или использования `esp_vfs_fat_spiffs_format_rw_api`).  
   * **`tud_msc_write_cb(lun, lba, offset, buffer, bufsize)`:**  
     * Аналогично, транслирует запись блоков в `m_storage->write(...)`.  
3. **Метод `serialPrint(const std::string& line)`:**  
   * Проверяет, подключен ли CDC (`tud_cdc_connected()`).  
   * Если да, вызывает `tud_cdc_write(line.c_str(), line.length())` для отправки данных в COM-порт.

## **4\. Тестирование (Host-First)**

* HalUsb — это "железный" модуль. Он **не будет** компилироваться в `[env:native]`.  
* Вместо него `[env:native]` будет использовать **`MockHalUsb`** (Спринт 1.7).  
* `MockHalUsb` будет реализовывать тот же интерфейс `IHalUsb`.

```cpp
// (Пример в test/mocks/MockHalUsb.h)
#pragma once
#include "hal_interfaces/i_hal_usb.h"
#include <string>
#include <iostream> // Для std::cout

class MockHalUsb : public IHalUsb {
public:
    virtual bool init(IHalStorage* storage) override { 
        m_storagePassed = (storage != nullptr);
        std::cout << "[MockHalUsb] Init() called." << std::endl;
        return true; 
    }

    /**
     * @brief Эмулирует serialPrint, выводя лог в консоль (stdout).
     */
    virtual bool serialPrint(const std::string& line) override {
        // Требование Спринта 1.7
        std::cout << "[SERIAL] " << line << std::endl;
        m_serialPrintCount++;
        return true;
    }

    // --- Методы для тестов ---
    int getSerialPrintCount() const { return m_serialPrintCount; }
    bool wasInitCalledWithStorage() const { return m_storagePassed; }

private:
    int m_serialPrintCount = 0;
    bool m_storagePassed = false;
};
```
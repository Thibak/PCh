# **Детальное описание модуля: hal/usb**

**Назначение документа:** \> Этот документ описывает интерфейс (IHalUsb) и реализацию (HalUsb) для композитного USB-устройства.

## **1\. Назначение**

Модуль hal\_usb предоставляет абстрактный интерфейс IHalUsb и реализует сложную логику, необходимую для работы USB OTG.

Согласно ARCH\_CONTRACT.MD, его **основная задача** — при подключении к ПК представить себя как **композитное (составное) USB-устройство**, которое одновременно предоставляет:

1. **Mass Storage Class (MSC):** "Флешка" для прямого доступа к hal\_storage (для редактирования .cfg файлов).  
2. **Communications Device Class (CDC):** "COM-порт" (Serial) для вывода логов из diagnostics\_logging.

## **2\. Зависимости**

* **ESP-IDF / TinyUSB:** (Критическая) Реализация (HalUsb.cpp) будет напрямую использовать API TinyUSB (стандарт для ESP32-S3) для настройки дескрипторов композитного устройства и обработки колбэков.  
* **\#include "hal\_interfaces/i\_hal\_storage.h":** (Критическая) Модуль hal\_usb *не* работает с SPIFFS напрямую. Он получает указатель на IHalStorage во время init() и использует его для обслуживания запросов чтения/записи от ОС (через MSC).  
* **\#include "diagnostics/logger.h":** Для логирования собственных ошибок (напр., "TinyUSB stack failed to start").

## **3\. Логика работы**

### **3.1. Интерфейс (i\_hal\_usb.h)**

Интерфейс IHalUsb очень "тонкий". Большинство модулей (кроме Logger) вообще не знают о его существовании.

// (i\_hal\_usb.h)  
\#pragma once

\#include \<string\>  
\#include "i\_hal\_storage.h" // Для init()

class IHalUsb {  
public:  
    virtual \~IHalUsb() {}

    /\*\*  
     \* @brief Инициализирует стек TinyUSB как композитное устройство.  
     \* @param storage Указатель на уже инициализированный hal\_storage.  
     \* @return true, если USB-стек успешно запущен.  
     \*/  
    virtual bool init(IHalStorage\* storage) \= 0;

    /\*\*  
     \* @brief Отправляет строку в буфер USB Serial (CDC).  
     \* \* Этот метод должен быть потокобезопасным (использовать внутренний  
     \* мьютекс), но быстрым (не блокирующим надолго).  
     \* \* @param line Строка для отправки (уже отформатированная логгером).  
     \* @return true, если данные успешно помещены в буфер отправки.  
     \*/  
    virtual bool serialPrint(const std::string& line) \= 0;  
};

### **3.2. Реализация (HalUsb.cpp)**

Вся сложность скрыта здесь.

1. **Фаза init(IHalStorage\* storage):**  
   * Сохраняет указатель m\_storage \= storage.  
   * Инициализирует стек TinyUSB (tusb\_init()).  
   * Настраивает **дескрипторы** композитного устройства (сообщает ОС, что у него есть 1x MSC и 1x CDC).  
   * Регистрирует *колбэки (callbacks)* для MSC.  
   * Запускает задачу FreeRTOS tud\_task() (главная задача TinyUSB), если это не делается автоматически.  
2. **Колбэки Mass Storage (MSC):**  
   * **tud\_msc\_read\_cb(lun, lba, offset, buffer, bufsize):**  
     * Этот колбэк вызывается ОС, когда она хочет *прочитать* данные с "флешки".  
     * HalUsb должен транслировать lba (Logical Block Address) в вызов m\_storage-\>read(...) (это нетривиальная задача, требующая эмуляции FAT16/FAT32 или использования esp\_vfs\_fat\_spiffs\_format\_rw\_api).  
   * **tud\_msc\_write\_cb(lun, lba, offset, buffer, bufsize):**  
     * Аналогично, транслирует запись блоков в m\_storage-\>write(...).  
3. **Метод serialPrint(const std::string& line):**  
   * Проверяет, подключен ли CDC (tud\_cdc\_connected()).  
   * Если да, вызывает tud\_cdc\_write(line.c\_str(), line.length()) для отправки данных в COM-порт.

## **4\. Тестирование (Host-First)**

* HalUsb — это "железный" модуль. Он **не будет** компилироваться в \[env:native\].  
* Вместо него \[env:native\] будет использовать **MockHalUsb** (Спринт 1.4).  
* MockHalUsb будет реализовывать тот же интерфейс IHalUsb.

// (Пример в test/mocks/MockHalUsb.h)  
\#include "hal\_interfaces/i\_hal\_usb.h"  
\#include \<string\>

class MockHalUsb : public IHalUsb {  
public:  
    virtual bool init(IHalStorage\* storage) override {   
        m\_storagePassed \= (storage \!= nullptr);  
        return true;   
    }

    virtual bool serialPrint(const std::string& line) override {  
        m\_lastSerialLine \= line; // Сохраняем последнюю строку для теста  
        m\_serialPrintCount++;  
        return true;  
    }

    // \--- Методы для тестов \---  
    std::string getLastSerialLine() const { return m\_lastSerialLine; }  
    int getSerialPrintCount() const { return m\_serialPrintCount; }  
    bool wasInitCalledWithStorage() const { return m\_storagePassed; }

private:  
    std::string m\_lastSerialLine;  
    int m\_serialPrintCount \= 0;  
    bool m\_storagePassed \= false;  
};  

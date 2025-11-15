# **PCh \- Прошивка эмулятора волынки**

Это репозиторий для **PCh**, прошивки реального времени с открытым исходным кодом для эмулятора волынки на базе **ESP32-S3**.

Проект разработан с использованием **PlatformIO** и следует строгим архитектурным принципам **HAL (Hardware Abstraction Layer)** и **Host-First Development**. Это означает, что 100% бизнес-логики (APP и CORE) полностью отделены от "железа" (HAL) и могут быть протестированы на ПК (env:native) перед прошивкой на устройство.

## **1\. Ключевые Особенности**

* **BLE-MIDI:** Потоковая передача MIDI-сообщений (NoteOn/Off, PitchBend) в реальном времени на любое совместимое устройство (DAW, iPad, Android).  
* **Конфигурация через USB:** Устройство определяется как композитное USB-устройство:  
  * **Mass Storage (Флешка):** Позволяет пользователю редактировать settings.cfg и fingering.cfg в любом текстовом редакторе.  
  * **Serial (CDC):** Предоставляет COM-порт для потокового вывода логов.  
* **Продвинутая логика сенсоров:**  
  * Динамическое распознавание жестов (вибрато, полузакрытие).  
  * Отдельный сенсор для Mute (AllNotesOff).  
  * Высокоэффективная фильтрация шума (EMA-фильтр).  
* **Гибкая настройка:**  
  * **fingering.cfg:** Полная карта аппликатур (какая маска сенсоров какую ноту играет).  
  * **settings.cfg:** Полная настройка "железа" (карта пинов, пороги срабатывания, частота опроса) и логики (параметры жестов).  
* **Управление питанием:** Автоматическое полное выключение после 10 минут бездействия.  
* **Индикация:** Светодиодный индикатор для отображения статуса (поиск, подключен, отправка MIDI).

## **2\. Навигатор по проектной документации**

Этот проект следует принципу **"Design-First"**. Вся архитектура и логика модулей задокументированы в директории docs/ *перед* реализацией.

### **2.1. "Три Столпа" Архитектуры (Высокий уровень)**

* [**ARCH\_MANIFEST.MD**](./docs/ARCH_MANIFEST.MD) (Архитектурный манифест): **"КАК?"** — Наши правила, философия и принципы разработки (Host-First, HAL, Event-Driven).  
* [**PRD.MD**](./docs/PRD.MD) (Product Requirements Document): **"ЧТО?"** — Что мы строим? Пользовательские сценарии и требования.  
* [**ARCH\_CONTRACT.MD**](./docs/ARCH_CONTRACT.MD) (Архитектурный контракт): **"ЧЕМ?"** — Техническое описание реализации. Как модули (APP, HAL, CORE) связаны друг с другом.

### **2.2. План и Визуализация**

* [**DEVELOPMENT\_PLAN.MD**](./docs/DEVELOPMENT_PLAN.MD): **"КОГДА?"** — Полный план разработки, разбитый по Эпикам и Спринтам.  
* [**docs/architecture\_diagrams.md**](./docs/architecture_diagrams.md): **"ВИЗУАЛИЗАЦИЯ"** — Диаграммы Mermaid (компонентная, последовательности) для ARCH\_CONTRACT.MD.  
* [**docs/HW\_SPEC.md**](./docs/HW_SPEC.md): **"ЖЕЛЕЗО"** — Спецификация оборудования и физический Pinout Blueprint.

### **2.3. Схемы Данных (Контракты Данных)**

* [**docs/CONFIG\_SCHEMA.md**](./docs/CONFIG_SCHEMA.md): **(Критически важно)** — Детальное описание синтаксиса, секций и ключей для settings.cfg и fingering.cfg.

### **2.4. Детальное проектирование: CORE (Сервисы)**

* [**docs/modules/core\_config\_manager.md**](./docs/modules/core_config_manager.md): (Читает settings.cfg).  
* [**docs/modules/core\_event\_dispatcher.md**](./docs/modules/core_event_dispatcher.md): (Наша "нервная система" Pub/Sub на базе FreeRTOS).  
* [**docs/modules/diagnostics\_logging.md**](./docs/modules/diagnostics_logging.md): (Централизованный логгер LOG\_INFO(...)).  
* [**docs/modules/core\_scheduler.md**](./docs/modules/core_scheduler.md): (Отвечает за правильный порядок запуска init() и startTask()).

### **2.5. Детальное проектирование: HAL (Абстракция "Железа")**

* [**docs/modules/hal\_storage.md**](./docs/modules/hal_storage.md): (Абстракция над SPIFFS/LittleFS).  
* [**docs/modules/hal\_usb.md**](./docs/modules/hal_usb.md): (Композитное устройство: Mass Storage \+ Serial CDC).  
* [**docs/modules/hal\_led.md**](./docs/modules/hal_led.md): (Управление LED-индикатором, включая BLINK\_ONCE).  
* [**docs/modules/hal\_ble.md**](./docs/modules/hal_ble.md): (Абстракция над BLE-MIDI).  
* [**docs/modules/hal\_sensors.md**](./docs/modules/hal_sensors.md): (Опрос Touch-пинов, фильтрация шума (EMA)).  
* [**docs/modules/hal\_power.md**](./docs/modules/hal_power.md): (Таймер автовыключения и "soft-latch").

### **2.6. Детальное проектирование: APP (Бизнес-Логика)**

* **(Предстоит) docs/modules/app\_fingering.md**: (Парсер fingering.cfg и транслятор "Маска \-\> Нота").  
* **(Предстоит) docs/modules/app\_logic.md**: ("Мозг" устройства. Детектор маски, Mute, жесты).  
* **(Предстоит) docs/modules/app\_midi.md**: (Формирователь MIDI-сообщений: NoteOn, PitchBend, AllNotesOff).

## **3\. Сборка и Тестирование**

Проект использует PlatformIO и имеет два ключевых окружения:

1. **env:native (Host-First)**  
   * Компилирует проект для вашего ПК (Host).  
   * Не использует "железо" ESP32.  
   * Использует **Mock-объекты** (из test/mocks/) вместо HAL.  
   * Используется для запуска Unit-тестов и интеграционных тестов бизнес-логики.  
   * **Команда:** pio test \-e native  
2. **env:esp32s3 (Target/Device)**  
   * Компилирует проект для целевого устройства (ESP32-S3).  
   * Использует *настоящие* реализации HAL.  
   * Используется для прошивки устройства и Hardware-in-the-Loop (HIL) тестов.  
   * **Команда (сборка):** pio run \-t upload \-e esp32s3  
   * **Команда (монитор):** pio device monitor \-e esp32s3
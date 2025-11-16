# **Архитектурный контракт: Эмулятор волынки**

**Назначение документа:** \> Этот документ описывает *конкретную техническую реализацию* прошивки. Он служит связующим звеном между `PRD.MD` (требованиями) и `ARCH_MANIFEST.M`D (принципами). Он детализирует модули, их взаимодействие и то, как они реализуют пользовательские сценарии.

## **1\. Обзор архитектуры**

Проект реализует модульную, событийно-ориентированную архитектуру на базе  `FreeRTOS`, разделенную на два слоя:

1. **Application Layer (`app/`):** Бизнес-логика, не зависящая от оборудования.  
2. **Hardware Abstraction Layer (`hal/`):** Драйверы, специфичные для `ESP32-S3`.

Взаимодействие между слоями асинхронное, через событийную шину (`core/event_dispatcher`).

## **2\. Декомпозиция модулей и интерфейсы**

### **Слой HAL (Hardware Abstraction Layer)**

* **`hal_sensors` (Интерфейс: i_hal_sensor.h)**  
  * **Назначение:** "Драйвер". Считывание, фильтрация и нормализация сигналов со всех сенсоров.  
  * **Логика:** При инициализации получает список physical_pins из `core/config_manager`. Запускает опрос этих пинов.  
  * **События (Out):** `SensorValueChanged(sensor_id, value)` (где `sensor_id` \- это *логический ID* / *индекс* из списка `physical_pins`).  
* **`hal_ble` (Интерфейс: `i_hal_ble.h`)**  
  * **Назначение:** Транспорт MIDI. Управление BLE-соединением.  
  * **API (In):** `sendMidiMessage(message)`  
  * **События (Out):** `BleConnected`, `BleDisconnected`  
* **`hal_usb` (Интерфейс: `i_hal_usb.h`)**  
  * **Назначение:** Конфигурация и отладка.  
  * **Реализация:** **Композитное USB-устройство (USB 2.0 OTG)**. При подключении к ПК *одновременно* предоставляет:  
    1. **Mass Storage Device** (для доступа к файловой системе).  
    2. **CDC Serial Device** (для вывода логов).  
  * **API (In):** `serialPrint(logLine)`  
* **`hal_storage` (Интерфейс: `i_hal_storage.h`)**  
  * **Назначение:** Файловая система (`SPIFFS`/`LittleFS`).  
  * **API (In):** `readFile(path)`, `writeFile(path, data)`  
  * **Файлы:** Предоставляет доступ к `fingering.cfg` и `settings.cfg`.  
* **`hal_led` (Интерфейс: `i_hal_led.h`)**  
  * **Назначение:** Управление светодиодным индикатором.  
  * **API (In):** `setMode(LedMode mode)` (e.g., `OFF`, `SOLID`, `FAST_BLINK`, `BLINK_ONCE`).  
* **`hal_power` (Интерфейс: `i_hal_power.h`)**  
  * **Назначение:** Управление питанием.  
  * **API (In):** `triggerPowerOff()`, `getBatteryLevel()`  
* **`hal_system` (Интерфейс: `i_hal_system.h`)**
  * **Назначение:** Системные функции (таймеры, перезагрузка).
  * **API (In):** `getSystemTimestampMs()`, `reboot()`.

### **Слой APP (Application Layer)**

* **`app/logic`**  
  * **Назначение:** "Интерпретатор". Превращает "сырые" данные в действия.  
  * **Зависимости:** `core/config_manager` (для получения порогов и ID).  
  * **События (In):** `SensorValueChanged` (от `hal_sensors`).  
  * **Логика:**  
    1. При старте получает `mute_sensor_id` и `hole_sensor_ids` из `core/config_manager`.  
    2. **Сенсор звука (Mute):** Отслеживает `SensorValueChanged(id, ...)`. Если `id == mute_sensor_id` и `value > mute_threshold`, генерирует `MuteEnabled`.  
    3. **Распознавание маски:** Собирает маску, используя `hole_sensor_ids` для определения порядка бит.  
    4. **Распознавание жестов:** Анализирует сигналы `hole_sensor_ids` (согласно `PRD.MD` `3`) на основе порогов из `settings.cfg`.  
  * **События (Out):** `SensorMaskChanged(mask_8bit)`, `VibratoDetected(sensor_id, depth)`, `HalfHoleDetected(sensor_id)`, `MuteEnabled`, `MuteDisabled`.  
* **`app/fingering`**  
  * **Назначение:** "Транслятор аппликатуры".  
  * **Зависимости:** `hal_storage` (для загрузки `fingering.cfg` при старте).  
  * **События (In):** `SensorMaskChanged(mask_8bit)`, `HalfHoleDetected`.  
  * **Логика:**  
    1. При старте загружает и парсит `fingering.cfg`.  
    2. При `SensorMaskChanged` ищет 8-битную маску в карте.  
    3. При `HalfHoleDetected` проверяет, есть ли правило полузакрытия для *этой маски* и *этого `sensor_id`*.  
  * **События (Out):** `NotePitchSelected(pitch)` (включая `NoteOff`).  
* **`app/midi`**  
  * **Назначение:** "Формирователь MIDI".  
  * **Зависимости:** `i_hal_ble`, `i_hal_led`  (Получает `basePitchHz` от `core_scheduler` при `init()`).  
  * **События (In):** `NotePitchSelected`, `VibratoDetected`, `MuteEnabled`, `MuteDisabled`.  
  * **Логика:**  
    1. При `init()`: Сохраняет `basePitchHz`.
    2. При `BleConnected`: (TBD) Отправляет в `i_hal_ble` MIDI-сообщение (RPN/MTS) для установки `basePitchHz`.
    3. При `NotePitchSelected`: Формирует "Note On" (или "Note Off"). Отправляет в `i_hal_ble`. Вызывает `i_hal_led->setMode(BLINK_ONCE)`.  
    4. При `VibratoDetected`: Формирует MIDI-сообщение **Pitch Bend** с нужной depth. Отправляет в `i_hal_ble`.  
    5. При `MuteEnabled`: Формирует "All Notes Off" / `MUTE`. Отправляет в `i_hal_ble`.  
    6. При `MuteDisabled`: (Если требуется) Отправляет "All Notes On" (или отменяет `MUTE`).

### **CORE (Ядро)**

* **`core/event_dispatcher`:** Событийная шина (на базе очередей `FreeRTOS`).  
* **`core/scheduler`:** Управление задачами `FreeRTOS`.  
* **`core/config_manager`**  
  * **Назначение:** Менеджер конфигурации.  
  * **Логика:** При старте читает `settings.cfg` из `hal_storage`. Парсит и хранит настройки в RAM.  
  * **API (In):** `getLogLevel()`, `getPhysicalPins()`, `getMuteSensorId()`, `getHoleSensorIds()`, `getMuteThreshold()`, `getVibratoThreshold()`, `getHalfHoleThreshold()`.  
* **`diagnostics & logging`:**
  * **Назначение:** Служба сбора логов.
  * **Зависимости:** `core/config_manager`, `i_hal_usb`, `i_hal_system`.
  * **Логика:** Принимает логи от всех модулей. Проверяет `core/config_manager->getLogLevel()`. Если уровень достаточный, запрашивает `i_hal_system->getSystemTimestampMs()`, форматирует строку и направляет лог в `hal_usb->serialPrint(...)`.

## **3\. Реализация ключевых сценариев (PRD)**

### **Сценарий 1: Основной сценарий (BLE MIDI)**

1. **`hal_sensors`** (Задача) опрашивает `physical_pins`, генерирует `SensorValueChanged(id, ...)`.  
2. **`app/logic`** (Задача) получает событие.  
   * Если `id == mute_sensor_id` \-\> "ВЫКЛ", генерирует `MuteEnabled`.  
   * Если id в списке `hole_sensor_ids`, анализирует жесты и маску. Генерирует `SensorMaskChanged`, `VibratoDetected` и/или `HalfHoleDetected`.  
3. **`app/fingering`** (Обработчик) получает `SensorMaskChanged(mask_8bit)` или `HalfHoleDetected`. Ищет в `fingering.cfg` ноту. Генерирует `NotePitchSelected(pitch)`.  
4. **`app/midi`** (Обработчик) получает `NotePitchSelected` ИЛИ `VibratoDetected` ИЛИ `MUTE_MODE`.  
* Если это игровой сенсор, анализирует жесты (вибрато, полузакрытие) и маску. Генерирует `SensorMaskChanged`, `VibratoDetected` и/или `HalfHoleDetected`.  
3. **`app/fingering`** (Обработчик) получает `SensorMaskChanged` или `HalfHoleDetected`. Ищет в `fingering.cfg` ноту. Генерирует `NotePitchSelected(pitch)`.  
4. **`app/midi`** (Обработчик) получает `NotePitchSelected` ИЛИ `VibratoDetected` ИЛИ `MuteEnabled`.  
   * `NotePitchSelected`: Отправляет "Note On" в `hal_ble` и вызывает `hal_led->setMode(BLINK_ONCE)`.  
   * `VibratoDetected`: Отправляет "Pitch Bend" в `hal_ble`.  
   * `MuteEnabled`: Отправляет "All Notes Off" в `hal_ble`.

### **Сценарий 2: Редактирование аппликатуры и настроек (USB)**

1. Пользователь подключает USB.  
2. **`hal_usb`** (Задача) определяет подключение и активирует **оба** интерфейса: **Mass Storage** и **Serial (CDC)**.  
3. **`hal_usb`** использует `i_hal_storage` для предоставления доступа к файловой системе (включая `fingering.cfg` и `settings.cfg`) компьютеру.  
4. Пользователь редактирует файлы.  
5. Пользователь отключает USB.  
6. При следующей загрузке `core/config_manager` и `app/fingering` запрашивают обновленные файлы у `i_hal_storage`.

### **Сценарий 3: Логирование и Отладка (USB)**

1. Устройство подключено по USB (см. Сценарий 2). **Serial (CDC)** интерфейс активен *всегда*.  
2. Пользователь открывает Serial Monitor (PuTTY, PlatformIO).  
3. Модули `app/*` и `hal/*` вызывают `LOG_INFO(...)`.  
4. Служба **`diagnostics & logging`** (Задача) получает лог.  
5. Она запрашивает `core/config_manager->getLogLevel()` (который был загружен из `settings.cfg`).  
6. Если уровень лога (`INFO`) выше или равен уровню в конфиге (`DEBUG` или `INFO`), служба запрашивает `i_hal_system->getSystemTimestampMs()`, форматирует лог (согласно Разделу 4) и вызывает `i_hal_usb->serialPrint(logLine)`.
7. **`hal_usb`** отправляет строку в COM-порт.

### **Сценарий 4: Управление LED-индикатором (UX)**

1. **`hal_ble`** (Задача) отслеживает состояние BLE.  
2. При `BleDisconnected` (инициализация или потеря связи), `hal_ble` вызывает `i_hal_led->setMode(FAST_BLINK)`.  
3. При `BleConnected`, `hal_ble` вызывает `i_hal_led->setMode(SOLID)`.  
4. *Одновременно (см. Сценарий 1\)*: **`app/midi`** при отправке ноты вызывает `i_hal_led->setMode(BLINK_ONCE)`. `hal_led` должен быть способен обработать `BLINK_ONCE`, не прерывая `SOLID` (т.е. моргнуть и вернуться в `SOLID`).

## **4\. Реализация подсистем (NFR)**

* **Энергосбережение (PRD 4):**  
  * **`hal_power`** реализует таймер бездействия (10 минут).  
  * При срабатывании таймера `hal_power` генерирует событие `SystemIdleTimeout`.  
  * `core/scheduler` (или главный управляющий модуль) обрабатывает это событие и, убедившись, что можно безопасно выключиться (например, нет активного USB-подключения), вызывает `i_hal_power->triggerPowerOff()`.  
  * `i_hal_power->triggerPowerOff()` инициирует аппаратное (`soft-latch`) полное выключение устройства.  
  * Включение устройства происходит только физической кнопкой питания.  
* **Логирование и Форматы (PRD 3):**  
  * Логи хранятся во временном буфере в RAM (циклический буфер) перед отправкой в `hal_usb`.  
  * Уровень логирования (LogLevel) определяется core/config_manager (из `settings.cfg`).  
  * **Формат логов:** Cтруктурированный plain text, выводимый в `hal_usb` (Serial).  
  * **Формат:** `timestamp - module - level - message` (например, `123456 - app/logic - INFO - SensorMaskChanged: 0b1101`). `timestamp` предоставляется `i_hal_system`.
  * Реализация форматирования инкапсулируется в службе d `iagnostics & logging`.

## **5. Структура репозитория и файлов**

Этот раздел детализирует физическое расположение файлов в репозитории, следуя принципам, изложенным в `ARCH_MANIFEST.MD` (Раздел 4) и плане `DEVELOPMENT_PLAN.MD`.

/
├── data/                     # Исходные "дефолтные" конфиги для `pio run -t uploadfs`
│   ├── settings.cfg
│   └── fingering.cfg
│
├── docs/                     # Вся проектная документация (Эпик 0)
│   ├── modules/              # Детальное описание каждого модуля
│   │   ├── app_fingering.md
│   │   ├── app_logic.md
│   │   ├── app_midi.md
│   │   ├── core_config_manager.md
│   │   ├── core_event_dispatcher.md
│   │   ├── core_scheduler.md
│   │   ├── diagnostics_logging.md
│   │   ├── hal_ble.md
│   │   ├── hal_led.md
│   │   ├── hal_power.md
│   │   ├── hal_sensors.md
│   │   ├── hal_storage.md
│   │   ├── hal_system.md
│   │   └── hal_usb.md
│   ├── ARCH_CONTRACT.MD      # (Этот файл)
│   ├── ARCH_MANIFEST.MD
│   ├── architecture_diagrams.md
│   ├── CONFIG_SCHEMA.md
│   ├── HW_SPEC.MD
│   └── PRD.MD
│
├── include/                  # Публичные заголовки (.h), интерфейсы, структуры данных
│   ├── interfaces/           # Абстрактные интерфейсы (Контракты HAL)
│   │   ├── IEventHandler.h
│   │   ├── IHalBle.h
│   │   ├── IHalLed.h
│   │   ├── IHalPower.h
│   │   ├── IHalSensors.h
│   │   ├── IHalStorage.h
│   │   ├── IHalSystem.h
│   │   └── IHalUsb.h
│   │
│   ├── app/                  # Публичные заголовки (.h) модулей APP
│   │   ├── AppFingering.h
│   │   ├── AppLogic.h
│   │   └── AppMidi.h
│   │
│   ├── core/                 # Публичные заголовки (.h) модулей CORE
│   │   ├── ConfigManager.h
│   │   ├── EventDispatcher.h
│   │   ├── Logger.h
│   │   └── Scheduler.h
│   │
│   ├── hal/                  # Публичные заголовки (.h) *реальных* HAL модулей
│   │   ├── HalBle.h
│   │   ├── HalLed.h
│   │   └── (и т.д. ... )
│   │
│   ├── events.h              # Enum EventType и struct Event
│   └── LogLevel.h            # Enum LogLevel
│
├── lib/                      # Внешние библиотеки (управляются PlatformIO)
│   └── Arduino-BLE-MIDI/     # (Например)
│
├── src/                      # Исходный код (.cpp)
│   ├── app/                  # Реализация (.cpp) бизнес-логики (Host + Target)
│   │   ├── AppFingering.cpp
│   │   ├── AppLogic.cpp
│   │   └── AppMidi.cpp
│   │
│   ├── core/                 # Реализация (.cpp) сервисов (Host + Target)
│   │   ├── ConfigManager.cpp
│   │   ├── EventDispatcher.cpp
│   │   ├── Logger.cpp
│   │   └── Scheduler.cpp
│   │
│   ├── hal/                  # Реализация (.cpp) "железных" модулей (Только Target)
│   │   ├── HalBle.cpp
│   │   ├── HalLed.cpp
│   │   ├── HalPower.cpp
│   │   ├── HalSensors.cpp
│   │   ├── HalStorage.cpp
│   │   ├── HalSystem.cpp
│   │   └── HalUsb.cpp
│   │
│   └── main.cpp              # Главная точка входа (запускает Scheduler)
│
├── test/                     # Тесты (Только Host, env:native)
│   ├── mocks/                # Mock-реализации HAL (Эпик 1)
│   │   ├── MockHalBle.h
│   │   ├── MockHalLed.h
│   │   ├── MockHalPower.h
│   │   ├── MockHalSensors.h
│   │   ├── MockHalStorage.h
│   │   ├── MockHalSystem.h
│   │   └── MockHalUsb.h
│   │
│   ├── unit_app/             # Unit-тесты для APP
│   │   └── test_app_fingering.cpp
│   │
│   ├── unit_core/            # Unit-тесты для CORE
│   │   └── test_config_manager.cpp
│   │
│   ├── integration/          # Интеграционные тесты Host-First (Эпик 2)
│   │   ├── test_host_logic_e2e.cpp  # (Главный тест логики)
│   │   └── test_host_midi.cpp
│   │
│   └── test_main.cpp         # Точка входа для `pio test -e native`
│
├── platformio.ini            # Конфигурация PlatformIO (Спринт 1.1)
└── README.md                 # Общее описание проекта

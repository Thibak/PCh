# **Архитектурный контракт: Эмулятор волынки**

**Назначение документа:** \> Этот документ описывает *конкретную техническую реализацию* прошивки. Он служит связующим звеном между PRD.MD (требованиями) и ARCH\_MANIFEST.MD (принципами). Он детализирует модули, их взаимодействие и то, как они реализуют пользовательские сценарии.

## **1\. Обзор архитектуры**

Проект реализует модульную, событийно-ориентированную архитектуру на базе FreeRTOS, разделенную на два слоя:

1. **Application Layer (app/):** Бизнес-логика, не зависящая от оборудования.  
2. **Hardware Abstraction Layer (hal/):** Драйверы, специфичные для ESP32-S3.

Взаимодействие между слоями асинхронное, через событийную шину (core/event\_dispatcher).

## **2\. Декомпозиция модулей и интерфейсы**

### **Слой HAL (Hardware Abstraction Layer)**

* **hal\_sensors (Интерфейс: i\_hal\_sensor.h)**  
  * **Назначение:** "Драйвер". Считывание, фильтрация и нормализация сигналов со всех сенсоров.  
  * **Логика:** При инициализации получает список physical\_pins из core/config\_manager. Запускает опрос этих пинов.  
  * **События (Out):** SensorValueChanged(sensor\_id, value) (где sensor\_id \- это *логический ID* / *индекс* из списка physical\_pins).  
* **hal\_ble (Интерфейс: i\_hal\_ble.h)**  
  * **Назначение:** Транспорт MIDI. Управление BLE-соединением.  
  * **API (In):** sendMidiMessage(message)  
  * **События (Out):** BleConnected, BleDisconnected  
* **hal\_usb (Интерфейс: i\_hal\_usb.h)**  
  * **Назначение:** Конфигурация и отладка.  
  * **Реализация:** **Композитное USB-устройство (USB 2.0 OTG)**. При подключении к ПК *одновременно* предоставляет:  
    1. **Mass Storage Device** (для доступа к файловой системе).  
    2. **CDC Serial Device** (для вывода логов).  
  * **API (In):** serialPrint(logLine)  
* **hal\_storage (Интерфейс: i\_hal\_storage.h)**  
  * **Назначение:** Файловая система (SPIFFS/LittleFS).  
  * **API (In):** readFile(path), writeFile(path, data)  
  * **Файлы:** Предоставляет доступ к fingering.cfg и settings.cfg.  
* **hal\_led (Интерфейс: i\_hal\_led.h)**  
  * **Назначение:** Управление светодиодным индикатором.  
  * **API (In):** setMode(LedMode mode) (e.g., OFF, SOLID, FAST\_BLINK, BLINK\_ONCE).  
* **hal\_power (Интерфейс: i\_hal\_power.h)**  
  * **Назначение:** Управление питанием.  
  * **API (In):** triggerPowerOff(), getBatteryLevel()  
* **hal\_system (Интерфейс: i\_hal\_system.h)**  
  * **Назначение:** Системные функции (таймеры, перезагрузка).

### **Слой APP (Application Layer)**

* **app/logic**  
  * **Назначение:** "Интерпретатор". Превращает "сырые" данные в действия.  
  * **Зависимости:** core/config\_manager (для получения порогов и ID).  
  * **События (In):** SensorValueChanged (от hal\_sensors).  
  * **Логика:**  
    1. При старте получает mute\_sensor\_id и hole\_sensor\_ids из core/config\_manager.  
    2. **Сенсор звука (Mute):** Отслеживает SensorValueChanged(id, ...). Если id \== mute\_sensor\_id и value \> mute\_threshold, генерирует MuteEnabled.  
    3. **Распознавание маски:** Собирает маску, используя hole\_sensor\_ids для определения порядка бит.  
    4. **Распознавание жестов:** Анализирует сигналы hole\_sensor\_ids (согласно PRD.MD 3\) на основе порогов из settings.cfg.  
  * **События (Out):** SensorMaskChanged(mask\_8bit), VibratoDetected(sensor\_id, depth), HalfHoleDetected(sensor\_id), MuteEnabled, MuteDisabled.  
* **app/fingering**  
  * **Назначение:** "Транслятор аппликатуры".  
  * **Зависимости:** hal\_storage (для загрузки fingering.cfg при старте).  
  * **События (In):** SensorMaskChanged(mask\_8bit), HalfHoleDetected.  
  * **Логика:**  
    1. При старте загружает и парсит fingering.cfg.  
    2. При SensorMaskChanged ищет 8-битную маску в карте.  
    3. При HalfHoleDetected проверяет, есть ли правило полузакрытия для *этой маски* и *этого sensor\_id*.  
  * **События (Out):** NotePitchSelected(pitch) (включая NoteOff).  
* **app/midi**  
  * **Назначение:** "Формирователь MIDI".  
  * **Зависимости:** i\_hal\_ble, i\_hal\_led.  
  * **События (In):** NotePitchSelected, VibratoDetected, MuteEnabled, MuteDisabled.  
  * **Логика:**  
    1. При NotePitchSelected: Формирует "Note On" (или "Note Off"). Отправляет в i\_hal\_ble. Вызывает i\_hal\_led-\>setMode(BLINK\_ONCE).  
    2. При VibratoDetected: Формирует MIDI-сообщение **Pitch Bend** с нужной depth. Отправляет в i\_hal\_ble.  
    3. При MuteEnabled: Формирует "All Notes Off" / MUTE. Отправляет в i\_hal\_ble.  
    4. При MuteDisabled: (Если требуется) Отправляет "All Notes On" (или отменяет MUTE).

### **CORE (Ядро)**

* **core/event\_dispatcher:** Событийная шина (на базе очередей FreeRTOS).  
* **core/scheduler:** Управление задачами FreeRTOS.  
* **core/config\_manager (НОВЫЙ МОДУЛЬ)**  
  * **Назначение:** Менеджер конфигурации.  
  * **Логика:** При старте читает settings.cfg из hal\_storage. Парсит и хранит настройки в RAM.  
  * **API (In):** getLogLevel(), getPhysicalPins(), getMuteSensorId(), getHoleSensorIds(), getMuteThreshold(), getVibratoThreshold(), getHalfHoleThreshold().  
* **diagnostics & logging:**  
  * **Назначение:** Служба сбора логов.  
  * **Логика:** Принимает логи от всех модулей. Проверяет core/config\_manager-\>getLogLevel(). Если уровень достаточный, форматирует и направляет лог в hal\_usb-\>serialPrint(...).

## **3\. Реализация ключевых сценариев (PRD)**

### **Сценарий 1: Основной сценарий (BLE MIDI)**

1. **hal\_sensors** (Задача) опрашивает physical\_pins, генерирует SensorValueChanged(id, ...).  
2. **app/logic** (Задача) получает событие.  
   * Если id \== mute\_sensor\_id \-\> "ВЫКЛ", генерирует MuteEnabled.  
   * Если id в списке hole\_sensor\_ids, анализирует жесты и маску. Генерирует SensorMaskChanged, VibratoDetected и/или HalfHoleDetected.  
3. **app/fingering** (Обработчик) получает SensorMaskChanged(mask\_8bit) или HalfHoleDetected. Ищет в fingering.cfg ноту. Генерирует NotePitchSelected(pitch).  
4. **app/midi** (Обработчик) получает NotePitchSelected ИЛИ VibratoDetected ИЛИ MUTE\_MODE.  
* Если это игровой сенсор, анализирует жесты (вибрато, полузакрытие) и маску. Генерирует SensorMaskChanged, VibratoDetected и/или HalfHoleDetected.  
3. **app/fingering** (Обработчик) получает SensorMaskChanged или HalfHoleDetected. Ищет в fingering.cfg ноту. Генерирует NotePitchSelected(pitch).  
4. **app/midi** (Обработчик) получает NotePitchSelected ИЛИ VibratoDetected ИЛИ MuteEnabled.  
   * NotePitchSelected: Отправляет "Note On" в hal\_ble и вызывает hal\_led-\>setMode(BLINK\_ONCE).  
   * VibratoDetected: Отправляет "Pitch Bend" в hal\_ble.  
   * MuteEnabled: Отправляет "All Notes Off" в hal\_ble.

### **Сценарий 2: Редактирование аппликатуры и настроек (USB)**

1. Пользователь подключает USB.  
2. **hal\_usb** (Задача) определяет подключение и активирует **оба** интерфейса: **Mass Storage** и **Serial (CDC)**.  
3. **hal\_usb** использует i\_hal\_storage для предоставления доступа к файловой системе (включая fingering.cfg и settings.cfg) компьютеру.  
4. Пользователь редактирует файлы.  
5. Пользователь отключает USB.  
6. При следующей загрузке core/config\_manager и app/fingering запрашивают обновленные файлы у i\_hal\_storage.

### **Сценарий 3: Логирование и Отладка (USB)**

1. Устройство подключено по USB (см. Сценарий 2). **Serial (CDC)** интерфейс активен *всегда*.  
2. Пользователь открывает Serial Monitor (PuTTY, PlatformIO).  
3. Модули app/\* и hal/\* вызывают LOG\_INFO(...).  
4. Служба **diagnostics & logging** (Задача) получает лог.  
5. Она запрашивает core/config\_manager-\>getLogLevel() (который был загружен из settings.cfg).  
6. Если уровень лога (INFO) выше или равен уровню в конфиге (DEBUG или INFO), служба форматирует лог (согласно Разделу 4\) и вызывает i\_hal\_usb-\>serialPrint(logLine).  
7. **hal\_usb** отправляет строку в COM-порт.

### **Сценарий 4: Управление LED-индикатором (UX)**

1. **hal\_ble** (Задача) отслеживает состояние BLE.  
2. При BleDisconnected (инициализация или потеря связи), hal\_ble вызывает i\_hal\_led-\>setMode(FAST\_BLINK).  
3. При BleConnected, hal\_ble вызывает i\_hal\_led-\>setMode(SOLID).  
4. *Одновременно (см. Сценарий 1\)*: **app/midi** при отправке ноты вызывает i\_hal\_led-\>setMode(BLINK\_ONCE). hal\_led должен быть способен обработать BLINK\_ONCE, не прерывая SOLID (т.е. моргнуть и вернуться в SOLID).

## **4\. Реализация подсистем (NFR)**

* **Энергосбережение (PRD 4):**  
  * **hal\_power** реализует таймер бездействия (10 минут).  
  * При срабатывании таймера hal\_power генерирует событие SystemIdleTimeout.  
  * core/scheduler (или главный управляющий модуль) обрабатывает это событие и, убедившись, что можно безопасно выключиться (например, нет активного USB-подключения), вызывает i\_hal\_power-\>triggerPowerOff().  
  * i\_hal\_power-\>triggerPowerOff() инициирует аппаратное (soft-latch) полное выключение устройства.  
  * Включение устройства происходит только физической кнопкой питания.  
* **Логирование и Форматы (PRD 3):**  
  * Логи хранятся во временном буфере в RAM (циклический буфер) перед отправкой в hal\_usb.  
  * Уровень логирования (LogLevel) определяется core/config\_manager (из settings.cfg).  
  * **Формат логов:** Cтруктурированный plain text, выводимый в hal\_usb (Serial).  
  * **Формат:** timestamp \- module \- level \- message (например, 123456 \- app/logic \- INFO \- SensorMaskChanged: 0b1101).  
  * Реализация форматирования инкапсулируется в службе diagnostics & logging.
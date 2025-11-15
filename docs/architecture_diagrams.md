# **Архитектурные диаграммы**

**Назначение документа:** \> Этот документ визуализирует архитектурные решения, описанные в ARCH_CONTRACT.MD, с использованием диаграмм Mermaid. Он служит для наглядного представления статической структуры и динамического взаимодействия модулей.

## **1\. Компонентная диаграмма (C4 \- Уровень 3\)**

Эта диаграмма показывает основные модули (HAL, APP, CORE) и их ключевые зависимости.
```mermaid
graph TD  
    subgraph "Hardware (Физический уровень)"  
        direction LR  
        Sensors["Сенсоры (Touch Pins)"]  
        LED["LED (встроенный)"]  
        BLE_HW[BLE Радио]  
        USB_HW["USB-C (встроенный)"]  
        PSW["Кнопка питания (внешняя)"]  
    end

    subgraph "Application Layer (app/)"  
        direction TB  
        APP_Logic["app/logic\n(Интерпретатор)"]  
        APP_Fingering["app/fingering\n(Транслятор аппликатуры)"]  
        APP_Midi["app/midi\n(Формирователь MIDI)"]  
    end

    subgraph "HAL (hal/)"  
        direction TB  
        HAL_Sensors["hal_sensors\n(i_hal_sensor.h)"]  
        HAL_LED["hal_led\n(i_hal_led.h)"]  
        HAL_BLE["hal_ble\n(i_hal_ble.h)"]  
        HAL_USB["hal_usb\n(i_hal_usb.h)"]  
        HAL_Storage["hal_storage\n(i_hal_storage.h)"]  
        HAL_Power["hal_power\n(i_hal_power.h)"]  
    end

    subgraph "CORE (core/)"  
        direction TB  
        CORE_Event["core/event_dispatcher\n(Событийная шина)"]  
        CORE_Config["core/config_manager\n(Менеджер конфиг.)"]  
        CORE_Log["diagnostics & logging\n(Логирование)"]  
        CORE_Scheduler["core/scheduler\n(Планировщик)"]  
    end

    %% Связи APP  
    APP_Logic --> CORE_Event  
    APP_Fingering --> CORE_Event  
    APP_Midi --> CORE_Event  
      
    APP_Midi -->|Вызовы API| HAL_BLE  
    APP_Midi -->|Вызовы API| HAL_LED  
    APP_Fingering -->|Вызовы API| HAL_Storage  
    APP_Logic -->|Вызовы API| CORE_Config

    %% Связи HAL  
    Sensors --> HAL_Sensors  
    HAL_Sensors --> CORE_Event  
      
    HAL_LED --> LED  
    HAL_BLE --> BLE_HW  
    HAL_Power --> PSW  
      
    HAL_USB --> USB_HW  
    HAL_Storage -.->|Доступ к FS| HAL_USB  
    CORE_Log -->|Вызовы API| HAL_USB

    %% Связи CORE  
    CORE_Config -->|Вызовы API| HAL_Storage  
    CORE_Scheduler -->|Вызовы API| HAL_Power  
      
    classDef hal fill:#DDEBF7,stroke:#333  
    classDef app fill:#E2F0D9,stroke:#333  
    classDef core fill:#FFF2CC,stroke:#333  
    classDef hw fill:#FBE5D6,stroke:#333  
      
    class HAL_Sensors,HAL_LED,HAL_BLE,HAL_USB,HAL_Storage,HAL_Power hal
    class APP_Logic,APP_Fingering,APP_Midi app
    class CORE_Event,CORE_Config,CORE_Log,CORE_Scheduler core
    class Sensors,LED,BLE_HW,USB_HW,PSW hw
```
## **2\. Диаграммы последовательности (Sequence Diagrams)**

### **Сценарий 1: Основной сценарий (BLE MIDI) \- (Обновлено)**

Эта диаграмма показывает "горячий путь" (hot path) и параллельную обработку "сенсора звука".
```mermaid
sequenceDiagram  
    participant S as "hal_sensors"  
    participant E as "core/event_dispatcher"  
    participant L as "app/logic"  
    participant F as "app/fingering"  
    participant M as "app/midi"  
    participant B as "hal_ble"  
    participant D as "hal_led"

    S->>E: Событие: SensorValueChanged(id, raw_value)  
    E->>L: SensorValueChanged(id, raw_value)

    par Основной поток (Ноты)  
        L->>L: Анализ (Нажатие? Полузакрытие?)  
        L->>E: Событие: SensorMaskChanged  
        E->>F: SensorMaskChanged  
        F-->>F: Поиск ноты в fingering.cfg  
        F->>E: Событие: NotePitchSelected(NoteOn)  
        E->>M: NotePitchSelected  
        M->>B: API: sendMidiMessage(NoteOn)  
        M->>D: API: setMode(BLINK_ONCE)  
    and Поток жестов (Вибрато)  
        L->>L: Анализ (Вибрато?)  
        L->>E: Событие: VibratoDetected  
        E->>M: VibratoDetected  
        M->>B: API: sendMidiMessage(PitchBend)  
    and Поток "Сенсора Звука" (Mute)  
        L->>L: Анализ (Mute?)  
        L->>E: Событие: MuteEnabled  
        E->>M: MuteEnabled  
        M->>B: API: sendMidiMessage(AllNotesOff)  
    end
```
### **Сценарий 2 и 3: Композитное USB (Конфигурация и Логирование)**

Эта диаграмма показывает *одновременную* работу двух функций композитного USB-устройства.
```mermaid
sequenceDiagram  
    participant User as "Пользователь (ПК)"  
    participant USB as "hal_usb"  
    participant FS as "hal_storage"  
    participant LOG as "diagnostics & logging"  
    participant CFG as "core/config_manager"  
    participant APP as "app/module"

    par Редактирование конфига (Mass Storage)  
        User->>USB: Чтение файла settings.cfg  
        USB->>FS: API: readFile("settings.cfg")  
        FS-->>USB: (данные файла)  
        USB-->>User: (данные файла)  
    and Чтение логов (Serial CDC)  
        APP->>LOG: LOG_INFO("Сообщение...")  
        LOG->>CFG: API: getLogLevel()  
        CFG-->>LOG: "INFO"  
        opt Уровень лога соответствует  
            LOG->>LOG: formatLine(...)  
            LOG->>USB: API: serialPrint("ts \- app \- INFO \- ...")  
            USB-->>User: (поток текста в Serial)  
        end  
    end
```
## **3\. Диаграмма физической компоновки (C4 \- Уровень 1\) \- (Обновлено)**

Эта диаграмма показывает физические компоненты устройства и их взаимосвязи, учитывая использование встроенных компонентов платы ESP32-S3.
```mermaid
graph TD  
    subgraph "Корпус устройства"  
        subgraph "Плата разработки (Dev Board)"  
            ESP32["ESP32-S3"]  
            TP4056["Модуль зарядки\n(TP4056)"]  
            BOOST["Boost-конвертор\n(DC-DC 3.7V -> 3.3V)"]  
            USB["USB-C (встроенный)"]  
            Board_Btns["Кнопки (RST/BOOT, встр.)"]  
            LED["LED (встроенный, напр. Neopixel)"]  
        end  
          
        BAT["Аккумулятор (LiPo 3.7V)"]  
        SENS["Сенсоры (Емкостные пины)"]  
        PSW["Кнопка питания (Вкл/Выкл)"]

        USB -- "Питание (5V)" --> TP4056  
        USB -- "Данные (D+/D-)" --> ESP32  
        TP4056 -- "Зарядка" --> BAT  
        BAT -- "Питание (3.7V)" --> BOOST  
        BOOST -- "Стаб. VCC (3.3V)" --> ESP32  
        BOOST -- "Стаб. VCC (3.3V)" --> SENS  
        SENS -- "Данные (Touch Pins)" --> ESP32  
        LED -- "Управление" --> ESP32  
        Board_Btns -- "Управление" --> ESP32  
        PSW -- "Вкл/Выкл (Soft Latch)" --> BOOST  
    end  
      
    classDef pcb fill:#DDEBF7,stroke:#333  
    classDef ext fill:#E2F0D9,stroke:#333  
      
    class ESP32,TP4056,BOOST,USB,Board_Btns,LED pcb
    class BAT,SENS,PSW ext
```
## **4\. Диаграмма управления LED (Сценарий 4\) \- (Исправлено)**

Эта диаграмма показывает, как разные модули управляют состоянием одного светодиода.
```mermaid
sequenceDiagram  
    participant BLE as "hal_ble"  
    participant MIDI as "app/midi"  
    participant LED as "hal_led"

    %% Сценарий подключения  
    BLE->>LED: API: setMode(FAST_BLINK)  
    note right of BLE: Инициализация, поиск...  
      
    rect rgb(240, 240, 240)  
        note over BLE,LED: ...проходит время...  
    end  
      
    BLE->>LED: API: setMode(SOLID)  
    note right of BLE: Устройство подключено

    %% Сценарий отправки MIDI (во время SOLID)  
    MIDI->>LED: API: setMode(BLINK_ONCE)  
    note left of LED: LED моргает 1 раз и \nвозвращается в SOLID  
      
    rect rgb(240, 240, 240)  
        note over MIDI,LED: ...проходит время, игра...  
    end  
      
    MIDI->>LED: API: setMode(BLINK_ONCE)  
      
    %% Сценарий отключения  
    BLE->>LED: API: setMode(FAST_BLINK)  
    note right of BLE: Соединение потеряно
```

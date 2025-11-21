/*
 * AppLogic.cpp
 *
 * Реализация "мозга" устройства.
 * Обрабатывает сырые данные сенсоров, детектирует жесты и управляет состоянием.
 *
 * Соответствует: docs/modules/app_logic.md
 * DEVELOPMENT_PLAN.MD - Спринт 2.7 / 2.10
 */
#include "app/AppLogic.h"
#include "core/Logger.h"
#include <iostream> // Для отладки в Native
#include <numeric>  // Для std::accumulate

#define TAG "AppLogic"

// --- Настройка окружения (RTOS vs Native) ---
// Используем условную компиляцию для поддержки тестов на ПК без FreeRTOS
#if defined(ESP32_TARGET)
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
#elif defined(NATIVE_TEST)
    // Заглушки типов и макросов FreeRTOS для компиляции на хосте
    typedef void* QueueHandle_t;
    #define xQueueCreate(len, size) ((void*)1) // Возвращаем не-null указатель
    #define xQueueSend(queue, item, ticks) (true)
    #define xQueueReceive(queue, item, ticks) (true)
    #define xTaskCreate(task, name, stack, params, prio, handle) (pdPASS)
    #define vTaskDelay(ticks) ((void)0)
    #define pdPASS 1
    #define portMAX_DELAY 0
#endif

// --- Конструктор ---
AppLogic::AppLogic() 
    : m_dispatcher(nullptr), 
      m_configManager(nullptr),
      m_sensorQueue(nullptr),
      m_isMuted(false),
      m_currentMask(0) {
    // Инициализация массивов и переменных происходит в списке инициализации
}

// --- Инициализация ---
bool AppLogic::init(ConfigManager* configManager, EventDispatcher* dispatcher) {
    // 1. Сброс состояния (ВАЖНО для тестов и корректной перезагрузки конфига)
    m_isMuted = false;
    m_currentMask = 0;
    // Сбрасываем состояния всех сенсоров в OPEN и очищаем историю вибрато
    for (int i = 0; i < 16; ++i) {
        m_sensorContexts[i] = SensorContext(); 
    }

    m_configManager = configManager;
    m_dispatcher = dispatcher;
    
    // 2. Загружаем параметры из ConfigManager
    // Это гарантирует, что мы используем актуальные настройки из settings.cfg
    m_muteSensorId = m_configManager->getMuteSensorId();
    m_holeSensorIds = m_configManager->getHoleSensorIds();
    
    // Пороги срабатывания сенсоров
    m_muteThreshold = m_configManager->getMuteThreshold();
    m_holeClosedThreshold = m_configManager->getHoleClosedThreshold();
    m_halfHoleThreshold = m_configManager->getHalfHoleThreshold();

    // Параметры для алгоритма детекции вибрато
    m_vibratoFreqMin = m_configManager->getVibratoFreqMin();
    m_vibratoFreqMax = m_configManager->getVibratoFreqMax();
    m_vibratoAmplitudeMin = m_configManager->getVibratoAmplitudeMin();

    // 3. Создание очереди событий
    #if defined(ESP32_TARGET)
    // В RTOS создаем реальную очередь.
    // Если init вызывается повторно (перезагрузка конфига), очередь может уже существовать.
    if (m_sensorQueue == nullptr) {
        m_sensorQueue = xQueueCreate(20, sizeof(Event));
        if (m_sensorQueue == nullptr) {
            LOG_ERROR(TAG, "Failed to create queue");
            return false;
        }
    } else {
        // Очищаем очередь от старых событий при ре-инициализации
        xQueueReset(m_sensorQueue);
    }
    #else
    // В тестах используем фейковый указатель
    m_sensorQueue = (void*)1; 
    #endif
    
    return true;
}

// --- Запуск задачи ---
void AppLogic::startTask() {
    #if defined(ESP32_TARGET)
    // Запускаем задачу обработки логики с приоритетом 5
    // Stack size 4096 байт обычно достаточно для логики без тяжелых аллокаций
    xTaskCreate(appLogicTask, "appLogicTask", 4096, this, 5, nullptr);
    #endif
}

// --- Подписка на события ---
void AppLogic::subscribe(EventDispatcher* dispatcher) {
    if (dispatcher) {
        // Подписываемся только на сырые данные сенсоров
        dispatcher->subscribe(EventType::SENSOR_VALUE_CHANGED, this);
    }
}

// --- Обработчик входящих событий (IEventHandler) ---
void AppLogic::handleEvent(const Event& event) {
    // Этот метод вызывается в контексте задачи EventDispatcher (или ISR).
    // Его цель - максимально быстро передать данные в собственную задачу AppLogic.
    
    #if defined(NATIVE_TEST)
        // В тестах (синхронный режим) вызываем логику напрямую для детерминизма
        if (event.type == EventType::SENSOR_VALUE_CHANGED) {
             processSensorEvent(event);
        }
    #else
        // В RTOS отправляем событие в очередь для асинхронной обработки
        if (m_sensorQueue) {
            xQueueSend(m_sensorQueue, &event, 0);
        }
    #endif
}

// --- Приватные методы: Задача FreeRTOS ---

void AppLogic::appLogicTask(void* params) {
    // Статическая обертка для вызова метода экземпляра
    auto* instance = static_cast<AppLogic*>(params);
    instance->runAppLogicTask();
}

void AppLogic::runAppLogicTask() {
    #if defined(ESP32_TARGET)
    Event event(EventType::BLE_CONNECTED); // Временная переменная
    
    while(true) {
        // Блокирующее ожидание новых данных из очереди
        if (xQueueReceive(m_sensorQueue, &event, portMAX_DELAY) == pdPASS) {
            processSensorEvent(event);
        }
    }
    #endif
}

// --- Приватные методы: Бизнес-логика ---

/**
 * @brief Основная логика обработки значений сенсоров.
 * Здесь принимаются решения о смене состояния (OPEN/HALF/CLOSED) и жестах.
 */
void AppLogic::processSensorEvent(const Event& event) {
    // Мы обрабатываем только изменения значений сенсоров
    if (event.type != EventType::SENSOR_VALUE_CHANGED) return;

    int id = event.payload.sensorValue.id;
    int value = event.payload.sensorValue.value;

    // --- 1. Логика Сенсора Mute ---
    if (id == m_muteSensorId) {
        // Простая пороговая логика (Шмитт триггер здесь не помешал бы, но пока простой порог)
        bool newMuteState = (value > m_muteThreshold);
        
        if (newMuteState != m_isMuted) {
            m_isMuted = newMuteState;
            // Публикуем событие изменения состояния Mute
            Event ev(newMuteState ? EventType::MUTE_ENABLED : EventType::MUTE_DISABLED);
            m_dispatcher->postEvent(ev);
            
            #if defined(NATIVE_TEST)
            std::cout << "[AppLogic] Mute changed: " << newMuteState << std::endl;
            #endif
        }
        return; // Сенсор Mute обработан, это не игровое отверстие
    }

    // --- 2. Логика Игровых Сенсоров ---
    
    // Проверяем, входит ли ID сенсора в список игровых (hole_sensor_ids)
    // и находим его битовый индекс (0..7) для маски.
    int bitIndex = -1;
    for (size_t i = 0; i < m_holeSensorIds.size(); ++i) {
        if (m_holeSensorIds[i] == id) {
            bitIndex = i;
            break;
        }
    }

    // Если сенсор игровой и ID валиден
    if (bitIndex != -1 && id < 16) {
        SensorContext& ctx = m_sensorContexts[id]; 
        SensorState oldState = ctx.state;
        SensorState newState = oldState;

        // --- A. Сбор истории для Вибрато ---
        ctx.valueHistory.push_back(value);
        
        // Ограничиваем размер буфера истории (равен частоте дискретизации = 1 секунда данных)
        int maxHistorySize = m_configManager->getSampleRateHz(); 
        if (maxHistorySize <= 0) maxHistorySize = 50; // Защита от некорректного конфига

        if (ctx.valueHistory.size() > (size_t)maxHistorySize) {
            ctx.valueHistory.erase(ctx.valueHistory.begin()); // Удаляем старые данные
        }

        // --- B. Анализ Вибрато ---
        // Запускаем анализ, только если набрали достаточно данных (половина буфера)
        if (ctx.valueHistory.size() >= (size_t)(maxHistorySize / 2)) {
            float vibratoDepth = analyzeVibrato(ctx.valueHistory);
            
            if (vibratoDepth > 0.0f) {
                // Вибрато обнаружено -> Публикуем событие
                Event ev(EventType::VIBRATO_DETECTED, VibratoPayload{id, vibratoDepth});
                m_dispatcher->postEvent(ev);
                
                #if defined(NATIVE_TEST)
                // std::cout << "[AppLogic] Vibrato Detected! Depth: " << vibratoDepth << std::endl;
                #endif
            }
        }

        // --- C. Определение Состояния (3 ступени) ---
        // Сравниваем значение с порогами из конфига
        if (value > m_holeClosedThreshold) {
            newState = SensorState::CLOSED;
        } else if (value > m_halfHoleThreshold) {
            newState = SensorState::HALF_HOLE;
        } else {
            newState = SensorState::OPEN;
        }

        // --- D. Обработка изменения состояния ---
        if (newState != oldState) {
            ctx.state = newState;

            #if defined(NATIVE_TEST)
            std::cout << "[AppLogic] Sensor " << id << " state: " << (int)newState << std::endl;
            #endif

            // !!! ИСПРАВЛЕНИЕ ПОРЯДКА СОБЫТИЙ !!!
            // Сначала обновляем МАСКУ. Это сбросит старое состояние Half-Hole в AppFingering.
            // Затем отправляем НОВОЕ событие Half-Hole (если оно есть).
            // Если сделать наоборот, маска перезапишет (сбросит) только что установленный Half-Hole.
            
            // 1. Обновляем маску (в любом случае)
            updateMaskAndPublish();

            // 2. Если перешли в состояние ПОЛУЗАКРЫТИЯ -> Публикуем специальное событие
            if (newState == SensorState::HALF_HOLE) {
                Event ev(EventType::HALF_HOLE_DETECTED, HalfHolePayload{id});
                m_dispatcher->postEvent(ev);
            }
        }
    }
}

/**
 * @brief Собирает битовую маску из состояний всех сенсоров и публикует, если она изменилась.
 */
void AppLogic::updateMaskAndPublish() {
    uint8_t newMask = 0;
    
    // Проходим по всем игровым сенсорам в порядке, заданном в конфиге
    for (size_t i = 0; i < m_holeSensorIds.size(); ++i) {
        int id = m_holeSensorIds[i];
        
        // Проверка выхода за границы массива
        if (id >= 16) continue;

        // !!! КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ ЛОГИКИ !!!
        // Для формирования базовой маски мы считаем отверстие "ЗАКРЫТЫМ" (бит = 1),
        // если оно находится в состоянии CLOSED *ИЛИ* HALF_HOLE.
        //
        // Это необходимо, чтобы AppFingering мог сначала найти базовую ноту
        // (соответствующую закрытому отверстию), а затем применить к ней
        // модификатор "Half-Hole", если он активен.
        //
        // Если мы не включим HALF_HOLE в маску, AppFingering увидит "открытое" отверстие
        // и выберет другую ноту, игнорируя правило полузакрытия.
        if (m_sensorContexts[id].state == SensorState::CLOSED || 
            m_sensorContexts[id].state == SensorState::HALF_HOLE) {
            newMask |= (1 << i); 
        }
    }

    // Публикуем событие только если маска действительно изменилась
    if (newMask != m_currentMask) {
        m_currentMask = newMask;
        Event ev(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{newMask});
        m_dispatcher->postEvent(ev);
        
        #if defined(NATIVE_TEST)
        std::cout << "[AppLogic] Mask changed: " << (int)newMask << std::endl;
        #endif
    }
}

/**
 * @brief Алгоритм Zero-Crossing для детекции частоты вибрато.
 * Анализирует историю значений сенсора.
 */
float AppLogic::analyzeVibrato(const std::vector<int>& history) {
    if (history.empty()) return 0.0f;

    // 1. Находим Min/Max (Амплитуду сигнала)
    int minVal = 4096;
    int maxVal = 0;
    long sum = 0;

    for (int v : history) {
        if (v < minVal) minVal = v;
        if (v > maxVal) maxVal = v;
        sum += v;
    }

    int amplitude = maxVal - minVal;
    
    // Если амплитуда меньше порога (шум), считаем, что вибрато нет
    if (amplitude < m_vibratoAmplitudeMin) {
        return 0.0f;
    }

    // 2. Считаем среднее значение (DC offset)
    int avg = sum / history.size();

    // 3. Считаем количество пересечений среднего значения (Zero Crossings)
    int crossings = 0;
    bool above = (history[0] > avg);

    for (size_t i = 1; i < history.size(); ++i) {
        bool nowAbove = (history[i] > avg);
        // Если сигнал пересек среднюю линию
        if (nowAbove != above) {
            crossings++;
            above = nowAbove;
        }
    }

    // 4. Вычисляем частоту
    // Частота = (Количество пересечений / 2) / Длительность выборки
    // Длительность = Кол-во сэмплов / Частота дискретизации
    int sampleRate = m_configManager->getSampleRateHz();
    float durationSec = (float)history.size() / (float)sampleRate;
    
    if (durationSec < 0.1f) return 0.0f; // Защита от деления на ноль при малом буфере

    float freq = ((float)crossings / 2.0f) / durationSec;

    // 5. Проверяем, попадает ли частота в диапазон музыкального вибрато (2-6 Гц)
    if (freq >= m_vibratoFreqMin && freq <= m_vibratoFreqMax) {
        // Вибрато обнаружено!
        
        // Вычисляем нормализованную глубину (0.0 - 1.0) для MIDI Pitch Bend.
        // Используем эмпирический делитель 500.0 (макс. амплитуда).
        float depth = (float)amplitude / 500.0f;
        if (depth > 1.0f) depth = 1.0f;
        
        return depth;
    }

    return 0.0f;
}
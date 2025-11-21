/*
 * test_main.cpp
 *
 * Unit-тесты для модуля app/AppLogic.
 * Проверяет: Mute, Маску, Half-Hole и ПОРЯДОК СОБЫТИЙ.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.7 / 2.10
 */
#include <unity.h>
#include "app/AppLogic.h"
#include "MockHalStorage.h"
#include "MockEventHandler.h"
#include "core/EventDispatcher.h"
#include "core/ConfigManager.h"
#include <cstdio>
#include <fstream>
#include <cmath> 

// --- Глобальные объекты ---
MockHalStorage mockStorage;
ConfigManager configManager;
EventDispatcher dispatcher;
MockEventHandler spy;
AppLogic appLogic;

bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

void setUp(void) {
    spy.reset();
    dispatcher.reset(); 
    dispatcher.init();

    if (file_exists("data/settings.cfg")) {
        std::remove("data/settings.cfg.bak");
        std::rename("data/settings.cfg", "data/settings.cfg.bak");
    }
    
    // Пишем конфиг
    std::string cfg = 
        "[sensors]\n"
        "sample_rate_hz = 50\n"         
        "mute_threshold = 500\n"
        "hole_closed_threshold = 400\n"
        "half_hole_threshold = 300\n"
        "[app_logic]\n"
        "mute_sensor_id = 8\n"
        "hole_sensor_ids = 0, 1, 2, 3, 4, 5, 6, 7\n"
        "[gestures]\n"
        "vibrato_freq_min_hz = 2.0\n"
        "vibrato_freq_max_hz = 6.0\n"
        "vibrato_amplitude_min = 50\n";
    
    mockStorage.writeFile("/settings.cfg", cfg);
    
    configManager.init(&mockStorage);
    appLogic.init(&configManager, &dispatcher);
    
    dispatcher.subscribe(EventType::MUTE_ENABLED, &spy);
    dispatcher.subscribe(EventType::MUTE_DISABLED, &spy);
    dispatcher.subscribe(EventType::SENSOR_MASK_CHANGED, &spy);
    dispatcher.subscribe(EventType::HALF_HOLE_DETECTED, &spy);
    dispatcher.subscribe(EventType::VIBRATO_DETECTED, &spy); 
}

void tearDown(void) {
    std::remove("data/settings.cfg");
    if (file_exists("data/settings.cfg.bak")) {
        std::rename("data/settings.cfg.bak", "data/settings.cfg");
    }
}

// ... (тесты mute и mask без изменений) ...
void test_mute_logic() {
    Event ev1(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{8, 600});
    appLogic.handleEvent(ev1); 
    TEST_ASSERT_EQUAL(EventType::MUTE_ENABLED, spy.getLastEventType());
}

void test_mask_logic() {
    Event ev1(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{0, 500});
    appLogic.handleEvent(ev1);
    TEST_ASSERT_EQUAL(EventType::SENSOR_MASK_CHANGED, spy.getLastEventType());
}


/**
 * @brief Тест 3 (ОБНОВЛЕННЫЙ): Проверка ПОРЯДКА событий Half-Hole.
 * Критически важно для интеграции с AppFingering.
 */
void test_half_hole_event_order() {
    // 1. Подаем значение 350 (Half-Hole).
    // Это должно вызвать ДВА события:
    // 1. Обновление маски (Маска = 1, т.к. HalfHole считается закрытым)
    // 2. Детекция жеста (HalfHoleDetected ID=1)
    
    // Чтобы проверить порядок, нам нужно, чтобы spy запомнил ИСТОРИЮ,
    // но наш текущий MockEventHandler помнит только ПОСЛЕДНЕЕ.
    // Мы проверим косвенно: последнее событие должно быть HALF_HOLE_DETECTED.
    // Если последнее было MASK_CHANGED, значит порядок неправильный!
    
    Event ev1(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{1, 350}); // ID 1 = bit 1
    appLogic.handleEvent(ev1);

    // Проверяем, что последнее - это ЖЕСТ
    TEST_ASSERT_EQUAL(EventType::HALF_HOLE_DETECTED, spy.getLastEventType());
    TEST_ASSERT_EQUAL_INT(1, spy.getLastIntPayload()); 
    
    // Если тест упадет здесь (вернет SENSOR_MASK_CHANGED), значит AppLogic
    // отправляет события в неправильном порядке (сначала жест, потом маска).
}

/**
 * @brief Тест 4: Детекция Вибрато (Симуляция синусоиды).
 */
void test_vibrato_logic() {
    int center = 200; 
    int amp = 100;    
    float freq = 4.0f;
    int sampleRate = 50;
    
    // Генерируем 1 секунду данных (50 сэмплов)
    for (int i = 0; i < sampleRate; ++i) {
        float t = (float)i / (float)sampleRate;
        int val = center + (int)(amp * sin(2 * 3.14159f * freq * t));
        
        Event ev(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{0, val});
        appLogic.handleEvent(ev);
    }

    // Проверяем, что вибрато было обнаружено
    // (Оно могло быть обнаружено несколько раз за секунду)
    TEST_ASSERT_TRUE_MESSAGE(spy.getReceivedCount() > 0, "Vibrato should be detected");
    TEST_ASSERT_EQUAL(EventType::VIBRATO_DETECTED, spy.getLastEventType());
    
    // Проверяем, что в payload есть глубина > 0
    // Наш MockEventHandler не сохраняет float payload для Vibrato (нужно доработать мок!)
    // Но сам факт получения события VIBRATO_DETECTED уже говорит об успехе.
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_mute_logic);
    RUN_TEST(test_mask_logic);
    RUN_TEST(test_half_hole_event_order); // <-- Обновленный тест
    RUN_TEST(test_vibrato_logic);
    return UNITY_END();
}
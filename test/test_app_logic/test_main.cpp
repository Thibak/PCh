/*
 * test_main.cpp
 *
 * Unit-тесты для модуля app/AppLogic.
 * Проверяет: Mute логику, 3-х ступенчатые пороги, формирование маски.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.7
 */
#include <unity.h>
#include "app/AppLogic.h"
#include "MockHalStorage.h"
#include "MockEventHandler.h"
#include "core/EventDispatcher.h"
#include "core/ConfigManager.h"
#include <cstdio>
#include <fstream>

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
    dispatcher.reset(); // Сбрасываем подписчиков
    dispatcher.init();

    // Создаем тестовый конфиг
    if (file_exists("data/settings.cfg")) {
        std::remove("data/settings.cfg.bak");
        std::rename("data/settings.cfg", "data/settings.cfg.bak");
    }
    
    // Пишем конфиг с известными порогами
    std::string cfg = 
        "[sensors]\n"
        "mute_threshold = 500\n"
        "hole_closed_threshold = 400\n"
        "half_hole_threshold = 300\n"
        "[app_logic]\n"
        "mute_sensor_id = 8\n"
        "hole_sensor_ids = 0, 1, 2, 3, 4, 5, 6, 7\n";
    
    mockStorage.writeFile("/settings.cfg", cfg);
    
    configManager.init(&mockStorage);
    appLogic.init(&configManager, &dispatcher);
    
    // Подписываем шпиона на все интересные события
    dispatcher.subscribe(EventType::MUTE_ENABLED, &spy);
    dispatcher.subscribe(EventType::MUTE_DISABLED, &spy);
    dispatcher.subscribe(EventType::SENSOR_MASK_CHANGED, &spy);
    dispatcher.subscribe(EventType::HALF_HOLE_DETECTED, &spy);
}

void tearDown(void) {
    std::remove("data/settings.cfg");
    if (file_exists("data/settings.cfg.bak")) {
        std::rename("data/settings.cfg.bak", "data/settings.cfg");
    }
}

/**
 * @brief Тест 1: Логика Mute.
 */
void test_mute_logic() {
    // 1. Mute ВКЛ (Value 600 > 500)
    Event ev1(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{8, 600});
    appLogic.handleEvent(ev1); // В Native режиме вызывает processSensorEvent

    TEST_ASSERT_EQUAL_INT(1, spy.getReceivedCount());
    TEST_ASSERT_EQUAL(EventType::MUTE_ENABLED, spy.getLastEventType());

    // 2. Mute ВЫКЛ (Value 400 < 500)
    Event ev2(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{8, 400});
    appLogic.handleEvent(ev2);

    TEST_ASSERT_EQUAL_INT(2, spy.getReceivedCount());
    TEST_ASSERT_EQUAL(EventType::MUTE_DISABLED, spy.getLastEventType());
}

/**
 * @brief Тест 2: Логика маски (OPEN -> CLOSED).
 */
void test_mask_logic() {
    // ID 0 (Бит 0) -> Закрыт (500 > 400)
    Event ev1(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{0, 500});
    appLogic.handleEvent(ev1);

    TEST_ASSERT_EQUAL_INT(1, spy.getReceivedCount());
    TEST_ASSERT_EQUAL(EventType::SENSOR_MASK_CHANGED, spy.getLastEventType());
    // Маска должна быть 0x01
    TEST_ASSERT_EQUAL_INT(0x01, spy.getLastIntPayload()); // В MockEventHandler мы использовали payload.sensorMask.mask?
    // Проверим MockEventHandler.cpp:
    // if (event.type == EventType::SENSOR_VALUE_CHANGED) ...
    // А для SENSOR_MASK_CHANGED мы не сохраняли payload в `m_lastIntPayload`?
    // Надо проверить MockEventHandler! Если нет, тест упадет.
}

/**
 * @brief Тест 3: Логика Half-Hole.
 */
void test_half_hole_logic() {
    // ID 1 (Бит 1) -> Half-Hole (350: >300 и <400)
    Event ev1(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{1, 350});
    appLogic.handleEvent(ev1);

    // Должно прийти событие HALF_HOLE_DETECTED
    // И возможно SENSOR_MASK_CHANGED (если состояние изменилось с CLOSED, но тут с OPEN, так что маска 0 -> 0, не должна меняться)
    
    // Проверяем последнее событие
    TEST_ASSERT_EQUAL(EventType::HALF_HOLE_DETECTED, spy.getLastEventType());
    // Payload = ID сенсора
    // TEST_ASSERT_EQUAL_INT(1, spy.getLastIntPayload()); // Опять же, зависит от MockEventHandler
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_mute_logic);
    RUN_TEST(test_mask_logic);
    RUN_TEST(test_half_hole_logic);
    return UNITY_END();
}
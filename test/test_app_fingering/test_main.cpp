/*
 * test_main.cpp
 *
 * Unit-тесты для модуля app/AppFingering.
 * Проверяет парсинг таблицы и логику выбора нот.
 * Реализует безопасную работу с файловой системой (Backup/Restore).
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.6
 */
#include <unity.h>
#include "app/AppFingering.h"
#include "MockHalStorage.h"
#include "MockEventHandler.h" 
#include "core/EventDispatcher.h" 
#include <cstdio>  // rename, remove
#include <fstream> // ifstream

// --- Глобальные объекты ---
MockHalStorage mockStorage;
EventDispatcher dispatcher;
MockEventHandler spy; 
AppFingering appFingering;

// --- Вспомогательная функция ---
bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

// --- Setup / Teardown ---
void setUp(void) {
    // 1. Сброс состояния объектов
    spy.reset();
    
    // ОЧИСТКА ДИСПЕТЧЕРА (Важное исправление)
    dispatcher.reset(); 
    dispatcher.init();

    // 2. Безопасный бэкап существующего конфига (если есть)
    if (file_exists("data/fingering.cfg")) {
        std::remove("data/fingering.cfg.bak"); // Удаляем старый бэкап
        if (std::rename("data/fingering.cfg", "data/fingering.cfg.bak") != 0) {
            // Логируем ошибку, но не валим тест, если файла не было
        }
    }
}

void tearDown(void) {
    // 1. Удаляем файл, созданный тестом
    std::remove("data/fingering.cfg");

    // 2. Восстанавливаем пользовательский файл из бэкапа
    if (file_exists("data/fingering.cfg.bak")) {
        std::rename("data/fingering.cfg.bak", "data/fingering.cfg");
    }
}

/**
 * @brief Тест 1: Загрузка конфига и базовое соответствие маски ноте.
 */
void test_basic_mapping() {
    // 1. Подготовка конфига
    std::string cfg = 
        "0b11111111 60  # C4 (Все закрыты)\n"
        "0b11111110 62  # D4 (Нижний открыт)\n";
    mockStorage.writeFile("/fingering.cfg", cfg);

    // 2. Инициализация
    bool initRes = appFingering.init(&mockStorage);
    TEST_ASSERT_TRUE(initRes);

    // Подписываем шпиона
    appFingering.subscribe(&dispatcher);
    dispatcher.subscribe(EventType::NOTE_PITCH_SELECTED, &spy); 

    // 3. Эмулируем событие: Маска изменилась на 0xFF (Все закрыты)
    Event evMask1(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{0xFF});
    appFingering.handleEvent(evMask1); 

    // 4. Проверка
    TEST_ASSERT_EQUAL_INT(1, spy.getReceivedCount());
    TEST_ASSERT_EQUAL(EventType::NOTE_PITCH_SELECTED, spy.getLastEventType());
    TEST_ASSERT_EQUAL_INT(60, spy.getLastIntPayload()); 

    // 5. Эмулируем событие: Маска изменилась на 0xFE
    Event evMask2(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{0xFE});
    appFingering.handleEvent(evMask2);

    TEST_ASSERT_EQUAL_INT(2, spy.getReceivedCount());
    TEST_ASSERT_EQUAL_INT(62, spy.getLastIntPayload()); 
}

/**
 * @brief Тест 2: Логика "Полузакрытия" (Half-Hole).
 */
void test_half_hole_logic() {
    std::string cfg = "0b11111110 62 1 63";
    mockStorage.writeFile("/fingering.cfg", cfg);
    appFingering.init(&mockStorage);
    
    // Переподписываемся
    appFingering.subscribe(&dispatcher);
    dispatcher.subscribe(EventType::NOTE_PITCH_SELECTED, &spy);

    // 1. Устанавливаем маску 0xFE (D4)
    appFingering.handleEvent(Event(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{0xFE}));
    TEST_ASSERT_EQUAL_INT(62, spy.getLastIntPayload());

    // 2. Сигнализируем Half-Hole на сенсоре 1
    appFingering.handleEvent(Event(EventType::HALF_HOLE_DETECTED, HalfHolePayload{1}));
    
    // Ожидаем, что нота изменилась на 63 (D#4)
    TEST_ASSERT_EQUAL_INT(63, spy.getLastIntPayload());

    // 3. Смена маски сбрасывает Half-Hole (возврат к базовой ноте новой маски или той же)
    appFingering.handleEvent(Event(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{0xFE}));
    TEST_ASSERT_EQUAL_INT(62, spy.getLastIntPayload());
}

/**
 * @brief Тест 3: Дедупликация (не отправлять одну и ту же ноту подряд).
 */
void test_deduplication() {
    std::string cfg = "0b00000001 50";
    mockStorage.writeFile("/fingering.cfg", cfg);
    appFingering.init(&mockStorage);
    appFingering.subscribe(&dispatcher);
    dispatcher.subscribe(EventType::NOTE_PITCH_SELECTED, &spy);

    // Первый раз
    appFingering.handleEvent(Event(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{1}));
    int countAfterFirst = spy.getReceivedCount();

    // Второй раз (та же маска)
    appFingering.handleEvent(Event(EventType::SENSOR_MASK_CHANGED, SensorMaskPayload{1}));
    
    // Счетчик не должен измениться
    TEST_ASSERT_EQUAL_INT(countAfterFirst, spy.getReceivedCount());
}


int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_basic_mapping);
    RUN_TEST(test_half_hole_logic);
    RUN_TEST(test_deduplication);
    
    return UNITY_END();
}
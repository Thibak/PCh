/*
 * test_main.cpp
 *
 * Интеграционный тест полной сигнальной цепочки (Full Chain).
 * Проверяет прохождение сигнала от эмуляции сенсора до эмуляции BLE-выхода.
 *
 * Цепочка: MockSensor -> AppLogic -> AppFingering -> AppMidi -> MockHalBle
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.12
 */

#include <unity.h>
#include "core/Scheduler.h"
#include <cstdio>
#include <fstream>

// --- Подключаем Моки ---
#include "MockHalStorage.h"
#include "MockHalSystem.h"
#include "MockHalUsb.h"
#include "MockHalSensors.h"
#include "MockHalLed.h"
#include "MockHalBle.h"
#include "MockHalPower.h"

// --- Глобальные объекты ---
Application app;

MockHalStorage mockStorage;
MockHalSystem  mockSystem;
MockHalUsb     mockUsb;
MockHalSensors mockSensors;
MockHalLed     mockLed;
MockHalBle     mockBle;
MockHalPower   mockPower;

// --- Вспомогательные функции ---
bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

// --- Setup / Teardown ---
void setUp(void) {
    // 1. Сброс состояния моков
    mockBle.reset();
    mockLed.reset();
    // mockSensors.reset(); // (у sensors нет состояния, кроме конфига)
    
    // 2. Бэкап конфигов
    if (file_exists("data/settings.cfg")) std::rename("data/settings.cfg", "data/settings.cfg.bak");
    if (file_exists("data/fingering.cfg")) std::rename("data/fingering.cfg", "data/fingering.cfg.bak");

    // 3. Создание тестовых конфигов
    // Настраиваем 3 игровых сенсора (0, 1, 2) и Mute (8)
    std::string settings = 
        "[system]\n"
        "base_pitch_hz = 440.0\n"
        "[sensors]\n"
        "mute_threshold = 500\n"
        "hole_closed_threshold = 400\n"
        "half_hole_threshold = 300\n"
        "[app_logic]\n"
        "mute_sensor_id = 8\n"
        "hole_sensor_ids = 0, 1, 2\n"; // Только 3 сенсора для простоты
    mockStorage.writeFile("/settings.cfg", settings);

    // Настраиваем аппликатуру:
    // Маска 0 (все открыты) -> Нота 0 (Off)
    // Маска 1 (Сенсор 0 закрыт) -> Нота 60
    // Маска 3 (Сенсор 0 и 1 закрыты) -> Нота 62
    std::string fingering = 
        "0b00000000 0\n"
        "0b00000001 60\n"
        "0b00000011 62\n";
    mockStorage.writeFile("/fingering.cfg", fingering);

    // 4. Инициализация приложения
    // Важно: в Native режиме init() сразу же связывает все компоненты
    app.init(&mockStorage, &mockSystem, &mockUsb, &mockSensors, &mockLed, &mockBle, &mockPower);
    app.startTasks(&mockSensors, &mockLed, &mockBle, &mockPower);
}

void tearDown(void) {
    // Удаляем тестовые и восстанавливаем бэкапы
    std::remove("data/settings.cfg");
    std::remove("data/fingering.cfg");
    if (file_exists("data/settings.cfg.bak")) std::rename("data/settings.cfg.bak", "data/settings.cfg");
    if (file_exists("data/fingering.cfg.bak")) std::rename("data/fingering.cfg.bak", "data/fingering.cfg");
}

/**
 * @brief Тест 1: Цепочка "Нажатие -> Нота"
 */
void test_sensor_to_midi_chain() {
    // 1. Исходное состояние: все открыто (значение 0)
    // Эмулируем, что сенсоры прочитали 0
    mockSensors.pushMockSensorValue(0, 0);
    mockSensors.pushMockSensorValue(1, 0);
    mockSensors.pushMockSensorValue(2, 0);
    
    // Ожидаем, что ничего не произошло (или NoteOff, если было что-то)
    // В начале lastNoteOn = -1
    
    // 2. Нажимаем сенсор 0 (Закрываем отверстие)
    // Значение 500 > hole_closed_threshold (400)
    mockSensors.pushMockSensorValue(0, 500);
    
    // Цепочка событий:
    // MockSensor -> SENSOR_VALUE_CHANGED(0, 500)
    // -> AppLogic: Видит > 400 -> State CLOSED. Маска становится 0b001.
    // -> AppLogic -> SENSOR_MASK_CHANGED(1)
    // -> AppFingering: Видит маску 1 -> Ищет в конфиге -> Нота 60.
    // -> AppFingering -> NOTE_PITCH_SELECTED(60)
    // -> AppMidi: Видит ноту 60 -> sendNoteOn(60)
    
    // Проверка:
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOn());
    TEST_ASSERT_EQUAL_INT(1, mockLed.getBlinkOnceCount()); // LED должен моргнуть

    // 3. Нажимаем сенсор 1 (Теперь закрыты 0 и 1)
    mockSensors.pushMockSensorValue(1, 500);
    
    // Маска становится 0b011 (3). Нота 62.
    // AppMidi должен выключить 60 и включить 62.
    
    TEST_ASSERT_EQUAL_INT(62, mockBle.getLastNoteOn());
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOff());
}

/**
 * @brief Тест 2: Цепочка Mute
 */
void test_mute_chain() {
    // 1. Играем ноту 60
    mockSensors.pushMockSensorValue(0, 500);
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOn());

    // 2. Активируем сенсор Mute (ID 8)
    // Значение 600 > mute_threshold (500)
    mockSensors.pushMockSensorValue(8, 600);
    
    // Цепочка:
    // MockSensor -> SENSOR_VALUE -> AppLogic (Mute Detected) -> MUTE_ENABLED
    // -> AppMidi (Mute Enabled) -> sendNoteOff(60) + sendAllNotesOff()
    
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOff());
    TEST_ASSERT_EQUAL_INT(1, mockBle.getAllNotesOffCount());
    
    // 3. Пытаемся сменить ноту при включенном Mute
    mockSensors.pushMockSensorValue(1, 500); // Маска меняется -> Нота 62
    
    // Но AppMidi должен проигнорировать это, т.к. Mute активен.
    // LastNoteOn должен остаться старым (60) или не измениться на 62
    // Проще проверить LED: он не должен моргнуть второй раз
    TEST_ASSERT_EQUAL_INT(1, mockLed.getBlinkOnceCount());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_sensor_to_midi_chain);
    RUN_TEST(test_mute_chain);
    return UNITY_END();
}
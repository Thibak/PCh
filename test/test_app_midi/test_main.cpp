/*
 * test_main.cpp
 *
 * Unit-тесты для модуля app/AppMidi.
 * Проверяет: Генерацию MIDI событий, логику Mute, смену нот.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.8
 */
#include <unity.h>
#include "app/AppMidi.h"
#include "core/EventDispatcher.h"
#include "MockHalBle.h"
#include "MockHalLed.h"

// --- Глобальные объекты ---
EventDispatcher dispatcher;
MockHalBle mockBle;
MockHalLed mockLed;
AppMidi appMidi;

void setUp(void) {
    // 1. Сброс состояния МОКОВ (Критически важно!)
    mockBle.reset();
    mockLed.reset();

    // 2. Сброс диспетчера
    dispatcher.reset();
    dispatcher.init();

    // 3. Реинициализация AppMidi (сброс внутренних флагов)
    appMidi.init(&mockBle, &mockLed, 440.0f);
    appMidi.subscribe(&dispatcher);
}

void tearDown(void) {}

/**
 * @brief Тест 1: Обычное воспроизведение ноты.
 */
void test_play_note() {
    // Отправляем событие "Выбрана нота 60"
    Event ev(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{60});
    appMidi.handleEvent(ev);

    // Проверяем, что ушло в BLE
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOn());
    // Проверяем, что LED моргнул
    TEST_ASSERT_EQUAL_INT(1, mockLed.getBlinkOnceCount());
}

/**
 * @brief Тест 2: Смена ноты (должен быть Off старой, потом On новой).
 */
void test_change_note() {
    // 1. Играем 60
    appMidi.handleEvent(Event(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{60}));
    
    // 2. Играем 62
    appMidi.handleEvent(Event(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{62}));

    // Проверяем последнее состояние
    TEST_ASSERT_EQUAL_INT(62, mockBle.getLastNoteOn()); // Последний On был 62
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOff()); // Последний Off был 60
    // Ожидаем 2 моргания (по 1 на каждую ноту) - теперь счетчик сброшен в setUp!
    TEST_ASSERT_EQUAL_INT(2, mockLed.getBlinkOnceCount()); 
}

/**
 * @brief Тест 3: Пауза (Note 0).
 */
void test_pause() {
    // 1. Играем 60
    appMidi.handleEvent(Event(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{60}));
    
    // 2. Пауза (0)
    appMidi.handleEvent(Event(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{0}));

    // Должен уйти Off 60, но не должно быть нового On
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOff());
    // getLastNoteOn все еще 60 (с прошлого раза, так как переменная хранит последнее значение)
    TEST_ASSERT_EQUAL_INT(60, mockBle.getLastNoteOn());
    
    // НО! Количество Blink должно быть 1 (только для первой ноты 60)
    TEST_ASSERT_EQUAL_INT(1, mockLed.getBlinkOnceCount()); 
}

/**
 * @brief Тест 4: Логика Mute.
 */
void test_mute_logic() {
    // 1. Играем ноту 70
    appMidi.handleEvent(Event(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{70}));
    TEST_ASSERT_EQUAL_INT(70, mockBle.getLastNoteOn());

    // 2. Включаем Mute
    appMidi.handleEvent(Event(EventType::MUTE_ENABLED));

    // Должен уйти Off 70 и AllNotesOff
    TEST_ASSERT_EQUAL_INT(70, mockBle.getLastNoteOff());
    TEST_ASSERT_EQUAL_INT(1, mockBle.getAllNotesOffCount());

    // 3. Пытаемся сыграть ноту 72 при включенном Mute
    appMidi.handleEvent(Event(EventType::NOTE_PITCH_SELECTED, NotePitchPayload{72}));

    // NoteOn НЕ должен измениться (остаться старым 70)
    TEST_ASSERT_EQUAL_INT(70, mockBle.getLastNoteOn());
    
    // LED не должен моргнуть второй раз
    TEST_ASSERT_EQUAL_INT(1, mockLed.getBlinkOnceCount());
}

/**
 * @brief Тест 5: Вибрато (Pitch Bend).
 */
void test_vibrato() {
    // Отправляем вибрато
    Event ev(EventType::VIBRATO_DETECTED, VibratoPayload{0, 0.8f});
    appMidi.handleEvent(ev);

    TEST_ASSERT_EQUAL_FLOAT(0.8f, mockBle.getLastPitchBend());

    // Проверяем, что Mute блокирует вибрато
    appMidi.handleEvent(Event(EventType::MUTE_ENABLED));
    
    Event ev2(EventType::VIBRATO_DETECTED, VibratoPayload{0, 0.2f});
    appMidi.handleEvent(ev2);
    
    // Значение должно остаться старым (0.8), т.к. новое (0.2) проигнорировано
    TEST_ASSERT_EQUAL_FLOAT(0.8f, mockBle.getLastPitchBend());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_play_note);
    RUN_TEST(test_change_note);
    RUN_TEST(test_pause);
    RUN_TEST(test_mute_logic);
    RUN_TEST(test_vibrato);
    return UNITY_END();
}
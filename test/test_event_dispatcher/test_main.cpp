/*
 * test_main.cpp
 *
 * Unit-тесты для модуля core/EventDispatcher.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.7
 */

#include <unity.h>
#include "core/EventDispatcher.h"
#include "MockEventHandler.h" // Наш новый универсальный мок

// --- Глобальные объекты ---
EventDispatcher dispatcher;
MockEventHandler handler1;
MockEventHandler handler2;

// --- Setup / Teardown (Без extern "C") ---
void setUp(void) {
    // Сбрасываем состояние моков перед каждым тестом
    handler1.reset();
    handler2.reset();
    
    // Важно: dispatcher хранит указатели на handler1/2.
    // В unit-тестах мы не пересоздаем dispatcher, поэтому подписки сохраняются.
    // Это нормально для простых тестов, но в идеале dispatcher надо пересоздавать.
    // Для этого теста мы будем просто проверять инкрементальные изменения.
}

void tearDown(void) {}

// --- Тесты ---

/**
 * @brief Тест 1: Подписка и получение простого события.
 */
void test_subscribe_and_receive() {
    // 1. Подписываем handler1 на BLE_CONNECTED
    dispatcher.subscribe(EventType::BLE_CONNECTED, &handler1);
    
    // 2. Публикуем событие
    Event ev(EventType::BLE_CONNECTED);
    dispatcher.postEvent(ev);
    
    // 3. Проверяем
    TEST_ASSERT_EQUAL_INT(1, handler1.getReceivedCount());
    // Приведение типа для enum class нужно для корректного сравнения в Unity, 
    // либо перегрузка оператора, но int проще.
    TEST_ASSERT_EQUAL(EventType::BLE_CONNECTED, handler1.getLastEventType());
}

/**
 * @brief Тест 2: Игнорирование событий, на которые не подписаны.
 */
void test_ignore_unsubscribed() {
    // handler1 подписан только на BLE_CONNECTED (из прошлого теста)
    
    // Публикуем ДРУГОЕ событие
    Event ev(EventType::MUTE_ENABLED);
    dispatcher.postEvent(ev);
    
    // handler1 не должен был его получить (счетчик не увеличился)
    // (Счетчик был сброшен в setUp)
    TEST_ASSERT_EQUAL_INT(0, handler1.getReceivedCount());
}

/**
 * @brief Тест 3: Множественные подписчики (Multicast) и передача данных.
 */
void test_multiple_subscribers() {
    // Подписываем ОБОИХ на SENSOR_VALUE_CHANGED
    dispatcher.subscribe(EventType::SENSOR_VALUE_CHANGED, &handler1);
    dispatcher.subscribe(EventType::SENSOR_VALUE_CHANGED, &handler2);
    
    // Публикуем событие с данными
    Event ev(EventType::SENSOR_VALUE_CHANGED, SensorValuePayload{1, 123});
    dispatcher.postEvent(ev);
    
    // Проверяем, что оба получили
    TEST_ASSERT_EQUAL_INT(1, handler1.getReceivedCount());
    TEST_ASSERT_EQUAL_INT(123, handler1.getLastIntPayload());
    
    TEST_ASSERT_EQUAL_INT(1, handler2.getReceivedCount());
    TEST_ASSERT_EQUAL_INT(123, handler2.getLastIntPayload());
}

// --- Main ---

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Инициализация диспетчера
    dispatcher.init();

    RUN_TEST(test_subscribe_and_receive);
    RUN_TEST(test_ignore_unsubscribed);
    RUN_TEST(test_multiple_subscribers);
    
    return UNITY_END();
}
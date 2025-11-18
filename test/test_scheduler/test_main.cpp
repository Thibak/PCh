/*
 * test_main.cpp
 *
 * Интеграционный тест для core/Scheduler.
 * Проверяет, что Application корректно инициализирует все подсистемы.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.3
 */

#include <unity.h>
#include "core/Scheduler.h"

// Подключаем Моки (из библиотеки lib/mocks)
// PlatformIO автоматически найдет их, т.к. они в lib/
#include "MockHalStorage.h"
#include "MockHalSystem.h"
#include "MockHalUsb.h"
#include "MockHalSensors.h"
#include "MockHalLed.h"
#include "MockHalBle.h"
#include "MockHalPower.h"

// --- Глобальные объекты для теста ---

// Тестируемый объект (System Under Test)
Application app;

// Mock-объекты (Заглушки)
MockHalStorage mockStorage;
MockHalSystem  mockSystem;
MockHalUsb     mockUsb;
MockHalSensors mockSensors;
MockHalLed     mockLed;
MockHalBle     mockBle;
MockHalPower   mockPower;

void setUp(void) {
    // Сброс состояния (если необходимо) перед каждым тестом
}

void tearDown(void) {}

/**
 * @brief Тест: Успешная инициализация и запуск
 * * Этот тест проверяет, что метод init() может быть вызван
 * со всеми зависимостями, и что startTasks() отрабатывает без падений.
 * Это доказывает, что архитектура "склеена" правильно.
 */
void test_scheduler_init_and_start(void) {
    TEST_MESSAGE("Step 1: Initializing Application with Mocks...");
    
    // 1. Внедрение зависимостей
    app.init(
        &mockStorage,
        &mockSystem,
        &mockUsb,
        &mockSensors,
        &mockLed,
        &mockBle,
        &mockPower
    );

    // Проверяем (косвенно через моки), что инициализация затронула HAL
    // Например, MockHalUsb должен был получить вызов init
    TEST_ASSERT_TRUE_MESSAGE(mockUsb.wasInitCalledWithStorage(), "HalUsb should be initialized with Storage");
    
    TEST_MESSAGE("Step 2: Starting Tasks...");

    // 2. Запуск задач
    app.startTasks(
        &mockSensors,
        &mockLed,
        &mockBle,
        &mockPower
    );
    
    // Если мы дошли до этой строки и не упали (SegFault), тест пройден.
    TEST_MESSAGE("Scheduler Init & Start Succeeded.");
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_scheduler_init_and_start);
    
    return UNITY_END();
}
/*
 * test_main.cpp
 *
 * Главная точка входа для тестов [env:native].
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.3
 */

#include <unity.h>

// 1. Включаем "клей", который мы тестируем
#include "core/Scheduler.h"

// 2. Включаем Моки (теперь это библиотека, пути простые)
#include "MockHalStorage.h"
#include "MockHalSystem.h"
#include "MockHalUsb.h"
#include "MockHalSensors.h"
#include "MockHalLed.h"
#include "MockHalBle.h"
#include "MockHalPower.h"

// --- Глобальные объекты для тестов ---

// Создаем "клей" (System Under Test)
Application app;

// Создаем *экземпляры* всех наших Mock-объектов
MockHalStorage mockStorage;
MockHalSystem  mockSystem;
MockHalUsb     mockUsb;
MockHalSensors mockSensors;
MockHalLed     mockLed;
MockHalBle     mockBle;
MockHalPower   mockPower;

// --- Функции Setup / Teardown (стандарт Unity) ---

void setUp(void) {
    // (Этот код выполняется *перед* каждым тестом)
    // (Пока нам здесь ничего не нужно)
}

void tearDown(void) {
    // (Этот код выполняется *после* каждого теста)
}

// --- Тест Спринта 2.3 ---

/**
 * @brief Интеграционный тест для Scheduler.
 * * Проверяет, что вся наша "чистая" архитектура (CORE/APP)
 * может быть инициализирована со всеми Mock-объектами (HAL)
 * и не падает.
 */

void test_scheduler_init_and_start(void) {
    TEST_MESSAGE("Running Integration Test: Scheduler Init & Start");

    // Просто базовая проверка, что наши моки живы (не null)
    TEST_ASSERT_NOT_NULL(&mockStorage);
    TEST_ASSERT_NOT_NULL(&mockSystem);
    TEST_ASSERT_NOT_NULL(&mockUsb);
    TEST_ASSERT_NOT_NULL(&mockSensors);
    TEST_ASSERT_NOT_NULL(&mockLed);
    TEST_ASSERT_NOT_NULL(&mockBle);
    TEST_ASSERT_NOT_NULL(&mockPower);

    // 1. ВЫЗЫВАЕМ INIT (Спринт 0.6 - Фаза 1-5)
    // Если любой include/контракт/зависимость нарушены,
    // этот вызов не скомпилируется или упадет.
    app.init(
        &mockStorage,
        &mockSystem,
        &mockUsb,
        &mockSensors,
        &mockLed,
        &mockBle,
        &mockPower
    );

    // 2. ВЫЗЫВАЕМ STARTTASKS (Спринт 0.6 - Фаза 6)
    // Проверяет, что все методы startTask() существуют
    // на mock-объектах.
    app.startTasks(
        &mockSensors,
        &mockLed,
        &mockBle,
        &mockPower
    );
    
    TEST_MESSAGE("Scheduler Init & Start Succeeded.");
}

/**
 * @brief Главная точка входа для `pio test -e native`
 */
int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Запускаем наш первый тест
    RUN_TEST(test_scheduler_init_and_start);
    
    UNITY_END();
    return 0;
}
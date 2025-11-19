/*
 * test_main.cpp
 *
 * Unit-тесты для модуля core/Logger.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.5
 */

#include <unity.h>
#include <string>

#include "core/Logger.h"
#include "core/ConfigManager.h"

// Моки
#include "MockHalUsb.h"
#include "MockHalSystem.h"
#include "MockHalStorage.h" // Нужен, так как ConfigManager зависит от IHalStorage в init

// --- Глобальные объекты для теста ---
ConfigManager configManager;
MockHalStorage mockStorage; // Заглушка, не читающая реальные файлы
MockHalUsb mockUsb;
MockHalSystem mockSystem;

// --- Setup / Teardown ---
void setUp(void) {
    // Сбрасываем состояние перед каждым тестом
    
    // 1. Сбрасываем конфиг в дефолт
    // (ConfigManager не имеет метода reset, но мы можем просто пересоздать его
    // или переинициализировать. Здесь проще всего переинициализировать через сеттеры,
    // или просто полагаться на то, что мы выставим нужные значения в тесте).
}

void tearDown(void) {
}

/**
 * @brief Тест 1: Проверка фильтрации уровней (INFO vs DEBUG)
 */
void test_log_level_filtering() {
    // 1. Настраиваем уровень INFO (в памяти, без записи на диск)
    configManager.setLogLevel(LogLevel::INFO);
    
    // 2. Инициализируем Logger (он кэширует уровень из configManager)
    Logger::getInstance()->init(&configManager, &mockUsb, &mockSystem);
    
    // Получаем текущее количество вызовов serialPrint
    int startCount = mockUsb.getSerialPrintCount();

    // 3. Пишем DEBUG лог (должен быть проигнорирован, т.к. DEBUG < INFO)
    LOG_DEBUG("TEST", "This is debug");
    
    TEST_ASSERT_EQUAL_INT(startCount, mockUsb.getSerialPrintCount()); 

    // 4. Пишем INFO лог (должен пройти)
    LOG_INFO("TEST", "This is info");
    
    TEST_ASSERT_EQUAL_INT(startCount + 1, mockUsb.getSerialPrintCount()); 
}

/**
 * @brief Тест 2: Проверка динамической смены уровня
 */
void test_log_level_change_runtime() {
    // 1. Сначала уровень ERROR (очень строгий)
    configManager.setLogLevel(LogLevel::ERROR);
    Logger::getInstance()->init(&configManager, &mockUsb, &mockSystem);
    
    int startCount = mockUsb.getSerialPrintCount();
    
    LOG_WARN("TEST", "Warning ignored");
    TEST_ASSERT_EQUAL_INT(startCount, mockUsb.getSerialPrintCount()); 

    // 2. Меняем уровень на WARN и переинициализируем логгер
    configManager.setLogLevel(LogLevel::WARN);
    Logger::getInstance()->init(&configManager, &mockUsb, &mockSystem); // Обновляет кэш уровня
    
    LOG_WARN("TEST", "Warning passed");
    TEST_ASSERT_EQUAL_INT(startCount + 1, mockUsb.getSerialPrintCount()); 
}

/**
 * @brief Тест 3: Проверка формата сообщения
 * Ожидаем: "TIMESTAMP - TAG - LEVEL - MESSAGE"
 */
void test_log_formatting() {
    configManager.setLogLevel(LogLevel::DEBUG);
    Logger::getInstance()->init(&configManager, &mockUsb, &mockSystem);
    
    // Пишем лог
    LOG_WARN("MY_TAG", "Hello %d", 123);
    
    std::string output = mockUsb.getLastSerialLine();
    // output должен выглядеть как "1234 - MY_TAG - WARN - Hello 123" (время зависит от мока)
    
    // Проверяем наличие подстрок
    TEST_ASSERT_TRUE_MESSAGE(output.find("MY_TAG") != std::string::npos, "Tag missing");
    TEST_ASSERT_TRUE_MESSAGE(output.find("WARN") != std::string::npos, "Level missing");
    TEST_ASSERT_TRUE_MESSAGE(output.find("Hello 123") != std::string::npos, "Message missing or formatted wrong");
    
    // Проверяем наличие цифр в начале (timestamp)
    TEST_ASSERT_TRUE_MESSAGE(isdigit(output[0]), "Timestamp missing");
}


int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    RUN_TEST(test_log_level_filtering);
    RUN_TEST(test_log_level_change_runtime);
    RUN_TEST(test_log_formatting);
    
    return UNITY_END();
}
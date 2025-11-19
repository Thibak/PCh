/*
 * test_main.cpp
 *
 * Unit-тесты для модуля core/ConfigManager.
 * Тестирует парсинг INI-файлов, обработку дефолтных значений и векторов.
 *
 * Соответствует: DEVELOPMENT_PLAN.MD - Спринт 2.5
 */

#include <unity.h>
#include <iostream>
#include <string>
#include <cstdio>  // rename, remove
#include <fstream> // ifstream

// Подключаем тестируемый класс
#include "core/ConfigManager.h"
// Подключаем необходимый мок
#include "MockHalStorage.h"

// --- Вспомогательная функция ---
bool file_exists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

// --- ОБЯЗАТЕЛЬНЫЕ ФУНКЦИИ UNITY ---

    void setUp(void) {
        // Если реальный файл существует, делаем его бэкап
        if (file_exists("data/settings.cfg")) {
            // Удаляем старый бэкап, если он вдруг остался (защита от сбоев)
            std::remove("data/settings.cfg.bak");
            if (std::rename("data/settings.cfg", "data/settings.cfg.bak") != 0) {
                std::cerr << "[TEST SETUP] Failed to backup settings.cfg!" << std::endl;
            }
        }
    }

    void tearDown(void) {
        // 1. Удаляем файл, который был создан тестом (это "мусор")
        std::remove("data/settings.cfg");

        // 2. Восстанавливаем оригинальный файл из бэкапа
        if (file_exists("data/settings.cfg.bak")) {
            if (std::rename("data/settings.cfg.bak", "data/settings.cfg") != 0) {
                std::cerr << "[TEST TEARDOWN] Failed to restore settings.cfg!" << std::endl;
            }
        }
    }


// --- Тестовые функции ---

/**
 * @brief Тест 1: Проверка загрузки значений по умолчанию, если файл не найден.
 */
void test_config_defaults_when_file_missing() {
    // 1. Подготовка
    MockHalStorage mockStorage; 
    ConfigManager config;

    // ВКЛЮЧАЕМ симуляцию ошибки чтения (файл как бы не найден)
    mockStorage.setSimulateReadError(true);

    // 2. Действие
    bool success = config.init(&mockStorage);
    
    // 3. Проверка
    TEST_ASSERT_FALSE_MESSAGE(success, "Init should return false when file missing (simulated)");
    
    // Проверяем дефолтные значения
    TEST_ASSERT_EQUAL(LogLevel::WARN, config.getLogLevel());
    TEST_ASSERT_EQUAL_FLOAT(440.0f, config.getBasePitchHz());
    TEST_ASSERT_EQUAL_FLOAT(0.1f, config.getFilterAlpha());
    TEST_ASSERT_EQUAL(500, config.getMuteThreshold());
}

/**
 * @brief Тест 2: Парсинг валидного файла с базовыми типами.
 */
void test_config_parse_valid_basic() {
    MockHalStorage mockStorage; 
    ConfigManager config;

    // Создаем тестовый конфиг
    std::string cfg = 
        "[system]\n"
        "log_level = DEBUG\n"         
        "base_pitch_hz = 415.0\n"     
        "led_pin = GPIO_TEST\n"       
        "[sensors]\n"
        "hole_closed_threshold = 800\n"
        "blink_duration_ms = 100\n"; 

    // Это создаст файл "data/settings.cfg" (но настоящий уже в бэкапе)
    mockStorage.writeFile("/settings.cfg", cfg);

    // Инициализация
    bool success = config.init(&mockStorage);
    TEST_ASSERT_TRUE_MESSAGE(success, "Init should return true when file exists");

    // Проверка значений
    TEST_ASSERT_EQUAL(LogLevel::DEBUG, config.getLogLevel());
    TEST_ASSERT_EQUAL_FLOAT(415.0f, config.getBasePitchHz());
    TEST_ASSERT_EQUAL_STRING("GPIO_TEST", config.getLedPin().c_str());
    TEST_ASSERT_EQUAL(800, config.getHoleClosedThreshold());
    TEST_ASSERT_EQUAL(100, config.getLedBlinkDurationMs()); 
}

/**
 * @brief Тест 3: Парсинг векторов (списков значений).
 */
void test_config_parse_vectors() {
    MockHalStorage mockStorage; 
    ConfigManager config;

    std::string cfg = 
        "[sensors]\n"
        "physical_pins = P_ONE, P_TWO, P_THREE\n"
        "[app_logic]\n"
        "hole_sensor_ids = 10, 20, 30, 40";     

    mockStorage.writeFile("/settings.cfg", cfg);
    config.init(&mockStorage);

    // Проверка вектора строк
    const auto& pins = config.getPhysicalPins();
    TEST_ASSERT_EQUAL(3, pins.size());
    if (pins.size() >= 3) {
        TEST_ASSERT_EQUAL_STRING("P_ONE", pins[0].c_str());
        TEST_ASSERT_EQUAL_STRING("P_TWO", pins[1].c_str());
    }
    
    // Проверка вектора чисел
    const auto& ids = config.getHoleSensorIds();
    TEST_ASSERT_EQUAL(4, ids.size());
    if (ids.size() >= 4) {
        TEST_ASSERT_EQUAL(10, ids[0]);
        TEST_ASSERT_EQUAL(40, ids[3]);
    }
}

/**
 * @brief Тест 4: Устойчивость к мусору.
 */
void test_config_parse_robustness() {
    MockHalStorage mockStorage; 
    ConfigManager config;

    std::string cfg = 
        "\n"                 
        "# Это комментарий\n"
        "   base_pitch_hz   =   432.0   # Комментарий в конце строки \n"
        "invalid_key = 123\n" 
        "mute_threshold = abc\n"; 

    mockStorage.writeFile("/settings.cfg", cfg);
    config.init(&mockStorage);

    TEST_ASSERT_EQUAL_FLOAT(432.0f, config.getBasePitchHz());
    TEST_ASSERT_EQUAL(500, config.getMuteThreshold());
}


// --- Точка входа (Main) ---

int main(int argc, char **argv) {
    std::cout << "\n--- Starting ConfigManager Unit Tests ---\n";
    UNITY_BEGIN();
    
    RUN_TEST(test_config_defaults_when_file_missing);
    RUN_TEST(test_config_parse_valid_basic);
    RUN_TEST(test_config_parse_vectors);
    RUN_TEST(test_config_parse_robustness);
    
    return UNITY_END();
}
/*
 * Главная точка входа (Entry Point)
 *
 * Этот файл использует условную компиляцию для разделения
 * "Host" (native) и "Target" (esp32) логики. 
 */

// 1. ЛОГИКА ДЛЯ "TARGET" (ESP32)
// ----------------------------------------------------
#if defined(ESP32_TARGET)

#include <Arduino.h>
#include "core/Scheduler.h"
#include "hal/HalStorage.h"
#include "hal/HalSystem.h"
#include "hal/HalUsb.h"
#include "hal/HalSensors.h"
#include "hal/HalLed.h"
#include "hal/HalBle.h"
#include "hal/HalPower.h"

// Создаем "клей" - главный класс Application
Application app;

// Создаем *реальные* "железные" HAL-объекты
HalStorage g_halStorage;
HalSystem  g_halSystem;
HalUsb     g_halUsb;
HalSensors g_halSensors;
HalLed     g_halLed;
HalBle     g_halBle;
HalPower   g_halPower;

void setup() {
    // Ждем запуска Serial (для логов)
    Serial.begin(115200);
    delay(1000);

    // 1. Инициализируем модули
    app.init(
        &g_halStorage,
        &g_halSystem,
        &g_halUsb,
        &g_halSensors,
        &g_halLed,
        &g_halBle,
        &g_halPower
    );

    // 2. Запускаем задачи FreeRTOS
    app.startTasks(
        &g_halSensors,
        &g_halLed,
        &g_halBle,
        &g_halPower
    );
    
    // (FreeRTOS vTaskStartScheduler() вызывается неявно ядром Arduino)
}

void loop() {
    // В нашей архитектуре loop() не используется.
    // Вся работа происходит в задачах FreeRTOS.
    vTaskDelay(pdMS_TO_TICKS(1000));
}

// 2. ЛОГИКА ДЛЯ "HOST" (NATIVE / PC)
// ----------------------------------------------------
#elif defined(NATIVE_TEST)

#include <iostream>
// (Здесь мы НЕ включаем Arduino.h)

/*
 * В [env:native] этот main.cpp не используется.
 * Вместо него точкой входа является test/test_main.cpp,
 * который запускает PlatformIO Unit Test runner.
 *
 * Оставим этот файл минимальным, чтобы CI-сборка (pio run -e native)
 * проходила успешно.
 */

#ifndef PIO_UNIT_TESTING
int main(int argc, char** argv) {
    std::cout << "Building for native... (main.cpp)" << std::endl;
    std::cout << "To run tests, use 'pio test -e native'" << std::endl;
    return 0;
}
#endif

#endif // ESP32_TARGET / NATIVE_TEST
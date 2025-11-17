/*
 * LogLevel.h
 *
 * Определяет Enum для уровней логирования.
 *
 * Соответствует: docs/CONFIG_SCHEMA.md
 */
#pragma once

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERROR = 3,
    NONE = 4
};
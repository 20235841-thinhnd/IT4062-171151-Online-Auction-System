#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

/* Open the log file in append mode. Returns false if it cannot be opened. */
bool logger_init(const char *file_path);

/* Write a timestamped message to the log file and console. */
void logger_log(LogLevel level, const char *format, ...);

/* Flush and close the log file. Safe to call more than once. */
void logger_close(void);

#endif /* LOGGER_H */

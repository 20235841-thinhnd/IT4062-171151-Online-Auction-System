#define _POSIX_C_SOURCE 200809L

#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static FILE *log_file;

static const char *level_name(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

bool logger_init(const char *file_path) {
    if (file_path == NULL || file_path[0] == '\0') {
        return false;
    }

    FILE *new_file = fopen(file_path, "a");
    if (new_file == NULL) {
        return false;
    }

    if (log_file != NULL) {
        fclose(log_file);
    }
    log_file = new_file;
    return true;
}

void logger_log(LogLevel level, const char *format, ...) {
    if (format == NULL) {
        return;
    }

    time_t now = time(NULL);
    struct tm local_time;
    char timestamp[20] = "0000-00-00 00:00:00";
    if (now != (time_t)-1 && localtime_r(&now, &local_time) != NULL) {
        (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local_time);
    }

    FILE *console = (level >= LOG_LEVEL_WARN) ? stderr : stdout;
    va_list args;
    va_start(args, format);
    fprintf(console, "[%s] [%s] ", timestamp, level_name(level));
    vfprintf(console, format, args);
    fputc('\n', console);
    fflush(console);
    va_end(args);

    if (log_file != NULL) {
        va_start(args, format);
        fprintf(log_file, "[%s] [%s] ", timestamp, level_name(level));
        vfprintf(log_file, format, args);
        fputc('\n', log_file);
        fflush(log_file);
        va_end(args);
    }
}

void logger_close(void) {
    if (log_file != NULL) {
        fflush(log_file);
        fclose(log_file);
        log_file = NULL;
    }
}

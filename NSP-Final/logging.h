#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <time.h>
#include <stdarg.h>

#define LEVEL_DEBUG   0
#define LEVEL_INFO    1
#define LEVEL_WARN    2
#define LEVEL_ERROR   3

#ifndef MIN_LOG_LEVEL
#define MIN_LOG_LEVEL LEVEL_INFO
#endif

void log_init(const char *filename);
void log_close(void);
void log_message(int level, const char *file, int line, const char *fmt, ...);

#define LOG_DEBUG(...) do { if (LEVEL_DEBUG >= MIN_LOG_LEVEL) log_message(LEVEL_DEBUG, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define LOG_INFO(...)  do { if (LEVEL_INFO >= MIN_LOG_LEVEL) log_message(LEVEL_INFO, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define LOG_WARN(...)  do { if (LEVEL_WARN >= MIN_LOG_LEVEL) log_message(LEVEL_WARN, __FILE__, __LINE__, __VA_ARGS__); } while(0)
#define LOG_ERROR(...) do { if (LEVEL_ERROR >= MIN_LOG_LEVEL) log_message(LEVEL_ERROR, __FILE__, __LINE__, __VA_ARGS__); } while(0)

#endif

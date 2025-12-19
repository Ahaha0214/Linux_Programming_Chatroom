#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *level_strings[] = { "DEBUG", "INFO", "WARN", "ERROR" };
static const char *level_colors[] = { "\033[36m", "\033[32m", "\033[33m", "\033[31m" };
#define COLOR_RESET "\033[0m"

void log_init(const char *filename) {
    pthread_mutex_lock(&log_lock);
    if (filename != NULL) {
        log_file = fopen(filename, "a");
    }
    pthread_mutex_unlock(&log_lock);
}

void log_close(void) {
    pthread_mutex_lock(&log_lock);
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_lock);
}

void log_message(int level, const char *file, int line, const char *fmt, ...) {
    // ... 格式化輸出到 stderr (彩色) 和 log_file ...
}

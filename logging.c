#include "logging.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR"
};

static const char *level_colors[] = {
    "\033[36m",  // Cyan for DEBUG
    "\033[32m",  // Green for INFO
    "\033[33m",  // Yellow for WARN
    "\033[31m"   // Red for ERROR
};

#define COLOR_RESET "\033[0m"

void log_init(const char *filename) {
    pthread_mutex_lock(&log_lock);
    if (filename != NULL) {
        log_file = fopen(filename, "a");
        if (log_file == NULL) {
            fprintf(stderr, "Warning: Could not open log file %s\n", filename);
        }
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
    if (level < 0 || level > LEVEL_ERROR) level = LEVEL_INFO;
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[26];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Extract just the filename from path
    const char *basename = strrchr(file, '/');
    if (basename) basename++;
    else basename = file;
    
    pthread_mutex_lock(&log_lock);
    
    // Print to stderr with colors
    fprintf(stderr, "%s[%s] %s (%s:%d): ",
            level_colors[level], level_strings[level], time_buf, basename, line);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "%s\n", COLOR_RESET);
    
    // Also write to log file if open (without colors)
    if (log_file != NULL) {
        fprintf(log_file, "[%s] %s (%s:%d): ", level_strings[level], time_buf, basename, line);
        
        va_start(args, fmt);
        vfprintf(log_file, fmt, args);
        va_end(args);
        
        fprintf(log_file, "\n");
        fflush(log_file);
    }
    
    pthread_mutex_unlock(&log_lock);
}

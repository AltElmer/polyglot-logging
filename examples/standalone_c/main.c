#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/**
 * @file main.c
 * @brief Zero-dependency C99 canonical dual-sink CLI logging MWE.
 *
 * Demonstrates strict stream separation (data to stdout, diagnostics to stderr),
 * dual-sink logging (interactive stderr + forensic timestamped file), thread-safe
 * time extraction, and standard CLI verbosity flags (-v, -l).
 */

enum { LOG_INFO, LOG_DEBUG };

static int   g_console_lvl = LOG_INFO;
static FILE *g_log_file    = NULL;

void log_msg(int level, const char *fmt, ...) {
    if (level > g_console_lvl && !g_log_file) return;

    va_list args;

    // 1. Console sink: strictly stderr
    if (level <= g_console_lvl) {
        va_start(args, fmt);
        fprintf(stderr, "[%s] ", level == LOG_DEBUG ? "DEBUG" : "INFO");
        vfprintf(stderr, fmt, args);
        fprintf(stderr, "\n");
        va_end(args);
    }

    // 2. File sink: forensic timestamp, flushed immediately
    if (g_log_file) {
        time_t now = time(NULL);
        char tbuf[20];
        struct tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", &tm_buf);

        va_start(args, fmt);
        fprintf(g_log_file, "%s [%s] ", tbuf, level == LOG_DEBUG ? "DEBUG" : "INFO");
        vfprintf(g_log_file, fmt, args);
        fprintf(g_log_file, "\n");
        fflush(g_log_file);
        va_end(args);
    }
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) {
            g_console_lvl = LOG_DEBUG;
        } else if (!strcmp(argv[i], "-l") && i + 1 < argc) {
            g_log_file = fopen(argv[++i], "a");
            if (!g_log_file) {
                fprintf(stderr, "[warn] Failed to open log file '%s'\n", argv[i]);
            }
        }
    }

    log_msg(LOG_INFO, "Tool initialized");
    log_msg(LOG_DEBUG, "Diagnostic payload: argc=%d", argc);

    // Primary program payload: strictly stdout
    printf("42\n");

    if (g_log_file) fclose(g_log_file);
    return 0;
}

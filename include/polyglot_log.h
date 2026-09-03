#ifndef POLYGLOT_LOG_H
#define POLYGLOT_LOG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standardized log severity levels across all language layers.
 */
typedef enum {
    LOG_LVL_TRACE = 0,
    LOG_LVL_DEBUG = 1,
    LOG_LVL_INFO  = 2,
    LOG_LVL_WARN  = 3,
    LOG_LVL_ERROR = 4,
    LOG_LVL_FATAL = 5
} log_level_t;

/**
 * @brief Initialize the unified dual-sink logging engine.
 *
 * Configures the console sink (stderr) and optional file sink with independent
 * filtering thresholds.
 *
 * @param console_level Minimum severity level for console output (stderr).
 * @param log_file Path to destination log file on disk (NULL or empty string disables file sink).
 * @param file_level Minimum severity level for file logging (ignored if log_file is NULL).
 */
void logger_init(log_level_t console_level, const char* log_file, log_level_t file_level);

/**
 * @brief Check if a given log level is actively accepted by any configured sink.
 *
 * Allows callers to perform fast-path rejection before paying the cost of
 * expensive string formatting or numerical serialization.
 *
 * @param level Severity level to check.
 * @return 1 if any sink accepts the level; 0 otherwise.
 */
int logger_is_enabled(log_level_t level);

/**
 * @brief Dispatch a diagnostic log event through the unified logging core.
 *
 * @param level Severity level of the log message.
 * @param component Component or module name (e.g., "CLI", "Solver", "Mesh"). Null-safe.
 * @param message Pre-formatted UTF-8 log message body. Null-safe.
 */
void logger_dispatch(log_level_t level, const char* component, const char* message);

/**
 * @brief Formatted diagnostic dispatch helper for pure C callers.
 *
 * Evaluates arguments once into an intermediate stack buffer before dispatching.
 *
 * @param level Severity level of the log message.
 * @param component Component or module name.
 * @param fmt Standard printf-style format string.
 */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 3, 4)))
#endif
void logger_dispatch_format(log_level_t level, const char* component, const char* fmt, ...);

/**
 * @brief Flush all pending log buffers and gracefully terminate sinks.
 */
void logger_shutdown(void);

/**
 * @brief Pure C numerical component demonstration.
 */
int run_c_computation(void);

/**
 * @brief Fortran 2008 numerical solver component demonstration.
 */
void run_fortran_solver(void);

#ifdef __cplusplus
}
#endif

#endif /* POLYGLOT_LOG_H */

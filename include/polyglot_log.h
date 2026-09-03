#ifndef POLYGLOT_LOG_H
#define POLYGLOT_LOG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Standardized log severity levels across all language layers.
 * Ordering: TRACE < DEBUG < INFO < WARN < ERROR < FATAL < OFF.
 * Severity increases towards FATAL; verbosity increases towards TRACE.
 */
typedef enum {
    LOG_LVL_TRACE = 0,
    LOG_LVL_DEBUG = 1,
    LOG_LVL_INFO  = 2,
    LOG_LVL_WARN  = 3,
    LOG_LVL_ERROR = 4,
    LOG_LVL_FATAL = 5,
    LOG_LVL_OFF   = 6  /**< Silences all output for the designated sink */
} log_level_t;

/**
 * @brief Terminal color modes for the console sink.
 */
typedef enum {
    COLOR_MODE_AUTO   = 0, /**< Colorize when attached to an interactive TTY and NO_COLOR unset */
    COLOR_MODE_ALWAYS = 1, /**< Always emit ANSI escape sequences */
    COLOR_MODE_NEVER  = 2  /**< Never emit ANSI escape sequences */
} color_mode_t;

/**
 * @brief File sink output format.
 */
typedef enum {
    LOG_FORMAT_TEXT = 0,   /**< Standard human-readable ISO-timestamped format */
    LOG_FORMAT_JSON = 1    /**< Single-line structured NDJSON for automated ingest */
} log_format_t;

/**
 * @brief Status return codes for logger operations.
 */
typedef enum {
    LOGGER_OK                =  0,
    LOGGER_ERR_INVALID_ARG   = -1,
    LOGGER_ERR_FILE_OPEN     = -2,
    LOGGER_ERR_INIT          = -3
} logger_status_t;

/**
 * @brief Initialize the unified dual-sink logging engine.
 *
 * Configures the console sink (stderr) and optional file sink with independent
 * filtering thresholds. Ensures parent directories exist for the log file.
 *
 * @param console_level Minimum severity level for console output (stderr).
 * @param log_file Path to destination log file on disk (NULL or empty string disables file sink).
 * @param file_level Minimum severity level for file logging (ignored if log_file is NULL).
 * @return LOGGER_OK on success, or a negative logger_status_t code on error.
 */
logger_status_t logger_init(log_level_t console_level, const char* log_file, log_level_t file_level);

/**
 * @brief Check if a given log level is actively accepted by ANY configured sink.
 *
 * Checks against active sink levels (console_level and file_level), allowing callers
 * to perform true fast-path rejection before expensive computations or string conversions.
 *
 * @param level Severity level to check.
 * @return 1 if any active sink accepts the level; 0 otherwise.
 */
int logger_is_enabled(log_level_t level);

/**
 * @brief Check if a given log level is accepted specifically by the console sink (stderr).
 */
int logger_is_console_enabled(log_level_t level);

/**
 * @brief Check if a given log level is accepted specifically by the file sink.
 */
int logger_is_file_enabled(log_level_t level);

/**
 * @brief Dynamically reconfigure the console sink severity threshold.
 */
logger_status_t logger_set_console_level(log_level_t level);

/**
 * @brief Dynamically reconfigure the file sink severity threshold.
 */
logger_status_t logger_set_file_level(log_level_t level);

/**
 * @brief Set the terminal color mode for the console sink.
 */
void logger_set_color_mode(color_mode_t mode);

/**
 * @brief Set the file sink format (text or JSON).
 */
void logger_set_format(log_format_t format);

/**
 * @brief Dispatch a diagnostic log event through the unified logging core.
 *
 * @param level Severity level of the log message.
 * @param component Component or module name (e.g., "CLI", "Solver", "Mesh"). Null-safe.
 * @param message Pre-formatted UTF-8 log message body. Null-safe.
 */
void logger_dispatch(log_level_t level, const char* component, const char* message);

/**
 * @brief Source-location-aware diagnostic dispatch function.
 *
 * Records origin file, line, and function for deep forensic file logging.
 *
 * @param level Severity level.
 * @param component Component or module name.
 * @param file Source file name (__FILE__).
 * @param line Source line number (__LINE__).
 * @param func Enclosing function name (__func__).
 * @param message Pre-formatted log message body.
 */
void logger_dispatch_loc(log_level_t level, const char* component,
                         const char* file, int line, const char* func,
                         const char* message);

/**
 * @brief Enable in-memory backtrace ring-buffer for crash forensics.
 * Caches recent TRACE and DEBUG messages in memory without polluting console output.
 * @param message_count Number of recent messages to preserve (e.g., 32 or 64).
 */
void logger_enable_backtrace(size_t message_count);

/**
 * @brief Dump cached backtrace messages immediately to all active sinks.
 */
void logger_dump_backtrace(void);

/**
 * @brief Configure file rotation size and archive retention policy.
 * @param max_file_size_bytes Maximum size in bytes before rotating (default: 10 MB).
 * @param max_rotated_files Maximum number of rotated archive files (default: 3).
 */
logger_status_t logger_set_rotation_policy(size_t max_file_size_bytes, size_t max_rotated_files);

/**
 * @brief Extended initialization supporting explicit rotation parameters.
 */
logger_status_t logger_init_ext(log_level_t console_level,
                               const char* log_file,
                               log_level_t file_level,
                               size_t max_file_size_bytes,
                               size_t max_rotated_files);

/**
 * @brief Formatted diagnostic dispatch helper for pure C callers.
 * Evaluates format arguments into an intermediate buffer before dispatching.
 */
#if defined(__GNUC__) || defined(__clang__)
  #define POLYGLOT_PRINTF_FORMAT(fmt_idx, va_idx) __attribute__((format(printf, fmt_idx, va_idx)))
  #define POLYGLOT_FORMAT_STRING
#elif defined(_MSC_VER)
  #include <sal.h>
  #define POLYGLOT_PRINTF_FORMAT(fmt_idx, va_idx)
  #define POLYGLOT_FORMAT_STRING _Printf_format_string_
#else
  #define POLYGLOT_PRINTF_FORMAT(fmt_idx, va_idx)
  #define POLYGLOT_FORMAT_STRING
#endif

POLYGLOT_PRINTF_FORMAT(3, 4)
void logger_dispatch_format(log_level_t level, const char* component,
                            POLYGLOT_FORMAT_STRING const char* fmt, ...);

/**
 * @brief Formatted diagnostic dispatch with caller source location.
 */
POLYGLOT_PRINTF_FORMAT(6, 7)
void logger_dispatch_format_loc(log_level_t level, const char* component,
                                const char* file, int line, const char* func,
                                POLYGLOT_FORMAT_STRING const char* fmt, ...);

/**
 * @brief Force flush all active logger sinks immediately.
 */
void logger_flush(void);

/**
 * @brief Flush pending buffers and clean up primary logger resources.
 * Safe to call multiple times and safe to call on exit.
 */
void logger_shutdown(void);

#ifdef __cplusplus
}
#endif

/* -------------------------------------------------------------------------- */
/* Compile-Time Stripping Configuration                                       */
/* Set POLYGLOT_ACTIVE_LEVEL to LOG_LVL_DEBUG, LOG_LVL_INFO, etc. to compile  */
/* out lower-level log macros completely in performance-critical builds.      */
/* -------------------------------------------------------------------------- */

#ifndef POLYGLOT_ACTIVE_LEVEL
#define POLYGLOT_ACTIVE_LEVEL LOG_LVL_TRACE
#endif

#if POLYGLOT_ACTIVE_LEVEL <= LOG_LVL_TRACE
#define LOG_TRACE(comp, msg) logger_dispatch_loc(LOG_LVL_TRACE, comp, __FILE__, __LINE__, __func__, msg)
#define LOGF_TRACE(comp, ...) do { if (logger_is_enabled(LOG_LVL_TRACE)) logger_dispatch_format_loc(LOG_LVL_TRACE, comp, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)
#else
#define LOG_TRACE(comp, msg) do {} while(0)
#define LOGF_TRACE(comp, ...) do {} while(0)
#endif

#if POLYGLOT_ACTIVE_LEVEL <= LOG_LVL_DEBUG
#define LOG_DEBUG(comp, msg) logger_dispatch_loc(LOG_LVL_DEBUG, comp, __FILE__, __LINE__, __func__, msg)
#define LOGF_DEBUG(comp, ...) do { if (logger_is_enabled(LOG_LVL_DEBUG)) logger_dispatch_format_loc(LOG_LVL_DEBUG, comp, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)
#else
#define LOG_DEBUG(comp, msg) do {} while(0)
#define LOGF_DEBUG(comp, ...) do {} while(0)
#endif

#if POLYGLOT_ACTIVE_LEVEL <= LOG_LVL_INFO
#define LOG_INFO(comp, msg)  logger_dispatch_loc(LOG_LVL_INFO,  comp, __FILE__, __LINE__, __func__, msg)
#define LOGF_INFO(comp, ...)  do { if (logger_is_enabled(LOG_LVL_INFO))  logger_dispatch_format_loc(LOG_LVL_INFO,  comp, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)
#else
#define LOG_INFO(comp, msg)  do {} while(0)
#define LOGF_INFO(comp, ...)  do {} while(0)
#endif

#define LOG_WARN(comp, msg)  logger_dispatch_loc(LOG_LVL_WARN,  comp, __FILE__, __LINE__, __func__, msg)
#define LOG_ERROR(comp, msg) logger_dispatch_loc(LOG_LVL_ERROR, comp, __FILE__, __LINE__, __func__, msg)
#define LOG_FATAL(comp, msg) logger_dispatch_loc(LOG_LVL_FATAL, comp, __FILE__, __LINE__, __func__, msg)

#define LOGF_WARN(comp, ...)  do { if (logger_is_enabled(LOG_LVL_WARN))  logger_dispatch_format_loc(LOG_LVL_WARN,  comp, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)
#define LOGF_ERROR(comp, ...) do { if (logger_is_enabled(LOG_LVL_ERROR)) logger_dispatch_format_loc(LOG_LVL_ERROR, comp, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)
#define LOGF_FATAL(comp, ...) do { if (logger_is_enabled(LOG_LVL_FATAL)) logger_dispatch_format_loc(LOG_LVL_FATAL, comp, __FILE__, __LINE__, __func__, __VA_ARGS__); } while(0)

/**
 * @brief Fail-fast diagnostic macros: log at FATAL level, flush all sinks, and abort immediately.
 */
#define LOG_FATAL_AND_ABORT(comp, msg) \
    do { \
        LOG_FATAL(comp, msg); \
        logger_flush(); \
        abort(); \
    } while(0)

#define LOGF_FATAL_AND_ABORT(comp, ...) \
    do { \
        LOGF_FATAL(comp, __VA_ARGS__); \
        logger_flush(); \
        abort(); \
    } while(0)

#endif /* POLYGLOT_LOG_H */

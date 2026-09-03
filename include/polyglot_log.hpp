#ifndef POLYGLOT_LOG_HPP
#define POLYGLOT_LOG_HPP

#include "polyglot_log.h"
#include <sstream>
#include <string>
#include <utility>

/**
 * @file polyglot_log.hpp
 * @brief Zero-dependency C++17 header-only stream wrapper around polyglot C ABI.
 *
 * Uses C++17 fold expressions over std::ostringstream to provide type-safe,
 * stream-based logging for arbitrary types without external library headers.
 */

namespace polylog {

template <typename... Args>
inline void log_stream(log_level_t level, const char* component,
                       const char* file, int line, const char* func,
                       Args&&... args) {
    if (!logger_is_enabled(level)) return;
    std::ostringstream oss;
    (oss << ... << std::forward<Args>(args));
    std::string msg = oss.str();
    logger_dispatch_loc(level, component, file, line, func, msg.c_str());
}

} // namespace polylog

#if POLYGLOT_ACTIVE_LEVEL <= LOG_LVL_TRACE
#define LOG_CPP_TRACE(comp, ...) \
    polylog::log_stream(LOG_LVL_TRACE, comp, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define LOG_CPP_TRACE(comp, ...) do {} while(0)
#endif

#if POLYGLOT_ACTIVE_LEVEL <= LOG_LVL_DEBUG
#define LOG_CPP_DEBUG(comp, ...) \
    polylog::log_stream(LOG_LVL_DEBUG, comp, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define LOG_CPP_DEBUG(comp, ...) do {} while(0)
#endif

#if POLYGLOT_ACTIVE_LEVEL <= LOG_LVL_INFO
#define LOG_CPP_INFO(comp, ...) \
    polylog::log_stream(LOG_LVL_INFO, comp, __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define LOG_CPP_INFO(comp, ...) do {} while(0)
#endif

#define LOG_CPP_WARN(comp, ...) \
    polylog::log_stream(LOG_LVL_WARN, comp, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_CPP_ERROR(comp, ...) \
    polylog::log_stream(LOG_LVL_ERROR, comp, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_CPP_FATAL(comp, ...) \
    polylog::log_stream(LOG_LVL_FATAL, comp, __FILE__, __LINE__, __func__, __VA_ARGS__)

#define LOG_CPP_FATAL_AND_ABORT(comp, ...) \
    do { \
        LOG_CPP_FATAL(comp, __VA_ARGS__); \
        logger_flush(); \
        std::abort(); \
    } while(0)

#endif /* POLYGLOT_LOG_HPP */

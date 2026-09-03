#include "polyglot_log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include <cstdarg>
#include <cstdio>
#include <vector>
#include <memory>
#include <string>
#include <mutex>

static std::shared_ptr<spdlog::logger> g_logger = nullptr;
static std::mutex g_init_mutex;

static spdlog::level::level_enum map_level(log_level_t level) {
    switch (level) {
        case LOG_LVL_TRACE: return spdlog::level::trace;
        case LOG_LVL_DEBUG: return spdlog::level::debug;
        case LOG_LVL_INFO:  return spdlog::level::info;
        case LOG_LVL_WARN:  return spdlog::level::warn;
        case LOG_LVL_ERROR: return spdlog::level::err;
        case LOG_LVL_FATAL: return spdlog::level::critical;
        default:            return spdlog::level::info;
    }
}

extern "C" void logger_init(log_level_t console_level, const char* log_file, log_level_t file_level) {
    std::lock_guard<std::mutex> lock(g_init_mutex);

    // If a logger already exists, flush and drop it first
    if (g_logger) {
        g_logger->flush();
        spdlog::drop("polyglot_cli");
        g_logger = nullptr;
    }

    std::vector<spdlog::sink_ptr> sinks;

    // 1. Console Sink (Strictly stderr, colorized, stripped of high-frequency noise)
    auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    console_sink->set_level(map_level(console_level));
    console_sink->set_pattern("%^[%l]%$ %v");
    sinks.push_back(console_sink);

    // 2. File Sink (Optional rotating forensic disk log)
    if (log_file && log_file[0] != '\0') {
        // 10 MB per file, 3 rotated generations
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file, 1024 * 1024 * 10, 3
        );
        file_sink->set_level(map_level(file_level));
        // ISO-8601 UTC/local timestamp, PID, ThreadID, Level, Message
        file_sink->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%P:%t] [%l] %v");
        sinks.push_back(file_sink);
    }

    // 3. Root Logger: Permissive trace level; individual sinks filter independently
    g_logger = std::make_shared<spdlog::logger>("polyglot_cli", sinks.begin(), sinks.end());
    g_logger->set_level(spdlog::level::trace);

    // Crash resilience: automatically flush on warnings or higher severity
    g_logger->flush_on(spdlog::level::warn);

    spdlog::set_default_logger(g_logger);
}

extern "C" int logger_is_enabled(log_level_t level) {
    if (!g_logger) return 0;
    return g_logger->should_log(map_level(level)) ? 1 : 0;
}

extern "C" void logger_dispatch(log_level_t level, const char* component, const char* message) {
    if (!g_logger) return;

    spdlog::level::level_enum lvl = map_level(level);
    if (!g_logger->should_log(lvl)) return;

    const char* safe_msg = message ? message : "";

    if (component && component[0] != '\0') {
        g_logger->log(lvl, "[{}] {}", component, safe_msg);
    } else {
        g_logger->log(lvl, "{}", safe_msg);
    }
}

extern "C" void logger_dispatch_format(log_level_t level, const char* component, const char* fmt, ...) {
    if (!g_logger || !fmt) return;
    if (!logger_is_enabled(level)) return;

    char stack_buf[1024];
    va_list args;

    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(stack_buf, sizeof(stack_buf), fmt, args);
    va_end(args);

    if (needed < 0) {
        va_end(args_copy);
        return;
    }

    if (static_cast<size_t>(needed) < sizeof(stack_buf)) {
        va_end(args_copy);
        logger_dispatch(level, component, stack_buf);
    } else {
        std::vector<char> heap_buf(static_cast<size_t>(needed) + 1);
        vsnprintf(heap_buf.data(), heap_buf.size(), fmt, args_copy);
        va_end(args_copy);
        logger_dispatch(level, component, heap_buf.data());
    }
}

extern "C" void logger_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    if (g_logger) {
        g_logger->flush();
        spdlog::drop_all();
        g_logger = nullptr;
    }
}

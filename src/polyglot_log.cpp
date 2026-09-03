#include "polyglot_log.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/pattern_formatter.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

static bool is_valid_level(log_level_t level) {
    return level >= LOG_LVL_TRACE && level <= LOG_LVL_OFF;
}

static spdlog::level::level_enum map_level(log_level_t level) {
    switch (level) {
        case LOG_LVL_TRACE: return spdlog::level::trace;
        case LOG_LVL_DEBUG: return spdlog::level::debug;
        case LOG_LVL_INFO:  return spdlog::level::info;
        case LOG_LVL_WARN:  return spdlog::level::warn;
        case LOG_LVL_ERROR: return spdlog::level::err;
        case LOG_LVL_FATAL: return spdlog::level::critical;
        case LOG_LVL_OFF:   return spdlog::level::off;
        default:            return spdlog::level::info;
    }
}

/**
 * @brief Custom flag formatter that escapes quotes, backslashes, and control characters for JSON.
 */
class json_escaped_flag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg &msg, const std::tm &, spdlog::memory_buf_t &dest) override {
        std::string escaped;
        escaped.reserve(msg.payload.size() + 16);
        for (char c : msg.payload) {
            switch (c) {
                case '"':  escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                case '\b': escaped += "\\b";  break;
                case '\f': escaped += "\\f";  break;
                case '\n': escaped += "\\n";  break;
                case '\r': escaped += "\\r";  break;
                case '\t': escaped += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        escaped += buf;
                    } else {
                        escaped += c;
                    }
                    break;
            }
        }
        dest.append(escaped.data(), escaped.data() + escaped.size());
    }

    std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<json_escaped_flag>();
    }
};

// Fallback bootstrap logger created statically to safely absorb any logs before logger_init()
static std::shared_ptr<spdlog::logger> get_bootstrap_logger() {
    static std::shared_ptr<spdlog::logger> bootstrap = []() {
        auto b = spdlog::get("bootstrap");
        if (!b) {
            auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            sink->set_level(spdlog::level::trace);
            sink->set_pattern("%^[%l]%$ %v");
            if (std::getenv("NO_COLOR") != nullptr) {
                sink->set_color_mode(spdlog::color_mode::never);
            }
            b = std::make_shared<spdlog::logger>("bootstrap", sink);
            b->set_level(spdlog::level::trace);
        }
        return b;
    }();
    return bootstrap;
}

static std::mutex g_init_mutex;
static std::shared_ptr<spdlog::logger> g_logger = get_bootstrap_logger();

static std::shared_ptr<spdlog::sinks::stderr_color_sink_mt> g_console_sink;
static std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> g_file_sink;

static std::atomic<log_level_t>  g_console_level{LOG_LVL_INFO};
static std::atomic<log_level_t>  g_file_level{LOG_LVL_OFF};
static std::atomic<bool>         g_file_enabled{false};
static std::atomic<bool>         g_is_initialized{false};
static std::atomic<color_mode_t> g_color_mode{COLOR_MODE_AUTO};
static std::atomic<log_format_t> g_format{LOG_FORMAT_TEXT};
static std::atomic<size_t>       g_max_file_size_bytes{1024 * 1024 * 10};
static std::atomic<size_t>       g_max_rotated_files{3};

static void apply_color_mode(color_mode_t mode) {
    if (!g_console_sink) return;
#if defined(_WIN32)
    // On Windows, if stderr is redirected to a pipe or file (not a console),
    // spdlog's wincolor_sink WriteConsoleA fails with ERROR_INVALID_HANDLE.
    // In that case, use automatic mode so WriteFile is used to preserve pipe output.
    bool is_pipe = !_isatty(_fileno(stderr));
    if (is_pipe && mode == COLOR_MODE_ALWAYS) {
        g_console_sink->set_color_mode(spdlog::color_mode::automatic);
        return;
    }
#endif
    if (mode == COLOR_MODE_ALWAYS) {
        // Explicit CLI flag overrides environment variable per NO_COLOR specification
        g_console_sink->set_color_mode(spdlog::color_mode::always);
    } else if (mode == COLOR_MODE_NEVER || (mode == COLOR_MODE_AUTO && std::getenv("NO_COLOR") != nullptr)) {
        g_console_sink->set_color_mode(spdlog::color_mode::never);
    } else {
        g_console_sink->set_color_mode(spdlog::color_mode::automatic);
    }
}

static void apply_file_format(log_format_t fmt) {
    if (!g_file_sink) return;
    if (fmt == LOG_FORMAT_JSON) {
        // Single-line NDJSON format with escaped message payload
        auto formatter = std::make_unique<spdlog::pattern_formatter>();
        formatter->add_flag<json_escaped_flag>('*').set_pattern(
            "{\"timestamp\":\"%Y-%m-%dT%H:%M:%S.%e%z\",\"pid\":%P,\"tid\":%t,"
            "\"level\":\"%l\",\"file\":\"%s\",\"line\":%#,\"message\":\"%*\"}"
        );
        g_file_sink->set_formatter(std::move(formatter));
    } else {
        // Standard ISO-8601 UTC/local timestamp, PID, ThreadID, Level, SourceLocation, Message
        g_file_sink->set_pattern("[%Y-%m-%dT%H:%M:%S.%e%z] [%P:%t] [%l] [%s:%#] %v");
    }
}

extern "C" logger_status_t logger_set_rotation_policy(size_t max_file_size_bytes, size_t max_rotated_files) {
    if (max_file_size_bytes == 0 || max_rotated_files == 0) {
        return LOGGER_ERR_INVALID_ARG;
    }
    g_max_file_size_bytes.store(max_file_size_bytes, std::memory_order_release);
    g_max_rotated_files.store(max_rotated_files, std::memory_order_release);
    return LOGGER_OK;
}

extern "C" logger_status_t logger_init_ext(log_level_t console_level,
                                           const char* log_file,
                                           log_level_t file_level,
                                           size_t max_file_size_bytes,
                                           size_t max_rotated_files) {
    if (max_file_size_bytes > 0 && max_rotated_files > 0) {
        logger_set_rotation_policy(max_file_size_bytes, max_rotated_files);
    }
    return logger_init(console_level, log_file, file_level);
}

extern "C" void logger_enable_backtrace(size_t message_count) {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    auto logger = std::atomic_load(&g_logger);
    if (logger && message_count > 0) {
        logger->enable_backtrace(message_count);
    }
}

extern "C" void logger_dump_backtrace(void) {
    auto logger = std::atomic_load(&g_logger);
    if (logger) {
        logger->dump_backtrace();
    }
}

extern "C" logger_status_t logger_init(log_level_t console_level, const char* log_file, log_level_t file_level) {
    if (!is_valid_level(console_level) || !is_valid_level(file_level)) {
        return LOGGER_ERR_INVALID_ARG;
    }

    std::lock_guard<std::mutex> lock(g_init_mutex);

    // If an existing custom logger is active, flush and drop it first
    if (g_is_initialized.load(std::memory_order_relaxed)) {
        auto cur_logger = std::atomic_load(&g_logger);
        if (cur_logger) cur_logger->flush();
        spdlog::drop("polyglot_cli");
        g_is_initialized.store(false, std::memory_order_release);
    }

    std::vector<spdlog::sink_ptr> sinks;

    // 1. Console Sink (Strictly stderr)
    g_console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    g_console_sink->set_level(map_level(console_level));
    g_console_sink->set_pattern("%^[%l]%$ %v");
    apply_color_mode(g_color_mode.load(std::memory_order_relaxed));

    sinks.push_back(g_console_sink);
    g_console_level.store(console_level, std::memory_order_release);

    // 2. File Sink (Optional rotating disk log)
    bool file_configured = false;
    g_file_sink = nullptr;

    if (log_file && log_file[0] != '\0' && file_level != LOG_LVL_OFF) {
        try {
            // Ensure parent directory exists before creating log file
            std::filesystem::path p(log_file);
            auto parent = p.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }

            g_file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file,
                g_max_file_size_bytes.load(std::memory_order_relaxed),
                g_max_rotated_files.load(std::memory_order_relaxed)
            );
            g_file_sink->set_level(map_level(file_level));
            apply_file_format(g_format.load(std::memory_order_relaxed));
            sinks.push_back(g_file_sink);

            file_configured = true;
            g_file_level.store(file_level, std::memory_order_release);
            g_file_enabled.store(true, std::memory_order_release);
        } catch (const spdlog::spdlog_ex& ex) {
            std::fprintf(stderr, "[error] Failed to create log file '%s': %s\n", log_file, ex.what());
            return LOGGER_ERR_FILE_OPEN;
        } catch (const std::exception& ex) {
            std::fprintf(stderr, "[error] Filesystem error for log file '%s': %s\n", log_file, ex.what());
            return LOGGER_ERR_FILE_OPEN;
        }
    }

    if (!file_configured) {
        g_file_enabled.store(false, std::memory_order_release);
        g_file_level.store(LOG_LVL_OFF, std::memory_order_release);
    }

    // 3. Multi-sink Logger Configuration
    try {
        auto new_logger = std::make_shared<spdlog::logger>("polyglot_cli", sinks.begin(), sinks.end());
        new_logger->set_level(spdlog::level::trace); // Root is permissive; individual sinks filter
        new_logger->flush_on(spdlog::level::warn);   // Flush automatically on warning or higher

        std::atomic_store(&g_logger, new_logger);
        spdlog::set_default_logger(new_logger);
        g_is_initialized.store(true, std::memory_order_release);
    } catch (const spdlog::spdlog_ex& ex) {
        std::fprintf(stderr, "[error] Logger initialization failed: %s\n", ex.what());
        return LOGGER_ERR_INIT;
    }

    return LOGGER_OK;
}

extern "C" logger_status_t logger_set_console_level(log_level_t level) {
    if (!is_valid_level(level)) return LOGGER_ERR_INVALID_ARG;
    g_console_level.store(level, std::memory_order_release);
    if (g_console_sink) {
        g_console_sink->set_level(map_level(level));
    }
    return LOGGER_OK;
}

extern "C" logger_status_t logger_set_file_level(log_level_t level) {
    if (!is_valid_level(level)) return LOGGER_ERR_INVALID_ARG;
    g_file_level.store(level, std::memory_order_release);
    if (g_file_sink) {
        g_file_sink->set_level(map_level(level));
    }
    return LOGGER_OK;
}

extern "C" void logger_set_color_mode(color_mode_t mode) {
    g_color_mode.store(mode, std::memory_order_release);
    apply_color_mode(mode);
}

extern "C" void logger_set_format(log_format_t format) {
    g_format.store(format, std::memory_order_release);
    apply_file_format(format);
}

extern "C" int logger_is_console_enabled(log_level_t level) {
    if (!is_valid_level(level) || level == LOG_LVL_OFF) return 0;
    log_level_t clvl = g_console_level.load(std::memory_order_relaxed);
    if (clvl == LOG_LVL_OFF) return 0;
    return (level >= clvl) ? 1 : 0;
}

extern "C" int logger_is_file_enabled(log_level_t level) {
    if (!is_valid_level(level) || level == LOG_LVL_OFF) return 0;
    if (!g_file_enabled.load(std::memory_order_relaxed)) return 0;
    log_level_t flvl = g_file_level.load(std::memory_order_relaxed);
    if (flvl == LOG_LVL_OFF) return 0;
    return (level >= flvl) ? 1 : 0;
}

extern "C" int logger_is_enabled(log_level_t level) {
    return (logger_is_console_enabled(level) || logger_is_file_enabled(level)) ? 1 : 0;
}

extern "C" void logger_dispatch_loc(log_level_t level, const char* component,
                                    const char* file, int line, const char* func,
                                    const char* message) {
    if (!logger_is_enabled(level)) return;

    auto logger = std::atomic_load(&g_logger);
    if (!logger) return;

    spdlog::level::level_enum lvl = map_level(level);
    spdlog::source_loc loc{file ? file : "", line, func ? func : ""};
    const char* safe_msg = message ? message : "";

    if (component && component[0] != '\0') {
        logger->log(loc, lvl, "[{}] {}", component, safe_msg);
    } else {
        logger->log(loc, lvl, "{}", safe_msg);
    }
}

extern "C" void logger_dispatch(log_level_t level, const char* component, const char* message) {
    logger_dispatch_loc(level, component, "", 0, "", message);
}

extern "C" void logger_dispatch_format_loc(log_level_t level, const char* component,
                                           const char* file, int line, const char* func,
                                           const char* fmt, ...) {
    if (!fmt) return;
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
        logger_dispatch_loc(level, component, file, line, func, stack_buf);
    } else {
        std::vector<char> heap_buf(static_cast<size_t>(needed) + 1);
        vsnprintf(heap_buf.data(), heap_buf.size(), fmt, args_copy);
        va_end(args_copy);
        logger_dispatch_loc(level, component, file, line, func, heap_buf.data());
    }
}

extern "C" void logger_dispatch_format(log_level_t level, const char* component, const char* fmt, ...) {
    if (!fmt) return;
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
        logger_dispatch_loc(level, component, "", 0, "", stack_buf);
    } else {
        std::vector<char> heap_buf(static_cast<size_t>(needed) + 1);
        vsnprintf(heap_buf.data(), heap_buf.size(), fmt, args_copy);
        va_end(args_copy);
        logger_dispatch_loc(level, component, "", 0, "", heap_buf.data());
    }
}

extern "C" void logger_flush(void) {
    auto logger = std::atomic_load(&g_logger);
    if (logger) {
        logger->flush();
    }
}

extern "C" void logger_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_init_mutex);
    auto cur_logger = std::atomic_load(&g_logger);
    if (cur_logger) {
        cur_logger->flush();
        if (g_is_initialized.load(std::memory_order_relaxed)) {
            spdlog::drop("polyglot_cli");
            g_is_initialized.store(false, std::memory_order_release);
        }
        // Retain fallback bootstrap logger so any late static destructors / teardown logs do not crash
        auto bootstrap = get_bootstrap_logger();
        std::atomic_store(&g_logger, bootstrap);
        spdlog::set_default_logger(bootstrap);
    }
}

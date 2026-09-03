#include <iostream>
#include <fstream>
#include <string_view>
#include <ctime>

/**
 * @file main.cpp
 * @brief Zero-dependency C++17 canonical dual-sink CLI logging MWE.
 *
 * Demonstrates variadic fold-expressions for type-safe logging, strict stream
 * separation (payload to std::cout, diagnostics to std::cerr), thread-safe time
 * formatting, and dual sinks.
 */

enum Level { INFO, DEBUG };

static Level         g_console_lvl = INFO;
static std::ofstream g_log_file;

template <typename... Args>
void log_msg(Level lvl, Args&&... args) {
    if (lvl > g_console_lvl && !g_log_file.is_open()) return;

    auto emit = [&](std::ostream& os, bool timestamp) {
        if (timestamp) {
            std::time_t t = std::time(nullptr);
            char buf[32];
            std::tm tm_buf;
#if defined(_WIN32)
            localtime_s(&tm_buf, &t);
#else
            localtime_r(&t, &tm_buf);
#endif
            std::strftime(buf, sizeof(buf), "%T ", &tm_buf);
            os << buf;
        }
        os << (lvl == DEBUG ? "[DEBUG] " : "[INFO] ");
        (os << ... << args) << '\n';
    };

    // 1. Console sink: stderr only
    if (lvl <= g_console_lvl) emit(std::cerr, false);

    // 2. File sink: timestamped and immediately flushed
    if (g_log_file.is_open()) {
        emit(g_log_file, true);
        g_log_file.flush();
    }
}

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-v") {
            g_console_lvl = DEBUG;
        } else if (arg == "-l" && i + 1 < argc) {
            g_log_file.open(argv[++i], std::ios::app);
            if (!g_log_file.is_open()) {
                std::cerr << "[warn] Failed to open log file '" << argv[i] << "'\n";
            }
        }
    }

    log_msg(INFO, "Tool initialized");
    log_msg(DEBUG, "Diagnostic payload: argc=", argc);

    // Primary program payload: strictly stdout
    std::cout << 42 << '\n';
    return 0;
}

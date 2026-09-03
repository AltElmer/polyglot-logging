#include "polyglot_log.h"
#include "c_subsystem.h"

#if defined(ENABLE_FORTRAN_SUBSYSTEM)
#include "fortran_solver.h"
#endif

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Crash resilience signal handler.
 * Safely flushes all pending in-memory diagnostic logs to disk before re-raising.
 */
static void emergency_flush_handler(int sig) {
    logger_flush();
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

struct LoggerConfig {
    log_level_t console_level   = LOG_LVL_INFO;
    log_level_t file_level      = LOG_LVL_TRACE;
    std::string log_file;
    bool        file_explicit   = false;
    bool        quiet           = false;
    bool        silent          = false;
    int         verbosity_count = 0;
    std::string color_mode      = "auto";
};

static bool parse_level_name(std::string_view name, log_level_t& out_level) {
    std::string lower{name};
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "trace")                    { out_level = LOG_LVL_TRACE; return true; }
    if (lower == "debug")                    { out_level = LOG_LVL_DEBUG; return true; }
    if (lower == "info")                     { out_level = LOG_LVL_INFO;  return true; }
    if (lower == "warn" || lower == "warning"){ out_level = LOG_LVL_WARN;  return true; }
    if (lower == "error")                    { out_level = LOG_LVL_ERROR; return true; }
    if (lower == "fatal" || lower == "critical") { out_level = LOG_LVL_FATAL; return true; }
    if (lower == "off")                      { out_level = LOG_LVL_OFF;   return true; }
    return false;
}

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS]\n\n"
              << "A cross-platform, hardware-agnostic polyglot CLI demonstrating canonical\n"
              << "dual-sink verbose debug logging across C, C++, and Fortran.\n\n"
              << "Options:\n"
              << "  -v, --verbose              Increase console verbosity (-v for DEBUG, -vv for TRACE)\n"
              << "  -q, --quiet                Quiet mode: display only errors and warnings on console\n"
              << "      --silent               Silent mode: disable all console logging (LOG_LVL_OFF)\n"
              << "      --log-level LEVEL      Explicitly set console level (trace|debug|info|warn|error|off)\n"
              << "  -l, --log-file PATH        Enable rotating file sink at specified path\n"
              << "      --log-file-level LEVEL Set file sink level independently (default: trace)\n"
              << "      --color MODE           Terminal color mode (auto|always|never)\n"
              << "  -h, --help                 Display this help message and exit\n"
              << "      --version              Display version information and exit\n\n"
              << "Environment Variables:\n"
              << "  POLYGLOT_LOG_LEVEL         Override default console verbosity level\n"
              << "  POLYGLOT_LOG_FILE          Default path for diagnostic log file\n"
              << "  POLYGLOT_LOG_FILE_LEVEL    Override file sink threshold\n"
              << "  NO_COLOR                   Suppresses ANSI colors per https://no-color.org\n\n"
              << "Stream Separation:\n"
              << "  Standard Output (stdout): Reserved strictly for machine/payload data.\n"
              << "  Standard Error  (stderr): Reserved strictly for diagnostic logging.\n";
}

int main(int argc, char* argv[]) {
    // 1. Install emergency signal handlers to guarantee file sink flushing on fatal faults
    std::signal(SIGSEGV, emergency_flush_handler);
    std::signal(SIGABRT, emergency_flush_handler);
    std::signal(SIGFPE,  emergency_flush_handler);
    std::signal(SIGTERM, emergency_flush_handler);

    // Guarantee clean shutdown flush on standard process exit
    std::atexit(logger_shutdown);

    LoggerConfig cfg;

    // 2. Read environment variable defaults
    if (const char* env_lvl = std::getenv("POLYGLOT_LOG_LEVEL")) {
        parse_level_name(env_lvl, cfg.console_level);
    } else if (const char* env_log = std::getenv("POLYGLOT_LOG")) {
        parse_level_name(env_log, cfg.console_level);
    }

    if (const char* env_file = std::getenv("POLYGLOT_LOG_FILE")) {
        cfg.log_file = env_file;
    }

    if (const char* env_file_lvl = std::getenv("POLYGLOT_LOG_FILE_LEVEL")) {
        parse_level_name(env_file_lvl, cfg.file_level);
    }

    // 3. Parse command-line arguments (CLI takes precedence over environment)
    bool explicit_console_level = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "polyglot-cli version 1.0.0\n";
            return 0;
        } else if (arg == "-q" || arg == "--quiet") {
            cfg.quiet = true;
        } else if (arg == "--silent") {
            cfg.silent = true;
        } else if (arg == "-v" || arg == "--verbose") {
            cfg.verbosity_count++;
        } else if (arg == "-vv") {
            cfg.verbosity_count += 2;
        } else if (arg == "-vvv") {
            cfg.verbosity_count += 3;
        } else if (arg == "--log-level") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --log-level requires a LEVEL argument.\n";
                return 1;
            }
            if (!parse_level_name(argv[++i], cfg.console_level)) {
                std::cerr << "Error: Invalid log level '" << argv[i] << "'.\n";
                return 1;
            }
            explicit_console_level = true;
        } else if (arg.rfind("--log-level=", 0) == 0) {
            std::string_view val = arg.substr(12);
            if (!parse_level_name(val, cfg.console_level)) {
                std::cerr << "Error: Invalid log level '" << val << "'.\n";
                return 1;
            }
            explicit_console_level = true;
        } else if (arg == "-l" || arg == "--log-file") {
            if (i + 1 >= argc) {
                std::cerr << "Error: " << arg << " requires a PATH argument.\n";
                return 1;
            }
            cfg.log_file = argv[++i];
            cfg.file_explicit = true;
        } else if (arg.rfind("--log-file=", 0) == 0) {
            cfg.log_file = std::string(arg.substr(11));
            cfg.file_explicit = true;
        } else if (arg == "--log-file-level") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --log-file-level requires a LEVEL argument.\n";
                return 1;
            }
            if (!parse_level_name(argv[++i], cfg.file_level)) {
                std::cerr << "Error: Invalid log file level '" << argv[i] << "'.\n";
                return 1;
            }
        } else if (arg.rfind("--log-file-level=", 0) == 0) {
            std::string_view val = arg.substr(17);
            if (!parse_level_name(val, cfg.file_level)) {
                std::cerr << "Error: Invalid log file level '" << val << "'.\n";
                return 1;
            }
        } else if (arg == "--color") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --color requires a MODE argument (auto|always|never).\n";
                return 1;
            }
            cfg.color_mode = argv[++i];
        } else if (arg.rfind("--color=", 0) == 0) {
            cfg.color_mode = std::string(arg.substr(8));
        } else {
            std::cerr << "Unknown option: " << arg << "\n"
                      << "Run '" << argv[0] << " --help' for usage.\n";
            return 1;
        }
    }

    // 4. Validate mutually exclusive options
    if ((cfg.quiet || cfg.silent) && cfg.verbosity_count > 0) {
        std::cerr << "Error: Contradictory options --quiet/--silent and --verbose cannot be used together.\n";
        return 1;
    }

    // Determine effective console level if not explicitly set via --log-level
    if (!explicit_console_level) {
        if (cfg.silent) {
            cfg.console_level = LOG_LVL_OFF;
        } else if (cfg.quiet) {
            cfg.console_level = LOG_LVL_ERROR;
        } else if (cfg.verbosity_count == 1) {
            cfg.console_level = LOG_LVL_DEBUG;
        } else if (cfg.verbosity_count >= 2) {
            cfg.console_level = LOG_LVL_TRACE;
        }
    }

    // 5. Initialize the unified dual-sink logging engine
    logger_status_t status = logger_init(cfg.console_level,
                                         cfg.log_file.empty() ? nullptr : cfg.log_file.c_str(),
                                         cfg.file_level);

    if (status != LOGGER_OK) {
        if (cfg.file_explicit) {
            std::cerr << "Error: Failed to initialize explicitly requested log file '"
                      << cfg.log_file << "'. Aborting.\n";
            return 1;
        }
        // If file came from environment or default, fallback gracefully with a warning
        LOGF_WARN("CLI", "Failed to initialize default log file '%s', proceeding with console only",
                  cfg.log_file.c_str());
    }

    LOG_INFO("CLI", "Application starting");

    if (logger_is_enabled(LOG_LVL_DEBUG)) {
        LOGF_DEBUG("CLI", "Parsed CLI config: verbosity=%d, quiet=%d, silent=%d, log_file='%s'",
                   cfg.verbosity_count, cfg.quiet, cfg.silent, cfg.log_file.c_str());
    }

    // 6. Invoke Pure C numerical component
    run_c_computation();

    // 7. Invoke Fortran solver component (if compiled with Fortran support)
#if defined(ENABLE_FORTRAN_SUBSYSTEM)
    run_fortran_solver();
#else
    LOG_DEBUG("CLI", "Fortran subsystem omitted (compiler not active in build)");
#endif

    // 8. Application Data Payload: STRICTLY directed to stdout
    std::cout << "COMPUTATION_SUCCESS: 42\n";

    LOG_INFO("CLI", "Application completed successfully");

    return 0;
}

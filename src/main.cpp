#include "polyglot_log.hpp"
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

#if !defined(_WIN32)
#include <unistd.h>
#endif

/**
 * @brief Graceful termination signal handler for SIGTERM.
 * Flushes all pending buffers and cleanly terminates process with exit code 0.
 */
static void sigterm_handler(int) {
    logger_shutdown();
    std::_Exit(0);
}

/**
 * @brief Crash safety signal handler for fatal hardware faults (SIGSEGV, SIGABRT, SIGFPE).
 * Resets signal disposition immediately and triggers minimal flush with deadlock safeguard.
 */
static void fatal_crash_handler(int sig) {
    std::signal(sig, SIG_DFL);
#if !defined(_WIN32)
    // Anti-deadlock alarm: force kernel termination if a mutex was held during fault
    alarm(2);
#endif
    logger_dump_backtrace(); // Automatically emit recent cached forensic history prior to crash
    logger_flush();
    std::raise(sig);
}

struct LoggerConfig {
    log_level_t  console_level   = LOG_LVL_INFO;
    log_level_t  file_level      = LOG_LVL_TRACE;
    std::string  log_file;
    bool         file_explicit   = false;
    bool         quiet           = false;
    bool         silent          = false;
    int          verbosity_count = 0;
    color_mode_t color_mode      = COLOR_MODE_AUTO;
    log_format_t log_format      = LOG_FORMAT_TEXT;
    size_t       max_file_size_mb = 10;
    size_t       max_rotated_files = 3;
};

static bool parse_level_name(std::string_view name, log_level_t& out_level) {
    std::string lower{name};
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "trace")                         { out_level = LOG_LVL_TRACE; return true; }
    if (lower == "debug")                         { out_level = LOG_LVL_DEBUG; return true; }
    if (lower == "info")                          { out_level = LOG_LVL_INFO;  return true; }
    if (lower == "warn" || lower == "warning")     { out_level = LOG_LVL_WARN;  return true; }
    if (lower == "error")                         { out_level = LOG_LVL_ERROR; return true; }
    if (lower == "fatal" || lower == "critical")  { out_level = LOG_LVL_FATAL; return true; }
    if (lower == "off")                           { out_level = LOG_LVL_OFF;   return true; }
    return false;
}

static bool parse_color_mode(std::string_view mode, color_mode_t& out_mode) {
    if (mode == "auto")   { out_mode = COLOR_MODE_AUTO;   return true; }
    if (mode == "always") { out_mode = COLOR_MODE_ALWAYS; return true; }
    if (mode == "never")  { out_mode = COLOR_MODE_NEVER;  return true; }
    return false;
}

static bool parse_log_format(std::string_view fmt, log_format_t& out_fmt) {
    if (fmt == "text") { out_fmt = LOG_FORMAT_TEXT; return true; }
    if (fmt == "json") { out_fmt = LOG_FORMAT_JSON; return true; }
    return false;
}

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS] [--] [ARGUMENTS...]\n\n"
              << "A cross-platform, hardware-agnostic polyglot CLI demonstrating canonical\n"
              << "dual-sink verbose debug logging across C, C++, and Fortran.\n\n"
              << "Options:\n"
              << "  -v, --verbose              Increase console verbosity (-v for DEBUG, -vv for TRACE)\n"
              << "  -q, --quiet                Quiet mode: display only errors on console\n"
              << "      --silent               Silent mode: disable all console logging (LOG_LVL_OFF)\n"
              << "      --log-level LEVEL      Explicitly set console level (trace|debug|info|warn|error|off)\n"
              << "  -l, --log-file PATH        Enable rotating file sink at specified path\n"
              << "      --log-file-level LEVEL Set file sink level independently (default: trace)\n"
              << "      --color MODE           Terminal color mode (auto|always|never)\n"
              << "      --log-format FORMAT    File sink format (text|json)\n"
              << "      --log-max-size MB      Maximum log file size in MB before rotating (default: 10)\n"
              << "      --log-max-files NUM    Maximum rotated log archive files to keep (default: 3)\n"
              << "  -h, --help                 Display this help message and exit\n"
              << "      --version              Display version information and exit\n"
              << "  --                         End of options delimiter\n\n"
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
    // 1. Install signal handlers
    std::signal(SIGTERM, sigterm_handler);
    std::signal(SIGSEGV, fatal_crash_handler);
    std::signal(SIGABRT, fatal_crash_handler);
    std::signal(SIGFPE,  fatal_crash_handler);

    // Guarantee clean shutdown flush on standard process exit
    std::atexit(logger_shutdown);

    // Enable in-memory forensic backtrace ring-buffer (caches up to 32 recent messages)
    logger_enable_backtrace(32);

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
    bool parsing_options = true;
    std::vector<std::string> positional_args;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (parsing_options && arg == "--") {
            parsing_options = false;
            continue;
        }

        if (parsing_options && arg.rfind("-", 0) == 0 && arg != "-") {
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
                if (!parse_color_mode(argv[++i], cfg.color_mode)) {
                    std::cerr << "Error: Invalid color mode '" << argv[i] << "'.\n";
                    return 1;
                }
            } else if (arg.rfind("--color=", 0) == 0) {
                std::string_view val = arg.substr(8);
                if (!parse_color_mode(val, cfg.color_mode)) {
                    std::cerr << "Error: Invalid color mode '" << val << "'.\n";
                    return 1;
                }
            } else if (arg == "--log-format") {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --log-format requires a FORMAT argument (text|json).\n";
                    return 1;
                }
                if (!parse_log_format(argv[++i], cfg.log_format)) {
                    std::cerr << "Error: Invalid log format '" << argv[i] << "'.\n";
                    return 1;
                }
            } else if (arg.rfind("--log-format=", 0) == 0) {
                std::string_view val = arg.substr(13);
                if (!parse_log_format(val, cfg.log_format)) {
                    std::cerr << "Error: Invalid log format '" << val << "'.\n";
                    return 1;
                }
            } else if (arg == "--log-max-size") {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --log-max-size requires a size in MB.\n";
                    return 1;
                }
                try {
                    cfg.max_file_size_mb = std::stoul(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid size for --log-max-size: '" << argv[i] << "'.\n";
                    return 1;
                }
            } else if (arg.rfind("--log-max-size=", 0) == 0) {
                std::string val{arg.substr(15)};
                try {
                    cfg.max_file_size_mb = std::stoul(val);
                } catch (...) {
                    std::cerr << "Error: Invalid size for --log-max-size: '" << val << "'.\n";
                    return 1;
                }
            } else if (arg == "--log-max-files") {
                if (i + 1 >= argc) {
                    std::cerr << "Error: --log-max-files requires a count.\n";
                    return 1;
                }
                try {
                    cfg.max_rotated_files = std::stoul(argv[++i]);
                } catch (...) {
                    std::cerr << "Error: Invalid count for --log-max-files: '" << argv[i] << "'.\n";
                    return 1;
                }
            } else if (arg.rfind("--log-max-files=", 0) == 0) {
                std::string val{arg.substr(16)};
                try {
                    cfg.max_rotated_files = std::stoul(val);
                } catch (...) {
                    std::cerr << "Error: Invalid count for --log-max-files: '" << val << "'.\n";
                    return 1;
                }
            } else {
                std::cerr << "Unknown option: " << arg << "\n"
                          << "Run '" << argv[0] << " --help' for usage.\n";
                return 1;
            }
        } else {
            positional_args.emplace_back(arg);
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

    // Configure options on logger core
    logger_set_color_mode(cfg.color_mode);
    logger_set_format(cfg.log_format);
    logger_set_rotation_policy(cfg.max_file_size_mb * 1024 * 1024, cfg.max_rotated_files);

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
        std::fprintf(stderr, "[warn] [CLI] Failed to initialize default log file '%s', proceeding console-only\n",
                     cfg.log_file.c_str());
    }

    LOG_CPP_INFO("CLI", "Application starting");

    if (logger_is_enabled(LOG_LVL_DEBUG)) {
        LOG_CPP_DEBUG("CLI", "Parsed CLI config: verbosity=", cfg.verbosity_count,
                      ", quiet=", cfg.quiet, ", silent=", cfg.silent, ", log_file='", cfg.log_file, "'");
    }

    if (!positional_args.empty() && logger_is_enabled(LOG_LVL_DEBUG)) {
        LOG_CPP_DEBUG("CLI", "Received ", positional_args.size(), " positional arguments");
    }

    // 6. Invoke Pure C numerical component and check error code
    int rc_c = run_c_computation();
    if (rc_c != 0) {
        LOG_CPP_ERROR("CLI", "C subsystem computation failed with error code: ", rc_c);
        return rc_c;
    }

    // 7. Invoke Fortran solver component (if compiled with Fortran support)
#if defined(ENABLE_FORTRAN_SUBSYSTEM)
    int rc_f = run_fortran_solver();
    if (rc_f != 0) {
        LOG_CPP_ERROR("CLI", "Fortran solver failed with error code: ", rc_f);
        return rc_f;
    }
#else
    LOG_CPP_DEBUG("CLI", "Fortran subsystem omitted (compiler not active in build)");
#endif

    // 8. Application Data Payload: STRICTLY directed to stdout
    std::cout << "COMPUTATION_SUCCESS: 42\n";

    LOG_CPP_INFO("CLI", "Application completed successfully");

    return 0;
}

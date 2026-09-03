#include "polyglot_log.h"
#include <iostream>
#include <string_view>
#include <string>
#include <vector>

extern "C" int run_c_computation(void);

#if defined(ENABLE_FORTRAN_SUBSYSTEM)
extern "C" void run_fortran_solver(void);
#endif

static void print_usage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [OPTIONS]\n\n"
              << "A cross-platform, hardware-agnostic polyglot CLI demonstrating canonical\n"
              << "dual-sink verbose debug logging across C, C++, and Fortran.\n\n"
              << "Options:\n"
              << "  -v, --verbose        Increase console verbosity (-v for DEBUG, -vv for TRACE)\n"
              << "  -q, --quiet          Quiet mode: display only errors on console\n"
              << "  -l, --log-file PATH  Enable rotating file sink at specified path (captures TRACE)\n"
              << "  -h, --help           Display this help message and exit\n"
              << "      --version        Display version information and exit\n\n"
              << "Stream Separation:\n"
              << "  Standard Output (stdout): Reserved strictly for machine/payload data.\n"
              << "  Standard Error  (stderr): Reserved strictly for diagnostic logging.\n";
}

int main(int argc, char* argv[]) {
    log_level_t console_lvl = LOG_LVL_INFO;
    const char* log_file = nullptr;
    int verbosity_count = 0;
    bool quiet_mode = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            std::cout << "polyglot-cli version 1.0.0\n";
            return 0;
        } else if (arg == "-q" || arg == "--quiet") {
            quiet_mode = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbosity_count++;
        } else if (arg == "-vv") {
            verbosity_count += 2;
        } else if (arg == "-vvv") {
            verbosity_count += 3;
        } else if ((arg == "-l" || arg == "--log-file") && i + 1 < argc) {
            log_file = argv[++i];
        } else {
            std::cerr << "Unknown option: " << arg << "\n"
                      << "Run '" << argv[0] << " --help' for usage.\n";
            return 1;
        }
    }

    if (quiet_mode) {
        console_lvl = LOG_LVL_ERROR;
    } else if (verbosity_count == 1) {
        console_lvl = LOG_LVL_DEBUG;
    } else if (verbosity_count >= 2) {
        console_lvl = LOG_LVL_TRACE;
    }

    // Initialize dual sinks: console at selected level, file sink captures all (TRACE)
    logger_init(console_lvl, log_file, LOG_LVL_TRACE);

    logger_dispatch(LOG_LVL_INFO, "CLI", "Application starting");

    if (logger_is_enabled(LOG_LVL_DEBUG)) {
        std::string dbg = "Parsed CLI options: verbosity=" + std::to_string(verbosity_count) +
                          ", quiet=" + (quiet_mode ? "true" : "false") +
                          ", log_file=" + (log_file ? log_file : "none");
        logger_dispatch(LOG_LVL_DEBUG, "CLI", dbg.c_str());
    }

    // 1. Invoke Pure C numerical component
    run_c_computation();

    // 2. Invoke Fortran solver component (if compiled with Fortran support)
#if defined(ENABLE_FORTRAN_SUBSYSTEM)
    run_fortran_solver();
#else
    logger_dispatch(LOG_LVL_DEBUG, "CLI", "Fortran subsystem omitted (compiler not active)");
#endif

    // 3. Application Data Payload: STRICTLY directed to stdout
    std::cout << "COMPUTATION_SUCCESS: 42\n";

    logger_dispatch(LOG_LVL_INFO, "CLI", "Application completed successfully");

    // Flush and close sinks
    logger_shutdown();

    return 0;
}

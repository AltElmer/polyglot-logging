#include "polyglot_log.h"
#include <iostream>
#include <fstream>
#include <string>

/**
 * @brief Regression test asserting JSON escaping prevents JSON injection
 * and maintains strict Single-Line NDJSON compatibility.
 */
int main(int argc, char* argv[]) {
    const char* log_file = (argc > 1) ? argv[1] : "test_escape.json";
    logger_set_format(LOG_FORMAT_JSON);
    logger_status_t status = logger_init(LOG_LVL_OFF, log_file, LOG_LVL_TRACE);
    if (status != LOGGER_OK) {
        std::cerr << "Failed to initialize logger: " << status << "\n";
        return 1;
    }

    // Log message containing quotes, backslashes, tabs, and newlines
    LOG_INFO("EscapeTest", "Testing \"quotes\", \\backslashes\\, \tand\nnewlines");
    logger_flush();
    logger_shutdown();

    std::ifstream ifs(log_file);
    std::string line;
    if (!std::getline(ifs, line)) {
        std::cerr << "Failed to read line from JSON log\n";
        return 1;
    }

    // Assert that quotes, backslashes, newlines, and tabs are escaped
    if (line.find("\\\"quotes\\\"") == std::string::npos) {
        std::cerr << "Escaped quotes missing in NDJSON: " << line << "\n";
        return 1;
    }
    if (line.find("\\\\backslashes\\\\") == std::string::npos) {
        std::cerr << "Escaped backslashes missing in NDJSON: " << line << "\n";
        return 1;
    }
    if (line.find("\\nnewlines") == std::string::npos) {
        std::cerr << "Escaped newline missing in NDJSON: " << line << "\n";
        return 1;
    }
    if (line.find("\\tand") == std::string::npos) {
        std::cerr << "Escaped tab missing in NDJSON: " << line << "\n";
        return 1;
    }

    // Assert there is no second line (ensuring single-line NDJSON format)
    std::string second_line;
    if (std::getline(ifs, second_line) && !second_line.empty()) {
        std::cerr << "Unexpected second line in NDJSON: " << second_line << "\n";
        return 1;
    }

    std::cout << "JSON_ESCAPING_TEST_SUCCESS\n";
    return 0;
}

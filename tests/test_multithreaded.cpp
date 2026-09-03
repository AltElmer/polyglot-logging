#include "polyglot_log.h"
#include <iostream>
#include <thread>
#include <vector>
#include <cassert>

/**
 * @brief Concurrency & multi-threading stress test.
 *
 * Spawns 8 threads logging concurrently across INFO, DEBUG, and TRACE levels
 * to verify thread safety of the C ABI, std::atomic_load/store on g_logger,
 * and rotating file sink mutexes.
 */
int main(int argc, char* argv[]) {
    const char* log_file = (argc > 1) ? argv[1] : nullptr;
    log_level_t console_lvl = (argc > 2) ? LOG_LVL_INFO : LOG_LVL_OFF;

    logger_status_t status = logger_init(console_lvl, log_file, LOG_LVL_TRACE);
    if (status != LOGGER_OK) {
        std::cerr << "Failed to initialize logger in multithreaded test: " << status << "\n";
        return 1;
    }

    const int num_threads = 8;
    const int msgs_per_thread = 200;
    std::vector<std::thread> workers;

    for (int t = 0; t < num_threads; ++t) {
        workers.emplace_back([t, msgs_per_thread]() {
            for (int m = 0; m < msgs_per_thread; ++m) {
                if (m % 3 == 0) {
                    LOGF_INFO("ThreadTest", "Worker %d emitting informational event %d", t, m);
                } else if (m % 3 == 1) {
                    LOGF_DEBUG("ThreadTest", "Worker %d diagnostic debug packet %d", t, m);
                } else {
                    LOGF_TRACE("ThreadTest", "Worker %d fine trace event %d", t, m);
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    logger_flush();
    logger_shutdown();

    std::cout << "MULTITHREADED_TEST_SUCCESS\n";
    return 0;
}

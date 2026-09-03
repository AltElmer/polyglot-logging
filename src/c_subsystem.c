#include "polyglot_log.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Demonstrates pure C numerical/computational component logging.
 *
 * Consumes the central C ABI wrapper defined in polyglot_log.h without any
 * C++ or spdlog dependencies.
 */
int run_c_computation(void) {
    logger_dispatch(LOG_LVL_INFO, "C-Subsystem", "Initializing sparse matrix allocation");

    // Fast-path guard example: skip work if debug level is not active
    if (logger_is_enabled(LOG_LVL_DEBUG)) {
        size_t allocated_bytes = 4096;
        logger_dispatch_format(LOG_LVL_DEBUG, "C-Subsystem",
                               "Allocated %lu bytes for CSR row pointers", (unsigned long)allocated_bytes);
    }

    if (logger_is_enabled(LOG_LVL_TRACE)) {
        logger_dispatch(LOG_LVL_TRACE, "C-Subsystem", "CSR index verification passed [dim=64x64, nnz=256]");
    }

    logger_dispatch(LOG_LVL_INFO, "C-Subsystem", "Matrix factorization complete");
    return 0;
}

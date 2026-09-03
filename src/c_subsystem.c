#include "c_subsystem.h"
#include "polyglot_log.h"
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Pure C numerical computation demonstration routine.
 *
 * Demonstrates logging from pure C using the polyglot logging facade
 * with automatic caller source-location capture (__FILE__, __LINE__).
 */
int run_c_computation(void) {
    LOG_INFO("C-Subsystem", "Initializing sparse matrix allocation");

    // Fast-path guard: macro internally checks logger_is_enabled(LOG_LVL_DEBUG)
    size_t allocated_bytes = 4096;
    LOGF_DEBUG("C-Subsystem", "Allocated %lu bytes for CSR row pointers", (unsigned long)allocated_bytes);

    LOG_TRACE("C-Subsystem", "CSR index verification passed [dim=64x64, nnz=256]");

    LOG_INFO("C-Subsystem", "Matrix factorization complete");
    return 0;
}

#ifndef FORTRAN_SOLVER_H
#define FORTRAN_SOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Fortran 2008 numerical solver demonstration routine.
 *
 * Demonstrates logging from Fortran 2008 using the ISO_C_BINDING module
 * and the polyglot logging C ABI.
 *
 * @return 0 on convergence/success, non-zero on numerical failure.
 */
int run_fortran_solver(void);

#ifdef __cplusplus
}
#endif

#endif /* FORTRAN_SOLVER_H */

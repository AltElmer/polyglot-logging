module logging_mod
    use, intrinsic :: iso_c_binding, only: c_int, c_char, c_null_char
    implicit none
    private

    public :: LOG_LVL_TRACE, LOG_LVL_DEBUG, LOG_LVL_INFO
    public :: LOG_LVL_WARN, LOG_LVL_ERROR, LOG_LVL_FATAL
    public :: f_log, f_log_enabled, run_fortran_solver

    enum, bind(c)
        enumerator :: LOG_LVL_TRACE = 0
        enumerator :: LOG_LVL_DEBUG = 1
        enumerator :: LOG_LVL_INFO  = 2
        enumerator :: LOG_LVL_WARN  = 3
        enumerator :: LOG_LVL_ERROR = 4
        enumerator :: LOG_LVL_FATAL = 5
    end enum

    interface
        subroutine logger_dispatch(level, component, message) bind(c, name="logger_dispatch")
            import :: c_int, c_char
            integer(c_int), value         :: level
            character(c_char), intent(in) :: component(*)
            character(c_char), intent(in) :: message(*)
        end subroutine logger_dispatch

        function logger_is_enabled(level) bind(c, name="logger_is_enabled") result(res)
            import :: c_int
            integer(c_int), value :: level
            integer(c_int)        :: res
        end function logger_is_enabled
    end interface

contains

    !> Check if a log level is enabled to guard expensive computations
    function f_log_enabled(level) result(enabled)
        integer(c_int), intent(in) :: level
        logical                    :: enabled

        enabled = (logger_is_enabled(level) /= 0)
    end function f_log_enabled

    !> Safe Fortran logging wrapper that ensures strict C-string null termination
    subroutine f_log(level, component, message)
        integer(c_int), intent(in)   :: level
        character(len=*), intent(in) :: component
        character(len=*), intent(in) :: message

        ! CRITICAL DEFENSE: Fortran strings are fixed/whitespace-padded and NOT null-terminated.
        ! We MUST concatenate c_null_char to prevent heap/stack over-reads in C/C++.
        call logger_dispatch(level, &
                             trim(component) // c_null_char, &
                             trim(message) // c_null_char)
    end subroutine f_log

    !> Demonstrates a numerical computation routine written in Fortran interoperating with C ABI
    subroutine run_fortran_solver() bind(c, name="run_fortran_solver")
        implicit none

        call f_log(LOG_LVL_INFO, "Fortran-Solver", "Initializing Krylov subspace iterative solver")

        if (f_log_enabled(LOG_LVL_DEBUG)) then
            call f_log(LOG_LVL_DEBUG, "Fortran-Solver", "Iteration 1: Residual norm = 1.42e-02")
        end if

        if (f_log_enabled(LOG_LVL_TRACE)) then
            call f_log(LOG_LVL_TRACE, "Fortran-Solver", "Vector dot product <r, r> = 2.0164e-04")
        end if

        if (f_log_enabled(LOG_LVL_DEBUG)) then
            call f_log(LOG_LVL_DEBUG, "Fortran-Solver", "Iteration 2: Residual norm = 8.15e-06")
        end if

        call f_log(LOG_LVL_INFO, "Fortran-Solver", "Solver converged in 2 iterations (tol=1.0e-05)")
    end subroutine run_fortran_solver

end module logging_mod

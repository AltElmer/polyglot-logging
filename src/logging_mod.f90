module logging_mod
    use, intrinsic :: iso_c_binding, only: c_int, c_char, c_null_char, c_double
    implicit none
    private

    public :: LOG_LVL_TRACE, LOG_LVL_DEBUG, LOG_LVL_INFO
    public :: LOG_LVL_WARN, LOG_LVL_ERROR, LOG_LVL_FATAL, LOG_LVL_OFF
    public :: f_log, f_log_loc, f_log_iteration, f_log_enabled, run_fortran_solver

    enum, bind(c)
        enumerator :: LOG_LVL_TRACE = 0
        enumerator :: LOG_LVL_DEBUG = 1
        enumerator :: LOG_LVL_INFO  = 2
        enumerator :: LOG_LVL_WARN  = 3
        enumerator :: LOG_LVL_ERROR = 4
        enumerator :: LOG_LVL_FATAL = 5
        enumerator :: LOG_LVL_OFF   = 6
    end enum

    interface
        subroutine logger_dispatch(level, component, message) bind(c, name="logger_dispatch")
            import :: c_int, c_char
            integer(c_int), value         :: level
            character(c_char), intent(in) :: component(*)
            character(c_char), intent(in) :: message(*)
        end subroutine logger_dispatch

        subroutine logger_dispatch_loc(level, component, file, line, func, message) &
                bind(c, name="logger_dispatch_loc")
            import :: c_int, c_char
            integer(c_int), value         :: level
            character(c_char), intent(in) :: component(*)
            character(c_char), intent(in) :: file(*)
            integer(c_int), value         :: line
            character(c_char), intent(in) :: func(*)
            character(c_char), intent(in) :: message(*)
        end subroutine logger_dispatch_loc

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

    !> Safe Fortran logging with explicit caller source coordinates
    subroutine f_log_loc(level, component, file, line, message)
        integer(c_int), intent(in)   :: level
        character(len=*), intent(in) :: component
        character(len=*), intent(in) :: file
        integer, intent(in)          :: line
        character(len=*), intent(in) :: message

        call logger_dispatch_loc(level, &
                                 trim(component) // c_null_char, &
                                 trim(file) // c_null_char, &
                                 int(line, c_int), &
                                 "" // c_null_char, &
                                 trim(message) // c_null_char)
    end subroutine f_log_loc

    !> Internal-write formatting helper for mathematical iteration loops
    subroutine f_log_iteration(level, component, iter, residual)
        integer(c_int), intent(in)   :: level
        character(len=*), intent(in) :: component
        integer, intent(in)          :: iter
        real(c_double), intent(in)   :: residual
        character(len=128)           :: buffer

        if (.not. f_log_enabled(level)) return

        write(buffer, '(A, I0, A, ES10.3)') "Iteration ", iter, ": Residual norm = ", residual
        call f_log_loc(level, &
                       component, &
                       __FILE__, &
                       __LINE__, &
                       trim(buffer))
    end subroutine f_log_iteration

    !> Demonstrates a numerical computation routine written in Fortran interoperating with C ABI
    function run_fortran_solver() result(rc) bind(c, name="run_fortran_solver")
        implicit none
        integer(c_int) :: rc
        real(c_double) :: res1, res2

        rc = 0
        call f_log_loc(LOG_LVL_INFO, &
                       "Fortran-Solver", &
                       __FILE__, &
                       __LINE__, &
                       "Initializing Krylov subspace iterative solver")

        res1 = 1.42d-02
        call f_log_iteration(LOG_LVL_DEBUG, "Fortran-Solver", 1, res1)

        if (f_log_enabled(LOG_LVL_TRACE)) then
            call f_log_loc(LOG_LVL_TRACE, &
                           "Fortran-Solver", &
                           __FILE__, &
                           __LINE__, &
                           "Vector dot product <r, r> = 2.0164e-04")
        end if

        res2 = 8.15d-06
        call f_log_iteration(LOG_LVL_DEBUG, "Fortran-Solver", 2, res2)

        call f_log_loc(LOG_LVL_INFO, &
                       "Fortran-Solver", &
                       __FILE__, &
                       __LINE__, &
                       "Solver converged in 2 iterations (tol=1.0e-05)")
    end function run_fortran_solver

end module logging_mod

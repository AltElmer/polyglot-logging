!> @file main.f90
!> @brief Zero-dependency modern Fortran 2008 canonical dual-sink CLI logging MWE.
!>
!> Demonstrates iso_fortran_env stream separation (error_unit vs output_unit),
!> newunit automatic file descriptor allocation, portable date_and_time timestamps,
!> and crash-resilient flush.
program main
    use, intrinsic :: iso_fortran_env, only: error_unit, output_unit
    implicit none

    integer, parameter :: LOG_INFO = 0, LOG_DEBUG = 1
    integer :: console_lvl = LOG_INFO
    integer :: file_unit   = -1
    integer :: i, n
    character(len=256) :: arg

    ! 1. CLI argument parsing
    n = command_argument_count()
    i = 1
    do while (i <= n)
        call get_command_argument(i, arg)
        if (trim(arg) == "-v") then
            console_lvl = LOG_DEBUG
        else if (trim(arg) == "-l" .and. i < n) then
            i = i + 1
            call get_command_argument(i, arg)
            open(newunit=file_unit, file=trim(arg), status="UNKNOWN", position="APPEND")
        end if
        i = i + 1
    end do

    ! 2. Diagnostic logging: strictly error_unit / file
    call log_msg(LOG_INFO, "Tool initialized")
    call log_msg(LOG_DEBUG, "Diagnostic payload: solver initialized")

    ! 3. Primary program payload: strictly output_unit (stdout)
    write(output_unit, '(A)') "42"

    if (file_unit /= -1) close(file_unit)

contains

    subroutine log_msg(level, msg)
        integer, intent(in) :: level
        character(len=*), intent(in) :: msg
        character(len=5) :: tag
        integer :: v(8)

        if (level > console_lvl .and. file_unit == -1) return

        tag = merge("DEBUG", "INFO ", level == LOG_DEBUG)

        ! Console sink: stderr only
        if (level <= console_lvl) then
            write(error_unit, '("[" , A , "] " , A)') trim(tag), trim(msg)
        end if

        ! File sink: timestamped and immediately flushed
        if (file_unit /= -1) then
            call date_and_time(values=v)
            write(file_unit, '(I2.2, ":", I2.2, ":", I2.2, " [", A, "] ", A)') &
                v(5), v(6), v(7), trim(tag), trim(msg)
            flush(file_unit)
        end if
    end subroutine log_msg

end program main

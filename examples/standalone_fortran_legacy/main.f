C======================================================================
C     CANONICAL DUAL-SINK LOGGING IN FORTRAN 77 (FIXED-FORM)
C     From Turn 8: Unit 0 for stderr, Unit * for stdout, COMMON block
C     for logger state, and IARGC/GETARG for argument parsing.
C======================================================================
      PROGRAM MAIN
      CHARACTER*64 ARG
      INTEGER N, I, IARGC

C     LOGGING STATE: LCON=CONSOLE LEVEL, LFILE=FILE ACTIVE FLAG
      INTEGER LCON, LFILE, IUNIT
      COMMON /LOGCFG/ LCON, LFILE, IUNIT

      LCON  = 0
      LFILE = 0
      IUNIT = 12

C     1. PARSE CLI ARGUMENTS VIA DE FACTO F77 EXTENSIONS
      N = IARGC()
      I = 1
 10   IF (I .GT. N) GOTO 20
        CALL GETARG(I, ARG)
        IF (ARG .EQ. '-v') THEN
          LCON = 1
        ELSE IF (ARG .EQ. '-l' .AND. I .LT. N) THEN
          I = I + 1
          CALL GETARG(I, ARG)
          OPEN(UNIT=IUNIT, FILE=ARG, STATUS='UNKNOWN')
          LFILE = 1
        ENDIF
        I = I + 1
        GOTO 10
 20   CONTINUE

C     2. DIAGNOSTIC LOGGING (STDERR / FILE)
      CALL LOGMSG(0, 'TOOL INITIALIZED')
      CALL LOGMSG(1, 'DIAGNOSTIC PAYLOAD: SOLVER INITIALIZED')

C     3. PRIMARY PROGRAM PAYLOAD (STDOUT ONLY)
      WRITE(*, '(A)') '42'

      IF (LFILE .EQ. 1) CLOSE(IUNIT)
      END

C======================================================================
C     SUBROUTINE: LOGMSG
C======================================================================
      SUBROUTINE LOGMSG(LVL, MSG)
      INTEGER LVL
      CHARACTER*(*) MSG
      INTEGER LCON, LFILE, IUNIT
      COMMON /LOGCFG/ LCON, LFILE, IUNIT
      CHARACTER*5 TAG

      IF (LVL .GT. LCON .AND. LFILE .EQ. 0) RETURN

      IF (LVL .EQ. 1) THEN
        TAG = 'DEBUG'
      ELSE
        TAG = 'INFO '
      ENDIF

C     CONSOLE SINK: PRE-CONNECTED UNIT 0 IS DE FACTO STDERR
      IF (LVL .LE. LCON) THEN
        WRITE(0, 100) TAG, MSG
      ENDIF

C     FILE SINK: WRITE TO DISK UNIT
      IF (LFILE .EQ. 1) THEN
        WRITE(IUNIT, 100) TAG, MSG
      ENDIF

 100  FORMAT('[', A5, '] ', A)
      END

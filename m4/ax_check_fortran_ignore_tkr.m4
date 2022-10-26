
dnl For now, we only need a type to pass from Fortran 2008 routines to
dnl the MPI_interceptF.c functions, which we declare also as Fortran
dnl interfaces. NO_ARG_CHECK attributes are not needed, just assumed
dnl types (TYPE(*)), or REAL types

# AX_CHECK_FORTRAN_IGNORE_TKR
# ---------------------------
AC_DEFUN([AX_CHECK_FORTRAN_IGNORE_TKR],
[
    # Assumed type
    AX_CHECK_FORTRAN_IGNORE_TKR_(
        [TYPE(*), DIMENSION(*)],
        [fortran_ignore_tkr=yes],
        [fortran_ignore_tkr=no])

    # REAL
    AS_IF([test x"$fortran_ignore_tkr" = xno], [
        AX_CHECK_FORTRAN_IGNORE_TKR_(
            [REAL, DIMENSION(*)],
            [fortran_ignore_tkr=yes],
            [fortran_ignore_tkr=no])
    ])

    AS_IF([test "x$fortran_ignore_tkr" = xyes], [
        AC_DEFINE_UNQUOTED([IGNORE_TKR_TYPE], [$fortran_ignore_tkr_type],
            [Fortran (void*)-like parameter type])
    ])
])

# AX_CHECK_FORTRAN_IGNORE_TKR_([TYPE], [ACTION-IF-OK], [ACTION-IF-ERROR])
# -----------------------------------------------------------------------
AC_DEFUN([AX_CHECK_FORTRAN_IGNORE_TKR_],
[
    AC_LANG_PUSH([Fortran])
    AC_MSG_CHECKING([for Fortran compiler support of $1])
    AC_COMPILE_IFELSE(AC_LANG_SOURCE([[!
subroutine conftest(buffer,n)
    implicit none
    $1 :: buffer
    integer :: n

    interface
        subroutine foo(buffer, n)
            $1, intent(in) :: buffer
            integer, intent(in) :: n
        end subroutine foo
    end interface

    call foo(buffer, n)
end subroutine
    ]]),
    [
        # ACTION-IF-OK
        msg=yes
        fortran_ignore_tkr_type="$1"
        $2
    ],
    [
        # ACTION-IF-ERROR
        msg=no
        $3
    ])
    AC_MSG_RESULT($msg)
    AC_LANG_POP([Fortran])
])

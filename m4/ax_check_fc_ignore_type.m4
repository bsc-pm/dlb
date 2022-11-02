
dnl For now, we only need a type to pass from Fortran 2008 routines to
dnl the MPI_interceptF.c functions, which we declare also as Fortran
dnl interfaces. NO_ARG_CHECK attributes are not needed, just check for
dnl assumed types (TYPE(*)), or REAL types; and assumed-rank entity
dnl (DIMENSION(..) or assumed-size array (DIMENSION(*))

# AX_CHECK_FC_IGNORE_TYPE
# -----------------------
AC_DEFUN([AX_CHECK_FC_IGNORE_TYPE],
[
    # Assumed-type + assumed-rank entity
    AX_CHECK_FC_IGNORE_TYPE_([TYPE(*), DIMENSION(..)])

    # Assumed type + assumed-size array
    AS_IF([test x"$fc_ignore_type" = x], [
        AX_CHECK_FC_IGNORE_TYPE_([TYPE(*), DIMENSION(*)])
    ])

    # REAL + assumed-size array
    AS_IF([test x"$fc_ignore_type" = x], [
        AX_CHECK_FC_IGNORE_TYPE_([REAL, DIMENSION(*)])
    ])

    AS_IF([test x"$fc_ignore_type" != x], [
        AC_DEFINE_UNQUOTED([FORTRAN_IGNORE_TYPE], [$fc_ignore_type],
            [Fortran (void*)-like parameter type])
    ])
])

# AX_CHECK_FC_IGNORE_TYPE_([TYPE])
# --------------------------------
AC_DEFUN([AX_CHECK_FC_IGNORE_TYPE_],
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
        msg=yes
        fc_ignore_type="$1"
    ],
    [
        msg=no
    ])
    AC_MSG_RESULT($msg)
    AC_LANG_POP([Fortran])
])

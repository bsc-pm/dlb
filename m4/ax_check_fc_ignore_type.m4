
# AX_CHECK_FC_ASSUMED_RANK
# ------------------------
AC_DEFUN([AX_CHECK_FC_ASSUMED_RANK],
[
    # Assumed-type + assumed-rank entity
    AX_CHECK_FC_BUFFER_TYPE_([TYPE(*), DIMENSION(..)])

    AS_IF([test x"$ax_check_fc_buffer_type" != x], [
        ax_check_fc_assumed_rank=yes
        AC_DEFINE([FORTRAN_SUPPORTS_ASSUMED_RANK], [1],
                [Whether Fortran supports assumed-rank, i.e. DIMENSION(..)])
    ])

])

# AX_CHECK_FC_IGNORE_TYPE
# -----------------------
AC_DEFUN([AX_CHECK_FC_IGNORE_TYPE],
[
    # Assumed type + assumed-size array
    AX_CHECK_FC_BUFFER_TYPE_([TYPE(*), DIMENSION(*)])

    # REAL + assumed-size array
    AS_IF([test x"$ax_check_fc_buffer_type" = x], [
        AX_CHECK_FC_BUFFER_TYPE_([REAL, DIMENSION(*)])
    ])

    AS_IF([test x"$ax_check_fc_buffer_type" != x], [
        ax_check_fc_ignore_type=yes
        AC_DEFINE_UNQUOTED([FORTRAN_IGNORE_TYPE], [$ax_check_fc_buffer_type],
            [Fortran (void*)-like parameter type])
    ])
])

# AX_CHECK_FC_BUFFER_TYPE_([TYPE])
# --------------------------------
AC_DEFUN([AX_CHECK_FC_BUFFER_TYPE_],
[
    AC_LANG_PUSH([Fortran])
    AC_MSG_CHECKING([Fortran compiler support for $1])
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
        ax_check_fc_buffer_type="$1"
    ],
    [
        msg=no
    ])
    AC_MSG_RESULT($msg)
    AC_LANG_POP([Fortran])
])

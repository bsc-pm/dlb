
# AX_CHECK_FC_HONORS_NO_C_BINDING_TYPE([ACTION-IF-OK], [ACTION-IF-ERROR])
# -----------------------------------------------------------------------
AC_DEFUN([AX_CHECK_FC_HONORS_NO_C_BINDING_TYPE],
[
    AC_LANG_PUSH([Fortran])
    AX_VAR_PUSHVALUE([FCFLAGS], [-Wall -Werror -Wno-c-binding-type])
    AC_COMPILE_IFELSE(AC_LANG_SOURCE([[!
subroutine foo(bar)
    implicit none
    type, bind(c) :: dtype
        integer :: a
    end type dtype
    type(dtype) :: bar
end subroutine
    ]]),
    [
        # ACTION-IF-OK
        :
        $1
    ],
    [
        # ACTION-IF-ERROR
        :
        $2
    ])
    AX_VAR_POPVALUE([FCFLAGS])
    AC_LANG_POP([Fortran])
])

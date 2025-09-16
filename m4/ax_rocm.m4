
# AX_ROCM
# -------
AC_DEFUN([AX_ROCM],
[
    AC_MSG_CHECKING([for ROCm])
    AC_ARG_WITH([rocm],
        AS_HELP_STRING([--with-rocm@<:@=DIR@:>@], [add ROCm support]),
        [], dnl Implicit: with_rocm=$enableval
        [with_rocm=check]
    )
    AC_MSG_RESULT([$with_rocm])

    AS_IF([test "x$with_rocm" != xno], [
        AS_IF([test -d "$with_rocm"], [
            AS_IF([test -d "$with_rocm/include"], [
                user_rocm_inc_flags="-I$with_rocm/include"
            ])
            AS_IF([test -d "$with_rocm/lib"], [
                user_rocm_libdir="$with_rocm/lib"
                user_rocm_lib_flags="-L$with_rocm/lib -Wl,-rpath,$with_rocm/lib"
            ])
            AS_IF([test -d "$with_rocm/lib64"], [
                user_rocm_libdir="$with_rocm/lib64"
                user_rocm_lib_flags="-L$with_rocm/lib64 -Wl,-rpath,$with_rocm/lib64"
            ])
        ])

        ### ROCM INCLUDES ###
        AX_VAR_PUSHVALUE([CPPFLAGS], [$CPPFLAGS $user_rocm_inc_flags])
        AC_CHECK_HEADERS([rocprofiler/v2/rocprofiler.h], [
            ROCM_CPPFLAGS="$user_rocm_inc_flags"
        ] , [
            AS_IF([test "x$with_rocm" != xcheck], [AC_MSG_ERROR([Cannot find ROCm headers])])
            with_rocm=no
        ])
        AX_VAR_POPVALUE([CPPFLAGS])
    ])

    AS_IF([test "x$with_rocm" != xno], [
        ### ROCM LIBS ###
        AX_VAR_PUSHVALUE([LIBS], [""])
        AX_VAR_PUSHVALUE([LDFLAGS], [$LDFLAGS $user_rocm_lib_flags])
        AC_SEARCH_LIBS([rocprofiler_initialize], [rocprofiler64v2], [
            ROCM_LDFLAGS="$user_rocm_lib_flags $LIBS"
            ROCM_LIBDIR="$user_rocm_libdir"
        ], [
            AS_IF([test "x$with_rocm" != xcheck], [AC_MSG_ERROR([Cannot find ROCm libraries])])
            with_rocm=no
        ])
        AX_VAR_POPVALUE([LDFLAGS])
        AX_VAR_POPVALUE([LIBS])
    ])

    AC_SUBST([ROCM_CPPFLAGS])
    AC_SUBST([ROCM_LDFLAGS])
    AC_SUBST([ROCM_LIBDIR])
    AM_CONDITIONAL([ROCM_PLUGIN], [test "x$with_rocm" != xno])
])

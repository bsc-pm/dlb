
# AX_HWLOC
# --------
AC_DEFUN([AX_PAPI],
[
    AC_MSG_CHECKING([for PAPI])
    AC_ARG_WITH([papi],
        AS_HELP_STRING([--with-papi@<:@=DIR@:>@], [add PAPI support]),
        [], dnl Implicit: with_papi=$enableval
        [with_papi=check]
    )
    AC_MSG_RESULT([$with_papi])

    AS_IF([test "x$with_papi" != xno], [
        AS_IF([test -d "$with_papi"], [
            AS_IF([test -d "$with_papi/include"], [
                user_papi_inc_flags="-I$with_papi/include"
            ])
            AS_IF([test -d "$with_papi/lib"], [
                user_papi_libdir="$with_papi/lib"
                user_papi_lib_flags="-L$with_papi/lib -Wl,-rpath,$with_papi/lib"
            ])
            AS_IF([test -d "$with_papi/lib64"], [
                user_papi_libdir="$with_papi/lib64"
                user_papi_lib_flags="-L$with_papi/lib64 -Wl,-rpath,$with_papi/lib64"
            ])

        ])

        ### PAPI INCLUDES ###
        AX_VAR_PUSHVALUE([CPPFLAGS], [$CPPFLAGS $user_papi_inc_flags])
        AC_CHECK_HEADERS([papi.h], [
            PAPI_CPPFLAGS="$user_papi_inc_flags"
        ] , [
            AS_IF([test "x$with_papi" != xcheck], [AC_MSG_ERROR([Cannot find PAPI headers])])
            with_papi=no
        ])
        AX_VAR_POPVALUE([CPPFLAGS])
    ])

    AS_IF([test "x$with_papi" != xno], [
        ### PAPI LIBS ###
        AX_VAR_PUSHVALUE([LIBS], [""])
        AX_VAR_PUSHVALUE([LDFLAGS], [$LDFLAGS $user_papi_lib_flags])
        AC_SEARCH_LIBS([PAPI_library_init], [papi], [
            PAPI_LDFLAGS="$user_papi_lib_flags $LIBS"
            PAPI_LIBDIR="$user_papi_libdir"
        ], [
            AS_IF([test "x$with_papi" != xcheck], [AC_MSG_ERROR([Cannot find PAPI libraries])])
            with_papi=no
        ])
        AX_VAR_POPVALUE([LDFLAGS])
        AX_VAR_POPVALUE([LIBS])
    ])

    AC_SUBST([PAPI_CPPFLAGS])
    AC_SUBST([PAPI_LDFLAGS])
    AC_SUBST([PAPI_LIBDIR])
    AM_CONDITIONAL([HAVE_PAPI], [test "x$with_papi" != xno])
])


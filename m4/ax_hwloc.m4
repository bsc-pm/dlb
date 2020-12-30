
# AX_HWLOC
# --------
AC_DEFUN([AX_HWLOC],
[
    AC_MSG_CHECKING([for HWLOC])
    AC_ARG_WITH([hwloc],
        AS_HELP_STRING([--with-hwloc@<:@=DIR@:>@], [add HWLOC support]),
        [], dnl Implicit: with_hwloc=$enableval
        [with_hwloc=check]
    )
    AC_MSG_RESULT([$with_hwloc])

    AS_IF([test "x$with_hwloc" != xno], [
        AS_IF([test -d "$with_hwloc"], [
            AS_IF([test -d "$with_hwloc/include"], [
                user_hwloc_inc_flags="-I$with_hwloc/include"
            ])
            AS_IF([test -d "$with_hwloc/lib"], [
                user_hwloc_libdir="$with_hwloc/lib"
                user_hwloc_lib_flags="-L$with_hwloc/lib"
            ])
        ])

        ### HWLOC INCLUDES ###
        AX_VAR_PUSHVALUE([CPPFLAGS], [$user_hwloc_inc_flags])
        AC_CHECK_HEADERS([hwloc.h], [
            HWLOC_CPPFLAGS="$user_hwloc_inc_flags"
        ] , [
            AS_IF([test "x$with_hwloc" != xcheck], [AC_MSG_ERROR([Cannot find HWLOC headers])])
            with_hwloc=no
        ])
        AX_VAR_POPVALUE([CPPFLAGS])
    ])

    AS_IF([test "x$with_hwloc" != xno], [
        ### HWLOC LIBS ###
        AX_VAR_PUSHVALUE([LIBS], [""])
        AX_VAR_PUSHVALUE([LDFLAGS], [$user_hwloc_lib_flags])
        AC_SEARCH_LIBS([hwloc_topology_init], [hwloc], [
            HWLOC_LDFLAGS="$user_hwloc_lib_flags $LIBS"
            HWLOC_LIBDIR="$user_hwloc_libdir"
        ], [
            AS_IF([test "x$with_hwloc" != xcheck], [AC_MSG_ERROR([Cannot find HWLOC libraries])])
            with_hwloc=no
        ])
        AX_VAR_POPVALUE([LDFLAGS])
        AX_VAR_POPVALUE([LIBS])
    ])

    AC_SUBST([HWLOC_CPPFLAGS])
    AC_SUBST([HWLOC_LDFLAGS])
    AC_SUBST([HWLOC_LIBDIR])
    AM_CONDITIONAL([HAVE_HWLOC], [test "x$with_hwloc" != xno])
])


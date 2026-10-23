# AX_EAR
# ------
AC_DEFUN([AX_EAR],
[
    AC_MSG_CHECKING([for EAR])
    AC_ARG_WITH([ear],
        AS_HELP_STRING([--with-ear@<:@=DIR@:>@], [add EAR support]),
        [], dnl Implicit: with_ear=$enableval
        [with_ear=check]
    )
    AC_MSG_RESULT([$with_ear])

    AS_IF([test "x$with_ear" = xcheck && test -n "$EAR_ROOT"], [
        with_ear="$EAR_ROOT"
    ])

    ear_explicit=check
    AS_IF([test "x$with_ear" != xcheck], [ear_explicit=yes])

    AS_IF([test "x$with_ear" != xno], [
        AS_IF([test -d "$with_ear/src"], [
            user_ear_inc_flags="-I$with_ear/src"
            user_ear_lib_flags="-Wl,--start-group,$with_ear/src/metrics/libmetrics.a,$with_ear/src/common/libcommon.a,$with_ear/src/daemon/local_api/libeard.a,--end-group"
        ])

        ### EAR HEADERS ###
        AX_VAR_PUSHVALUE([CPPFLAGS], [$CPPFLAGS $user_ear_inc_flags])
        AC_CHECK_HEADERS([common/hardware/topology.h common/states.h common/types/configuration/cluster_conf.h metrics/common/apis.h metrics/energy_cpu/energy_cpu.h], [], [
            AS_IF([test "x$ear_explicit" = xyes], [AC_MSG_ERROR([Cannot find EAR headers])])
            with_ear=no
        ])
        AX_VAR_POPVALUE([CPPFLAGS])
    ])

    AS_IF([test "x$with_ear" != xno], [
        ### EAR LIBS ###
        AX_VAR_PUSHVALUE([LIBS], [$user_ear_lib_flags])
        AX_VAR_PUSHVALUE([LDFLAGS], [$LDFLAGS -shared -fPIC])
        AC_MSG_CHECKING([whether EAR libraries link])
        AC_LINK_IFELSE([AC_LANG_PROGRAM(
            [[
extern int topology_init(void *tp);
extern int energy_cpu_load(void *tp, unsigned int api);
            ]],
            [[
topology_init(0);
energy_cpu_load(0, 0);
            ]])], [
            AC_MSG_RESULT([yes])
            EAR_LDFLAGS="$user_ear_lib_flags"
        ], [
            AC_MSG_RESULT([no])
            AS_IF([test "x$ear_explicit" = xyes], [AC_MSG_ERROR([Cannot find EAR libraries])])
            with_ear=no
        ])
        AX_VAR_POPVALUE([LDFLAGS])
        AX_VAR_POPVALUE([LIBS])
    ])

    AS_IF([test "x$with_ear" != xno], [
        EAR_CPPFLAGS="$user_ear_inc_flags"
    ], [
        EAR_CPPFLAGS=""
        EAR_LDFLAGS=""
    ])

    AC_SUBST([EAR_CPPFLAGS])
    AC_SUBST([EAR_LDFLAGS])
    AM_CONDITIONAL([EAR_PLUGIN], [test "x$with_ear" != xno])
])

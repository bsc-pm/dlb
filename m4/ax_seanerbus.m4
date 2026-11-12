# AX_SEANERBUS
# ------------
AC_DEFUN([AX_SEANERBUS],
[
    AC_ARG_WITH([seanerbus],
        AS_HELP_STRING([--with-seanerbus@<:@=DIR@:>@], [add SEANERBUS support]),
        [], dnl Implicit: with_seanerbus=$enableval
        [with_seanerbus=check]
    )

    AS_IF([test -d "$with_seanerbus"], [
        AS_IF([test -d "$with_seanerbus/lib/pkgconfig"], [
            PKG_CONFIG_PATH="$with_seanerbus/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
            export PKG_CONFIG_PATH
        ])
        AS_IF([test -d "$with_seanerbus/lib64/pkgconfig"], [
            PKG_CONFIG_PATH="$with_seanerbus/lib64/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
            export PKG_CONFIG_PATH
        ])
    ])

    AS_IF([test "x$with_seanerbus" != xno], [
        PKG_CHECK_MODULES([SEANERBUS], [seanerbus-c], [
            with_seanerbus=yes
            SEANERBUS_LIBDIR=`$PKG_CONFIG --variable=libdir seanerbus-c`
            SEANERBUS_LIBS="$SEANERBUS_LIBS -Wl,--disable-new-dtags -Wl,-rpath,$SEANERBUS_LIBDIR"
        ], [
            AS_IF([test "x$with_seanerbus" != xcheck], [AC_MSG_ERROR([Cannot find seanerbus-c])])
            with_seanerbus=no
        ])
    ])

    AM_CONDITIONAL([HAVE_SEANERBUS], [test "x$with_seanerbus" != xno])
])

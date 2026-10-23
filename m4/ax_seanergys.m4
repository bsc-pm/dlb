# AX_SEANERGYS
# ------------
AC_DEFUN([AX_SEANERGYS],
[
    AC_ARG_WITH([seanergys],
        AS_HELP_STRING([--with-seanergys@<:@=DIR@:>@], [add SEANERGYS support]),
        [], dnl Implicit: with_seanergys=$enableval
        [with_seanergys=check]
    )

    AS_IF([test -d "$with_seanergys"], [
        AS_IF([test -d "$with_seanergys/lib/pkgconfig"], [
            PKG_CONFIG_PATH="$with_seanergys/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
            export PKG_CONFIG_PATH 
        ])
        AS_IF([test -d "$with_seanergys/lib64/pkgconfig"], [ 
            PKG_CONFIG_PATH="$with_seanergys/lib64/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
            export PKG_CONFIG_PATH
        ])
    ])
    
    AS_IF([test "x$with_seanergys" != xno], [
        PKG_CHECK_MODULES([SEANERGYS], [seanerbus-c], [
            with_seanergys=yes
            SEANERGYS_LIBDIR=`$PKG_CONFIG --variable=libdir seanerbus-c`
            SEANERGYS_LIBS="$SEANERGYS_LIBS -Wl,--disable-new-dtags -Wl,-rpath,$SEANERGYS_LIBDIR"
        ], [
            AS_IF([test "x$with_seanergys" != xcheck], [AC_MSG_ERROR([Cannot find seanerbus-c])])
            with_seanergys=no
        ])
    ])

    AM_CONDITIONAL([HAVE_SEANERGYS], [test "x$with_seanergys" != xno])
])



# AX_CHECK_ATTRIBUTE_ALIAS
# ------------------------
AC_DEFUN([AX_CHECK_ATTRIBUTE_ALIAS],
[
    AC_REQUIRE([AC_PROG_GREP])
    AC_CHECK_TOOL([NM], [nm])

    ax_cv_check_attribute_alias=no
    AC_MSG_CHECKING([whether __attribute__((alias)) generates exported symbol])

    AC_LANG_PUSH([C])
    AC_LANG_CONFTEST([
        AC_LANG_SOURCE([[
            void original(void) {}
            void alias_name(void) __attribute__((alias("original")));
        ]])
    ])
    AC_LANG_POP([C])

    AS_IF([$CC -c conftest.c 2>&AS_MESSAGE_LOG_FD 1>&2], [
        AS_IF([$NM conftest.o | $GREP -q alias_name], [
            ax_cv_check_attribute_alias=yes
            AC_DEFINE([HAVE_ALIAS_ATTRIBUTE], [1], [Define if compiler supports usable alias])
        ])
    ])

    AC_MSG_RESULT([$ax_cv_check_attribute_alias])
])

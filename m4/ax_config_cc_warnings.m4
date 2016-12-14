
# AX_CONFIG_CC_WARNINGS
# --------------------
AC_DEFUN([AX_CONFIG_CC_WARNINGS],
[
    AS_IF([test "x$GCC" = "xyes"],[
        AC_CACHE_CHECK([gcc version],[ax_cv_gcc_version],[
            ax_cv_gcc_version="`$CC -dumpversion`"
            dnl GCC > 4.3
            AX_COMPARE_VERSION([$ax_cv_gcc_version], [gt], [4.3], [
                ax_cv_gcc_version_warnings="-Wno-unused-result "
            ])
            dnl GCC < 4.5
            AX_COMPARE_VERSION([$ax_cv_gcc_version], [lt], [4.5], [
                ax_cv_gcc_version_warnings+="-Wno-uninitialized "
            ])
        ])
    ])
])


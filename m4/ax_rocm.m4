
# AX_ROCM
# -------
AC_DEFUN([AX_ROCM],
[
    AC_MSG_CHECKING([for ROCm])
    AC_ARG_WITH([rocm],
        AS_HELP_STRING([--with-rocm@<:@=DIR@:>@], [add ROCm support (requires rocprofilerv2 or rocprofiler-sdk)]),
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
        AC_CHECK_HEADERS([hip/hip_runtime_api.h], [], [],
            [#define __HIP_PLATFORM_AMD__])
        AC_CHECK_HEADERS([rocprofiler/v2/rocprofiler.h])
        AC_CHECK_HEADERS([rocprofiler-sdk/rocprofiler.h], [], [],
            [#ifdef HAVE_HIP_HIP_RUNTIME_API_H
             # define __HIP_PLATFORM_AMD__
             # include <hip/hip_runtime_api.h>
             #endif
            ])
        AX_VAR_POPVALUE([CPPFLAGS])

        AS_IF([test "x$ac_cv_header_rocprofiler_v2_rocprofiler_h" = xyes \
                || test "x$ac_cv_header_rocprofiler_sdk_rocprofiler_h" = xyes], [
            ROCM_CPPFLAGS="$user_rocm_inc_flags"
        ], [
            AS_IF([test "x$with_rocm" != xcheck], [AC_MSG_ERROR([Cannot find ROCm headers])])
            with_rocm=no
        ])
    ])

    AS_IF([test "x$with_rocm" != xno], [
        ### ROCM LIBS ###
        AX_VAR_PUSHVALUE([LIBS])
        AX_VAR_PUSHVALUE([LDFLAGS], [$LDFLAGS $user_rocm_lib_flags])
        AS_IF([test "x$ac_cv_header_rocprofiler_v2_rocprofiler_h" = xyes], [
            LIBS=""
            AC_SEARCH_LIBS([rocprofiler_initialize], [rocprofiler64v2], [
                rocm_rocprofv2_plugin=yes
                ROCM_ROCPROFV2_LDFLAGS="$user_rocm_lib_flags $LIBS"
                ROCM_LIBDIR="$user_rocm_libdir"
            ])
        ])
        AS_IF([test "x$ac_cv_header_rocprofiler_sdk_rocprofiler_h" = xyes], [
            LIBS=""
            AC_SEARCH_LIBS([rocprofiler_is_initialized], [rocprofiler-sdk], [
                rocm_rocprofsdk_plugin=yes
                ROCM_ROCPROFSDK_LDFLAGS="$user_rocm_lib_flags $LIBS"
                ROCM_LIBDIR="$user_rocm_libdir"
            ])
        ])
        AX_VAR_POPVALUE([LDFLAGS])
        AX_VAR_POPVALUE([LIBS])

        AS_IF([test "x$with_rocm" != xcheck \
                && test "x$rocm_rocprofv2_plugin" != xyes \
                && test "x$rocm_rocprofsdk_plugin" != xyes], [
            AC_MSG_ERROR([Cannot find ROCm libraries])
        ])
    ])

    AC_SUBST([ROCM_CPPFLAGS])
    AC_SUBST([ROCM_LIBDIR])
    AC_SUBST([ROCM_ROCPROFV2_LDFLAGS])
    AC_SUBST([ROCM_ROCPROFSDK_LDFLAGS])
    AM_CONDITIONAL([ROCM_ROCPROFV2_PLUGIN], [test "x$rocm_rocprofv2_plugin" = xyes])
    AM_CONDITIONAL([ROCM_ROCPROFSDK_PLUGIN], [test "x$rocm_rocprofsdk_plugin" = xyes])
])

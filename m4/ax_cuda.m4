
# AX_CUDA
# --------
AC_DEFUN([AX_CUDA],
[
    AC_MSG_CHECKING([for CUDA])
    AC_ARG_WITH([cuda],
        AS_HELP_STRING([--with-cuda@<:@=DIR@:>@], [add CUDA support (requires CUPTI)]),
        [], dnl Implicit: with_cuda=$enableval
        [with_cuda=check]
    )
    AC_MSG_RESULT([$with_cuda])

    AS_IF([test "x$with_cuda" != xno], [

        # Extract directories from --with-cuda
        AS_IF([test -d "$with_cuda"], [
            AS_IF([test -d "$with_cuda/include"], [
                user_cuda_inc_flags="-I$with_cuda/include"
            ])
            AS_IF([test -d "$with_cuda/extras/CUPTI/include"], [
                user_cuda_inc_flags="$user_cuda_inc_flags -I$with_cuda/extras/CUPTI/include"
            ])
            AS_CASE([$host],
                [x86_64-*], [cuda_target_dir=x86_64-linux],
                [aarch64-*], [cuda_target_dir=aarch64-linux],
                [ppc64le-*], [cuda_target_dir=ppc64le-linux],
                []
            )
            AS_IF([test -d "$with_cuda/targets/$cuda_target_dir/include"], [
                user_cuda_inc_flags="$user_cuda_inc_flags -I$with_cuda/targets/$cuda_target_dir/include"
            ])
            AS_IF([test -d "$with_cuda/lib"], [
                user_cuda_ldflags="-L$with_cuda/lib -Wl,-rpath,$with_cuda/lib"
            ])
            AS_IF([test -d "$with_cuda/lib64"], [
                user_cuda_ldflags="$user_cuda_ldflags -L$with_cuda/lib64 -Wl,-rpath,$with_cuda/lib64"
            ])
            AS_IF([test -d "$with_cuda/extras/CUPTI/lib64"], [
                user_cuda_ldflags="$user_cuda_ldflags -L$with_cuda/extras/CUPTI/lib64 -Wl,-rpath,$with_cuda/extras/CUPTI/lib64"
            ])
        ])
    ])

    ### CUDA and CUPTI headers ###
    AS_IF([test "x$with_cuda" != xno], [
        AX_VAR_PUSHVALUE([CPPFLAGS], [$CPPFLAGS $user_cuda_inc_flags])

        AC_CHECK_HEADERS([cuda_runtime.h], [], [
            AS_IF([test "x$with_cuda" != xcheck], [
                AC_MSG_ERROR([Cannot find CUDA headers])
            ])
        ])

        AC_CHECK_HEADERS([cupti.h], [], [
            AS_IF([test "x$with_cuda" != xcheck], [
                AC_MSG_ERROR([Cannot find CUPTI headers])
            ])
        ])

        AS_IF([test "x$ac_cv_header_cuda_runtime_h" = xyes \
                && test "x$ac_cv_header_cupti_h" = xyes], [
            CUDA_CPPFLAGS="$user_cuda_inc_flags"
        ], [
            with_cuda=no
        ])

        AX_VAR_POPVALUE([CPPFLAGS])
    ])

    ### OpenACC Profiling headers are optional
    AS_IF([test "x$with_cuda" != xno], [
        AX_VAR_PUSHVALUE([CPPFLAGS], [$CPPFLAGS $user_cuda_inc_flags])
        AC_CHECK_HEADERS([acc_prof.h])
        AX_VAR_POPVALUE([CPPFLAGS])
    ])

    ### CUDA and CUPTI libraries ###
    AS_IF([test "x$with_cuda" != xno], [
        AX_VAR_PUSHVALUE([LIBS], [""])
        AX_VAR_PUSHVALUE([LDFLAGS], [$LDFLAGS $user_cuda_ldflags])

        AC_SEARCH_LIBS([cudaGetDeviceCount], [cudart], [], [
            AS_IF([test "x$with_cuda" != xcheck], [
                AC_MSG_ERROR([Cannot find CUDA libraries in $user_cuda_ldflags])
            ])
            with_cuda=no
        ])

        AC_SEARCH_LIBS([cuptiSubscribe], [cupti], [], [
            AS_IF([test "x$with_cuda" != xcheck], [
                AC_MSG_ERROR([Cannot find CUPTI libraries])
            ])
            with_cuda=no
        ])

        AS_IF([test "x$with_cuda" != xno], [
            CUDA_LDFLAGS="$user_cuda_ldflags $LIBS"
        ])

        AX_VAR_POPVALUE([LDFLAGS])
        AX_VAR_POPVALUE([LIBS])
    ])

    AC_SUBST([CUDA_CPPFLAGS])
    AC_SUBST([CUDA_LDFLAGS])
    AM_CONDITIONAL([CUPTI_PLUGIN], [test "x$with_cuda" != xno])
])

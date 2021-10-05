
# AX_MPI
# ------
AC_DEFUN([AX_MPI],
[
    AC_MSG_CHECKING([for MPI])
    AC_ARG_WITH([mpi],
        AS_HELP_STRING([--with-mpi@<:@=DIR@:>@], [build also libraries with MPI support]),
        [], dnl Implicit: with_mpi=$withvalue
        [with_mpi=no]
    )
    AC_MSG_RESULT([$with_mpi])

    AS_IF([test "x$with_mpi" != xno], [
        AS_IF([test -d "$with_mpi"], [
            AC_CHECK_SIZEOF([size_t])
            bits=$((ac_cv_sizeof_size_t * 8))
            AS_IF([test -d "$with_mpi/bin"], [user_mpi_bin="$with_mpi/bin"],
                [test -d "$with_mpi/bin$bits"], [user_mpi_bin="$with_mpi/bin$bits"])
            AS_IF([test -d "$with_mpi/include"], [user_mpi_includes="-I$with_mpi/include"],
                [test -d "$with_mpi/include$bits"], [user_mpi_includes="-I$with_mpi/include$bits"])
            AS_IF([test -d "$with_mpi/lib"], [user_mpi_libdir="-L$with_mpi/lib"],
                [test -d "$with_mpi/lib$bits"], [user_mpi_libdir="-L$with_mpi/lib$bits"])
        ])

        ### MPI Binaries, $user_mpi_bin takes preference
        AC_PATH_PROGS([MPICC], [$MPICC mpicc], [], [$user_mpi_bin])
        AS_IF([test "x$MPICC" = x], [
            AC_PATH_PROGS([MPICC], [$MPICC mpicc], [], [$PATH])
        ])
        AC_PATH_PROGS([MPIEXEC], [mpiexec mpirun], [], [$user_mpi_bin])
        AS_IF([test "x$MPIEXEC" = x], [
            AC_PATH_PROGS([MPIEXEC], [mpiexec mpirun], [], [$PATH])
        ])

        ### MPI INCLUDES ###
        AX_CHECK_MPI_CPPFLAGS([MPICC], [$user_mpi_includes],
            [],
            [AC_MSG_ERROR([Cannot find MPI headers])])

        ### MPI LIBS ###
        AX_CHECK_MPI_LDFLAGS([MPICC], [$user_mpi_libdir],
            [],
            [AC_MSG_ERROR([Cannot find MPI libraries])])

        ### MPI TESTS ###
        AC_MSG_CHECKING([whether to enable MPI test suite])
        AC_ARG_ENABLE([mpi-tests],
            AS_HELP_STRING([--enable-mpi-tests], [enable MPI tests]),
            [], dnl Implicit: enable_mpi_tests=$enableval
            [enable_mpi_tests=no]
        )
        AC_MSG_RESULT([$enable_mpi_tests])

        AS_IF([test "x$enable_mpi_tests" != xno], [
            AC_MSG_WARN([Option --enable-mpi-tests is currently not supported and has no effect.])
            AC_LANG_PUSH([C])
            AC_LANG_CONFTEST([
                AC_LANG_SOURCE([[
                    #ifndef _GNU_SOURCE
                    #define _GNU_SOURCE
                    #endif
                    #include <sched.h>
                    #include <mpi.h>
                    int main(int argc, char *argv[])
                    {
                        int rank;
                        MPI_Init(&argc, &argv);
                        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
                        cpu_set_t mask;
                        sched_getaffinity(0, sizeof(cpu_set_t), &mask) ;
                        int local_error = CPU_COUNT(&mask) != 1;
                        int global_error = 0;
                        MPI_Reduce(&local_error, &global_error, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
                        MPI_Finalize();
                        return global_error;
                    }
                ]])
            ])
            AC_LANG_POP([C])

            AC_MSG_CHECKING([for mpiexec binding options])
            MPIEXEC_BIND_OPTS="none found"
            AS_IF([test "x$MPICC" != x && {
                    $MPICC conftest.c -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2; } ], [
                AS_IF([$MPIEXEC -np 2 --bind-to-core ./conftest 2>&AS_MESSAGE_LOG_FD 1>&2],
                        [MPIEXEC_BIND_OPTS="--bind-to-core"],
                    [$MPIEXEC -np 2 --bind-to core ./conftest 2>&AS_MESSAGE_LOG_FD 1>&2],
                        [MPIEXEC_BIND_OPTS="--bind-to core"],
                    [$MPIEXEC -np 2 --bind-to hwthread ./conftest 2>&AS_MESSAGE_LOG_FD 1>&2],
                        [MPIEXEC_BIND_OPTS="--bind-to hwthread"])
            ])
            rm -f conftest
            AC_MSG_RESULT([$MPIEXEC_BIND_OPTS])

            AS_IF([test "x$MPIEXEC_BIND_OPTS" = "xnone found"], [
                AS_IF([test "x$enable_mpi_tests" != xcheck], [
                    AC_MSG_ERROR([Cannot find a suitable binding option for MPI tests])
                ], [
                    enable_mpi_tests=no
                ])
            ])
        ])

        ### MPI EXPORT ENV. VAR. ###
        AC_LANG_PUSH([C])
        AC_LANG_CONFTEST([
            AC_LANG_SOURCE([[
                #include <stdlib.h>
                #include <string.h>
                int main(int argc, char *argv[])
                {
                    char *var = getenv("CONFTEST_VARIABLE");
                    if (var && strcmp(var, "CONFTEST") == 0) return EXIT_SUCCESS;
                    return EXIT_FAILURE;
                }
            ]])
        ])
        AC_LANG_POP([C])

        AC_MSG_CHECKING([for mpiexec export flag])
        MPIEXEC_EXPORT_FLAG="none found"
        AS_IF([$MPICC conftest.c -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2], [
            AS_IF([$MPIEXEC -np 1 -x CONFTEST_VARIABLE=CONFTEST ./conftest 2>&AS_MESSAGE_LOG_FD 1>&2],
                        [MPIEXEC_EXPORT_FLAG="-x"],
                [$MPIEXEC -np 1 -genv CONFTEST_VARIABLE=CONFTEST ./conftest 2>&AS_MESSAGE_LOG_FD 1>&2],
                        [MPIEXEC_EXPORT_FLAG="-genv"])
        ])
        rm -f conftest
        AC_MSG_RESULT([$MPIEXEC_EXPORT_FLAG])

        ### MPI LIBRARY VERSION ###
        AC_LANG_PUSH([C])
        AC_LANG_CONFTEST([
            AC_LANG_SOURCE([[
                #include <mpi.h>
                #include <stdio.h>
                #include <stdlib.h>
                int main(int argc, char *argv[])
                {
                    char version[MPI_MAX_LIBRARY_VERSION_STRING];
                    int resultlen;
                    MPI_Get_library_version(version, &resultlen);
                    printf("%s", version);
                    return EXIT_SUCCESS;
                }
            ]])
        ])
        AC_LANG_POP([C])

        AC_MSG_CHECKING([for MPI library version])
        mpi_library_version=""
        AS_IF([$MPICC conftest.c -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2], [
            mpi_library_version="\"$(./conftest)\""
        ])
        rm -f conftest
        AC_MSG_RESULT([$mpi_library_version])
    ])

    AC_SUBST([MPICC])
    AC_SUBST([MPIEXEC])
    AC_SUBST([MPIEXEC_BIND_OPTS])
    AC_SUBST([MPIEXEC_EXPORT_FLAG])
    AM_CONDITIONAL([MPI_LIB], [test "x$with_mpi" != xno])
    AM_CONDITIONAL([MPI_TESTS], [test "x$enable_mpi_tests" != xno])
    AC_DEFINE_UNQUOTED([MPI_LIBRARY_VERSION], [$mpi_library_version], [MPI Library version])
])

# AX_CHECK_MPI_CPPFLAGS([MPICC], [FLAGS], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# ---------------------------------------------------------------------------------
AC_DEFUN([AX_CHECK_MPI_CPPFLAGS],
[
    # MPICC -show checks take priority over custom flags
    AS_IF([test -f $$1], [
        AS_IF([mpicc_showme_compile=$($$1 -showme:compile 2>&AS_MESSAGE_LOG_FD)], [
            AX_VAR_PUSHVALUE([CPPFLAGS], [$mpicc_showme_compile])
            AC_CHECK_HEADERS([mpi.h], [MPI_CPPFLAGS="$mpicc_showme_compile"])
            AX_VAR_POPVALUE([CPPFLAGS])
        ])
        AS_IF([test "x$ac_cv_header_mpi_h" != xyes], [
            AS_IF([mpicc_show_c=$($$1 -show -c 2>&AS_MESSAGE_LOG_FD)], [
                mpicc_show_c=$(echo $mpicc_show_c | tr -s ' ' | cut -d' ' -f3-)
                AX_VAR_PUSHVALUE([CPPFLAGS], [$mpicc_show_c])
                AS_UNSET([ac_cv_header_mpi_h])
                AC_CHECK_HEADERS([mpi.h], [MPI_CPPFLAGS="$mpicc_show_c"])
                AX_VAR_POPVALUE([CPPFLAGS])
            ])
        ])
    ])
    # If MPICC wrapper did not succeed, try user custom flags
    AS_IF([test "x$ac_cv_header_mpi_h" != xyes], [
        AX_VAR_PUSHVALUE([CPPFLAGS], [$2])
        AS_UNSET([ac_cv_header_mpi_h])
        AC_CHECK_HEADERS([mpi.h], [MPI_CPPFLAGS="$2"])
        AX_VAR_POPVALUE([CPPFLAGS])
    ])

    AS_IF([test "x$ac_cv_header_mpi_h" != xyes], [
        # ACTION-IF-NOT-FOUND
        :
        $4
    ] , [
        # ACTION-IF-FOUND
        :
        $3
    ])
    AC_SUBST([MPI_CPPFLAGS])
])


# AX_CHECK_MPI_LDFLAGS([MPICC], [FLAGS], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# --------------------------------------------------------------------------------
AC_DEFUN([AX_CHECK_MPI_LDFLAGS],
[
    # LIBS is also pushed/popped because AC_SEARCH_LIBS adds
    # the corresponding link flag and we do not want that

    # MPICC -show checks take priority over custom flags
    AS_IF([test -f $$1], [
        AS_IF([mpicc_showme_link=$($$1 -showme:link 2>&AS_MESSAGE_LOG_FD)], [
            AX_VAR_PUSHVALUE([LIBS], [""])
            AX_VAR_PUSHVALUE([LDFLAGS], [$mpicc_showme_link])
            AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$mpicc_showme_link"])
            AX_VAR_POPVALUE([LDFLAGS])
            AX_VAR_POPVALUE([LIBS])
        ])
        AS_IF([test "x$ac_cv_search_MPI_Init" = x || test "x$ac_cv_search_MPI_Init" = xno], [
            AS_IF([mpicc_link_info=$($$1 -link_info 2>&AS_MESSAGE_LOG_FD)], [
                mpicc_link_info=$(echo $mpicc_link_info | tr -s ' ' | cut -d' ' -f2-)
                AX_VAR_PUSHVALUE([LIBS], [""])
                AX_VAR_PUSHVALUE([LDFLAGS], [$mpicc_link_info])
                AS_UNSET([ac_cv_search_MPI_Init])
                AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$mpicc_link_info"])
                AX_VAR_POPVALUE([LDFLAGS])
                AX_VAR_POPVALUE([LIBS])
            ])
        ])
        AS_IF([test "x$ac_cv_search_MPI_Init" = x || test "x$ac_cv_search_MPI_Init" = xno], [
            AS_IF([mpicc_show=$($$1 -show 2>&AS_MESSAGE_LOG_FD)], [
                mpicc_show=$(echo $mpicc_show | tr -s ' ' | cut -d' ' -f2-)
                AX_VAR_PUSHVALUE([LIBS], [""])
                AX_VAR_PUSHVALUE([LDFLAGS], [$mpicc_show])
                AS_UNSET([ac_cv_search_MPI_Init])
                AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$mpicc_show"])
                AX_VAR_POPVALUE([LDFLAGS])
                AX_VAR_POPVALUE([LIBS])
            ])
        ])
    ])
    # If MPICC wrapper did not succeed, try user custom flags
    AS_IF([test "x$ac_cv_search_MPI_Init" = x || test "x$ac_cv_search_MPI_Init" = xno], [
        AX_VAR_PUSHVALUE([LIBS], [""])
        AX_VAR_PUSHVALUE([LDFLAGS], [$2])
        AS_UNSET([ac_cv_search_MPI_Init])
        AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$2 $LIBS"])
        AX_VAR_POPVALUE([LDFLAGS])
        AX_VAR_POPVALUE([LIBS])
    ])

    AS_IF([test "x$MPI_LDFLAGS" = x], [
        # ACTION-IF-NOT-FOUND
        :
        $4
    ] , [
        # ACTION-IF-FOUND
        :
        $3
    ])
    AC_SUBST([MPI_LDFLAGS])
])

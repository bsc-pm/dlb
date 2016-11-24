
# AX_MPI
# ------
AC_DEFUN([AX_MPI],
[
    AC_MSG_CHECKING([for MPI])
    AC_ARG_WITH([mpi],
        AC_HELP_STRING([--with-mpi@<:@=DIR@:>@], [build also libraries with MPI support]),
        [], dnl Implicit: with_mpi=$withvalue
        [with_mpi=no]
    )
    AC_MSG_RESULT([$with_mpi])

    AS_IF([test "x$with_mpi" != xno], [
        AS_IF([test -d "$with_mpi"], [
            AC_CHECK_SIZEOF([size_t])
            AS_VAR_ARITH([bits], [ac_cv_sizeof_size_t \* 8])
            AS_IF([test -d "$with_mpi/bin"], [user_mpi_bin="$with_mpi/bin"],
                [test -d "$with_mpi/bin$bits"], [user_mpi_bin="$with_mpi/bin$bits"])
            AS_IF([test -d "$with_mpi/include"], [user_mpi_includes="-I$with_mpi/include"],
                [test -d "$with_mpi/include$bits"], [user_mpi_includes="-I$with_mpi/include$bits"])
            AS_IF([test -d "$with_mpi/lib"], [user_mpi_libdir="-L$with_mpi/lib"],
                [test -d "$with_mpi/lib$bits"], [user_mpi_libdir="-L$with_mpi/lib$bits"])
        ])

        AC_PATH_PROG([MPICC], [mpicc], [], [$user_mpi_bin$PATH_SEPARATOR$PATH])
        AC_PATH_PROGS([MPIEXEC], [mpiexec mpirun], [], [$user_mpi_bin$PATH_SEPARATOR$PATH])

        ### MPI INCLUDES ###
        AS_IF([showme_compile=$($MPICC -showme:compile 2>&AS_MESSAGE_LOG_FD)], [
            AX_VAR_PUSHVALUE([CPPFLAGS], [$showme_compile])
            AC_CHECK_HEADERS([mpi.h], [MPI_CPPFLAGS="$showme_compile" ; mpi_h=yes])
            AX_VAR_POPVALUE([CPPFLAGS])
        ])
        AS_IF([test "x$mpi_h" != xyes], [
            AX_VAR_PUSHVALUE([CPPFLAGS], [$user_mpi_includes])
            AS_UNSET([ac_cv_header_mpi_h])
            AC_CHECK_HEADERS([mpi.h], [MPI_CPPFLAGS="$user_mpi_includes" ; mpi_h=yes])
            AX_VAR_POPVALUE([CPPFLAGS])
        ])
        AS_IF([test "x$mpi_h" != xyes], [AC_MSG_ERROR([Cannot find MPI headers])])

        ### MPI LIBS ###
        AS_IF([showme_link=$($MPICC -showme:link 2>&AS_MESSAGE_LOG_FD)], [
            AX_VAR_PUSHVALUE([LDFLAGS], [$showme_link])
            AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$showme_link" ; mpi_lib=yes])
            AX_VAR_POPVALUE([LDFLAGS])
        ])
        AS_IF([test "x$mpi_lib" != xyes], [
            AX_VAR_PUSHVALUE([LIBS], [""])
            AX_VAR_PUSHVALUE([LDFLAGS], [$user_mpi_libdir])
            AS_UNSET([ac_cv_search_MPI_Init])
            AC_SEARCH_LIBS([MPI_Init], [mpi mpich],
                [MPI_LDFLAGS="$user_mpi_libdir $LIBS" ; mpi_lib=yes])
            AX_VAR_POPVALUE([LDFLAGS])
            AX_VAR_POPVALUE([LIBS])
        ])
        AS_IF([test "x$mpi_lib" != xyes], [AC_MSG_ERROR([Cannot find MPI libraries])])

        ### MPI TESTS ###
        AC_MSG_CHECKING([whether to enable MPI test suite])
        AC_ARG_ENABLE([mpi-tests],
            AS_HELP_STRING([--disable-mpi-tests], [Disable MPI tests]),
            [], dnl Implicit: enable_mpi_tests=$enableval
            [enable_mpi_tests=check]
        )
        AC_MSG_RESULT([$enable_mpi_tests])

        AS_IF([test "x$enable_mpi_tests" != xno], [
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

            AC_MSG_CHECKING([for mpirun binding options])
            MPIEXEC_BIND_OPTS="none found"
            AS_IF([test "x$MPICC" != x && $MPICC conftest.c -o conftest], [
                AS_IF([$MPIEXEC -n 2 --bind-to-core ./conftest 2>&AS_MESSAGE_LOG_FD] 1>&2,
                        [MPIEXEC_BIND_OPTS="--bind-to-core"],
                    [$MPIEXEC -n 2 --bind-to core ./conftest 2>&AS_MESSAGE_LOG_FD] 1>&2,
                        [MPIEXEC_BIND_OPTS="--bind-to core"],
                    [$MPIEXEC -n 2 --bind-to hwthread ./conftest 2>&AS_MESSAGE_LOG_FD] 1>&2,
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
    ])

    AC_SUBST([MPICC])
    AC_SUBST([MPIEXEC])
    AC_SUBST([MPI_CPPFLAGS])
    AC_SUBST([MPI_LDFLAGS])
    AC_SUBST([MPIEXEC_BIND_OPTS])
    AM_CONDITIONAL([MPI_LIB], [test "x$with_mpi" != no])
    AM_CONDITIONAL([IGNORE_MPI_TESTS], [test "x$enable_mpi_tests" != no])
])

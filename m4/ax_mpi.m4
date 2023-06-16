
# AX_MPI
# ------
AC_DEFUN([AX_MPI],
[
    AC_MSG_CHECKING([for MPI])
    AC_ARG_WITH([mpi],
        AS_HELP_STRING([--with-mpi@<:@=DIR@:>@], [build libdlb_mpi.so, a library to intercept both C and Fortran MPI]),
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
        AC_PATH_PROGS([MPIFC], [$MPIFC mpif90 mpifort mpifc], [], [$user_mpi_bin])
        AS_IF([test "x$MPIFC" = x], [
            AC_PATH_PROGS([MPIFC], [$MPIFC mpif90 mpifort mpifc], [], [$PATH])
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

        ### MPI TESTS  ###
        # deprecated, keep variable in case we want to re-do them, but don't show option in configure
        enable_mpi_tests=no
        dnl AC_MSG_CHECKING([whether to enable MPI test suite])
        dnl AC_ARG_ENABLE([mpi-tests],
        dnl     AS_HELP_STRING([--enable-mpi-tests], [enable MPI tests]),
        dnl     [], dnl Implicit: enable_mpi_tests=$enableval
        dnl     [enable_mpi_tests=no]
        dnl )
        dnl AC_MSG_RESULT([$enable_mpi_tests])

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

        ### MPI VERSION ###
        AC_LANG_PUSH([C])
        AC_LANG_CONFTEST([
            AC_LANG_SOURCE([[
                #include <mpi.h>
                #include <stdio.h>
                #include <stdlib.h>
                int main(int argc, char *argv[])
                {
                    printf("%d.%d\n", MPI_VERSION, MPI_SUBVERSION);
                    return EXIT_SUCCESS;
                }
            ]])
        ])
        AC_LANG_POP([C])

        AC_MSG_CHECKING([for MPI version])
        MPI_VERSION=""
        AS_IF([$MPICC conftest.c -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2], [
            MPI_VERSION="$(./conftest)"
        ])
        rm -f conftest
        AC_MSG_RESULT([$MPI_VERSION])
        AX_COMPARE_VERSION([$MPI_VERSION], [ge], [3.0], [mpi3=yes], [mpi3=no])

        ### MPI LIBRARY VERSION ###
        AC_LANG_PUSH([C])
        AC_LANG_CONFTEST([
            AC_LANG_SOURCE([[
                #include <mpi.h>
                #include <stdio.h>
                #include <stdlib.h>
                #include <string.h>
                int main(int argc, char *argv[])
                {
                    char version[MPI_MAX_LIBRARY_VERSION_STRING];
                    int resultlen;
                    MPI_Get_library_version(version, &resultlen);
                    char *newline = strchr(version, '\n');
                    if (newline) *newline = '\0';
                    printf("%s", version);
                    return EXIT_SUCCESS;
                }
            ]])
        ])
        AC_LANG_POP([C])

        AC_MSG_CHECKING([for MPI library version])
        MPI_LIBRARY_VERSION=""
        AS_IF([$MPICC conftest.c -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2], [
            MPI_LIBRARY_VERSION="$(./conftest)"
        ])
        rm -f conftest
        AC_MSG_RESULT([$MPI_LIBRARY_VERSION])

        ### Check for MPI_OFFSET_KIND
        AS_IF([test x"$FC" != x && test x"$mpi3" = xyes], [
            AC_MSG_CHECKING([for MPI_OFFSET_KIND])
            AC_LANG_PUSH([Fortran])
            AC_LANG_CONFTEST([
                AC_LANG_PROGRAM([], [[
                    include 'mpif.h'
                    write(*, '(I0)')  MPI_OFFSET_KIND
                ]])
            ])
            AC_LANG_POP([Fortran])
            AS_IF([$MPIFC $FCFLAGS conftest.f -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2], [
                mpi_offset_kind="$(./conftest)"
            ])
            rm -f conftest
            AC_MSG_RESULT([$mpi_offset_kind])
            AC_DEFINE_UNQUOTED([MPI_OFFSET_KIND], [$mpi_offset_kind], [MPI Fortran MPI_OFFSET_KIND])
        ])

        ### Check for MPI_ADDRESS_KIND
        AS_IF([test x"$FC" != x && test x"$mpi3" = xyes], [
            AC_MSG_CHECKING([for MPI_ADDRESS_KIND])
            AC_LANG_PUSH([Fortran])
            AC_LANG_CONFTEST([
                AC_LANG_PROGRAM([], [[
                    include 'mpif.h'
                    write(*, '(I0)')  MPI_ADDRESS_KIND
                ]])
            ])
            AC_LANG_POP([Fortran])
            AS_IF([$MPIFC $FCFLAGS conftest.f -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2], [
                mpi_address_kind="$(./conftest)"
            ])
            rm -f conftest
            AC_MSG_RESULT([$mpi_address_kind])
            AC_DEFINE_UNQUOTED([MPI_ADDRESS_KIND], [$mpi_address_kind], [MPI Fortran MPI_ADDRESS_KIND])
        ])

        ### Check for MPI_COUNT_KIND
        AS_IF([test x"$FC" != x && test x"$mpi3" = xyes], [
            AC_MSG_CHECKING([for MPI_COUNT_KIND])
            AC_LANG_PUSH([Fortran])
            AC_LANG_CONFTEST([
                AC_LANG_PROGRAM([], [[
                    include 'mpif.h'
                    write(*, '(I0)')  MPI_COUNT_KIND
                ]])
            ])
            AC_LANG_POP([Fortran])
            AS_IF([$MPIFC $FCFLAGS conftest.f -o conftest 2>&AS_MESSAGE_LOG_FD 1>&2], [
                mpi_count_kind="$(./conftest)"
            ])
            rm -f conftest
            AC_MSG_RESULT([$mpi_count_kind])
            AC_DEFINE_UNQUOTED([MPI_COUNT_KIND], [$mpi_count_kind], [MPI Fortran MPI_COUNT_KIND])
        ])

        ### Check for specific C MPI library
        AC_MSG_CHECKING([whether to compile libdlb_mpic.so, containing only C MPI symbols])
        AC_ARG_ENABLE([c-mpi-library],
            AS_HELP_STRING([--enable-c-mpi-library], [compile also a DLB MPI library specific for C]),
            [], dnl Implicit: enable_c_mpi_library=$enableval
            [enable_c_mpi_library=no]
        )
        AC_MSG_RESULT([$enable_c_mpi_library])
        #
        ### Check for specific Fortran MPI library
        AC_MSG_CHECKING([whether to compile libdlb_mpif.so, containing only Fortran MPI symbols])
        AC_ARG_ENABLE([fortran-mpi-library],
            AS_HELP_STRING([--enable-fortran-mpi-library], [compile also a DLB MPI library specific for Fortran]),
            [], dnl Implicit: enable_fortran_mpi_library=$enableval
            [enable_fortran_mpi_library=no]
        )
        AC_MSG_RESULT([$enable_fortran_mpi_library])

        ### MPI Fortran 08 bindings ###
        AC_MSG_CHECKING([whether to compile MPI Fortran 2008 interface])
        AS_IF([test x"$FC" != x \
                && test x"$mpi3" = xyes \
                && test x"$mpi_offset_kind" != x \
                && test x"$mpi_address_kind" != x \
                && test x"$mpi_count_kind" != x \
                && test x"$fc_ignore_type" != x], [
            mpi_f08=yes
        ], [
            mpi_f08=no
        ])
        AC_MSG_RESULT([$mpi_f08])
    ])

    AC_SUBST([MPICC])
    AC_SUBST([MPIEXEC])
    AC_SUBST([MPIEXEC_BIND_OPTS])
    AC_SUBST([MPI_VERSION])
    AC_SUBST([MPI_LIBRARY_VERSION])
    AM_CONDITIONAL([MPI_LIB], [test "x$with_mpi" != xno])
    AM_CONDITIONAL([MPIC_LIB], [test "x$enable_c_mpi_library" != xno])
    AM_CONDITIONAL([MPIF_LIB], [test "x$enable_fortran_mpi_library" != xno])
    AM_CONDITIONAL([MPI_TESTS], [test "x$enable_mpi_tests" != xno])
    AM_CONDITIONAL([MPI_F08], [test x"$mpi_f08" = xyes])
    AM_CONDITIONAL([HAVE_MPI_VERSION], [test "x$MPI_VERSION" != x])
    AM_CONDITIONAL([HAVE_MPI_LIBRARY_VERSION], [test "x$MPI_LIBRARY_VERSION" != x])
    AC_DEFINE_UNQUOTED([MPI_LIBRARY_VERSION], ["$MPI_LIBRARY_VERSION"], [MPI Library version])
])

# AX_CHECK_MPI_CPPFLAGS([MPICC], [FLAGS], [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
# ---------------------------------------------------------------------------------
AC_DEFUN([AX_CHECK_MPI_CPPFLAGS],
[
    # MPICC -show checks take priority over custom flags
    AS_IF([test -f $$1], [
        # OpenMPI
        AS_IF([mpicc_showme_compile=$($$1 -showme:compile 2>&AS_MESSAGE_LOG_FD)], [
            AX_VAR_PUSHVALUE([CPPFLAGS], [$mpicc_showme_compile])
            AC_CHECK_HEADERS([mpi.h], [MPI_CPPFLAGS="$mpicc_showme_compile"])
            AX_VAR_POPVALUE([CPPFLAGS])
        ])
	# Generic flag
        AS_IF([test "x$ac_cv_header_mpi_h" != xyes], [
            AS_IF([mpicc_show_c=$($$1 -show -c 2>&AS_MESSAGE_LOG_FD)], [
                mpicc_show_c=$(echo $mpicc_show_c | tr -s ' ' | cut -d' ' -f3-)
                AX_VAR_PUSHVALUE([CPPFLAGS], [$mpicc_show_c])
                AS_UNSET([ac_cv_header_mpi_h])
                AC_CHECK_HEADERS([mpi.h], [MPI_CPPFLAGS="$mpicc_show_c"])
                AX_VAR_POPVALUE([CPPFLAGS])
            ])
        ])
        # Generic flag with quoted paths
        AS_IF([test "x$ac_cv_header_mpi_h" != xyes], [
            AS_IF([mpicc_show_c=$($$1 -show -c 2>&AS_MESSAGE_LOG_FD)], [
                mpicc_show_c=$(echo $mpicc_show_c | tr -s ' ' | cut -d' ' -f3- | sed 's|"||g')
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
        # OpenMPI
        AS_IF([mpicc_showme_link=$($$1 -showme:link 2>&AS_MESSAGE_LOG_FD)], [
            AX_VAR_PUSHVALUE([LIBS], [""])
            AX_VAR_PUSHVALUE([LDFLAGS], [$mpicc_showme_link])
            AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$mpicc_showme_link"])
            AX_VAR_POPVALUE([LDFLAGS])
            AX_VAR_POPVALUE([LIBS])
        ])
        # MPICH
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
        # Intel oneAPI quoted paths
        AS_IF([test "x$ac_cv_search_MPI_Init" = x || test "x$ac_cv_search_MPI_Init" = xno], [
            AS_IF([mpicc_link_info=$($$1 -link_info 2>&AS_MESSAGE_LOG_FD)], [
                mpicc_link_info=$(echo $mpicc_link_info | tr -s ' ' | cut -d' ' -f2- | sed 's|"||g')
                AX_VAR_PUSHVALUE([LIBS], [""])
                AX_VAR_PUSHVALUE([LDFLAGS], [$mpicc_link_info])
                AS_UNSET([ac_cv_search_MPI_Init])
                AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$mpicc_link_info"])
                AX_VAR_POPVALUE([LDFLAGS])
                AX_VAR_POPVALUE([LIBS])
            ])
        ])
	# Generic flag
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
        # Generic flag with quoted paths
        AS_IF([test "x$ac_cv_search_MPI_Init" = x || test "x$ac_cv_search_MPI_Init" = xno], [
            AS_IF([mpicc_show=$($$1 -show 2>&AS_MESSAGE_LOG_FD)], [
                mpicc_show=$(echo $mpicc_show | tr -s ' ' | cut -d' ' -f2- | sed 's|"||g')
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
        AS_IF([test x"$cross_compiling" = xyes ], [LDFLAGS="$LDFLAGS -Wl,-z,undefs"], [])
        AS_UNSET([ac_cv_search_MPI_Init])
        AC_SEARCH_LIBS([MPI_Init], [mpi mpich], [MPI_LDFLAGS="$LDFLAGS $LIBS"])
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

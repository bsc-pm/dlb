# AX_PROG_MPI
# -----------
AC_DEFUN([AX_PROG_MPI],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH([mpi],
      AC_HELP_STRING(
         [--with-mpi@<:@=DIR@:>@],
         [specify where to find MPI libraries and includes]
      ),
      [mpi_paths=${withval}],
      [mpi_paths="not_set"]
   )

   if test ${mpi_paths} != "no" -a ${mpi_paths} != "not_set" -a ! -d ${mpi_paths} ; then
      AC_MSG_ERROR([Attention! You have passed an invalid MPI location.])
   fi

   # Search for MPI installation
   AX_FIND_INSTALLATION([MPI], [$mpi_paths], [mpi])

   if test "${MPI_INSTALLED}" = "yes" ; then

      if test -d "$MPI_INCLUDES/mpi" ; then
         MPI_INCLUDES="$MPI_INCLUDES/mpi"
         MPI_CFLAGS="-I$MPI_INCLUDES"
         CFLAGS="$MPI_CFLAGS $CFLAGS"
      fi

      # Check for the MPI header files.
      AC_CHECK_HEADERS([mpi.h], [], [MPI_INSTALLED="no"])

      if test ${MPI_INSTALLED} = "no" ; then
         AC_MSG_ERROR([Cannot find mpi.h file in the MPI specified path])
      fi

      # Check for MPICH2
      AX_CHECK_DEFINED([mpi.h], [MPICH2], [MPI_IS_MPICH2="yes"], [MPI_IS_MPICH2="no"])

      # Check for the MPI library.
      # We won't use neither AC_CHECK_LIB nor AC_TRY_LINK because this library may have unresolved references
      AC_MSG_CHECKING([for MPI library])
      if test -f "${MPI_LIBSDIR}/libmpi_mt.so" ; then
         MPI_LIBS="-lmpi_mt"
      elif test -f "${MPI_LIBSDIR}/libmpi.so" ; then
         MPI_LIBS="-lmpi"
      elif test -f "${MPI_LIBSDIR}/libmpich-gcc.legacy.a"; then
         MPI_LIBS="-lmpich-gcc.legacy"
      elif test -f "${MPI_LIBSDIR}/libmpich-xl.legacy.a"; then
         MPI_LIBS="-lmpich-xl.legacy"
      elif test -f "${MPI_LIBSDIR}/libmpich_intel.so"; then
         MPI_LIBS="-lmpich_intel"
      elif test -f "${MPI_LIBSDIR}/libmpich.a" -o -f "${MPI_LIBSDIR}/libmpich.so" -o -f "${MPI_LIBSDIR}/shared/libmpich.so" ; then
         MPI_LIBS="-lmpich"
      elif test -f "${MPI_LIBSDIR}/libmpi_ibm.so" ; then
         MPI_LIBS="-lmpi_ibm"
      dnl Specific for BG/P machine
      elif test -f "${MPI_LIBSDIR}/libmpich.cnk.a" ; then
         MPI_LIBS="-lmpich.cnk"
      else
         MPI_LIBS="not found"
      fi
      AC_MSG_RESULT([${MPI_LIBSDIR}, ${MPI_LIBS}])

      if test "${MPI_LIBS}" = "not found" ; then
         AC_MSG_ERROR([Cannot find MPI libraries file in the MPI specified path])
      fi

      AC_SUBST([MPI_LDFLAGS])
      AC_SUBST([MPI_LIBS])

   fi

   if test "${MPI_INSTALLED}" = "yes" ; then
      AC_DEFINE([HAVE_MPI], [1], [Determine if MPI in installed])
   fi

   AX_FLAGS_RESTORE()
])

# AX_PROG_MPITESTS
# -----------
AC_DEFUN([AX_PROG_MPITESTS],
[
    AC_REQUIRE([AX_PROG_MPI])
    AX_FLAGS_SAVE()

    AC_MSG_CHECKING([whether to enable MPI test suite])
    AC_ARG_ENABLE([mpi-tests],
        AS_HELP_STRING([--disable-mpi-tests], [Disable MPI tests]),
        [ mpi_tests="$enableval" ],
        [ mpi_tests=yes ]
    )
    AC_MSG_RESULT([$mpi_tests])

    AS_IF([ test $mpi_tests = yes ],
    [
        AS_IF([test -d "${MPI_HOME}/bin"], [MPI_BINS="${MPI_HOME}/bin"],
            [test -d "${MPI_HOME}/bin${BITS}"], [MPI_BINS="${MPI_HOME}/bin${BITS}"],
            [MPI_BINS="not found"]
        )

        AC_MSG_CHECKING([for mpicc])
        AS_IF([test -f "${MPI_BINS}/mpigcc"], [MPICC="${MPI_BINS}/mpigcc"],
            [test -f "${MPI_BINS}/mpicc"], [MPICC="${MPI_BINS}/mpicc"],
            [MPICC="not found"]
        )
        AC_MSG_RESULT([${MPICC}])

        AC_MSG_CHECKING([for mpirun])
        AS_IF([test -f "${MPI_BINS}/mpirun"], [MPIRUN="${MPI_BINS}/mpirun"],
            [test -f "${MPI_BINS}/mpiexec"], [MPIRUN="${MPI_BINS}/mpiexec"],
            [MPIRUN="not found"]
        )
        AC_MSG_RESULT([${MPIRUN}])

        AS_IF([test "$MPICC" = "not found" -o "$MPIRUN" = "not found"],
        [
            AC_MSG_NOTICE([Cannot find MPI execution environment. Disabling MPI test suite.])
            mpi_tests=no
        ])
    ])

    # Check again once MPICC and MPIRUN are valid
    AS_IF([ test $mpi_tests = yes ],
    [
        AC_LANG_PUSH([C])
        AC_LANG_CONFTEST(
        [
            AC_LANG_SOURCE([[
                #ifndef _GNU_SOURCE
                #define _GNU_SOURCE
                #endif
                #include <sched.h>
                #include <mpi.h>
                int main(int argc, char *argv[])
                {
                    int rank;
                    MPI_Init( &argc, &argv );
                    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
                    cpu_set_t mask;
                    sched_getaffinity( 0, sizeof(cpu_set_t), &mask) ;
                    int error = CPU_COUNT( &mask ) != 1;
                    MPI_Reduce(&error, &error, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD );
                    MPI_Finalize();
                    return error;
                }
            ]])
        ])
        AC_LANG_POP([C])

        AC_MSG_CHECKING([for mpirun binding options])
        AS_IF([${MPICC} conftest.c -o conftest],
        [
            AS_IF([${MPIRUN} -n 2 --bind-to-core ./conftest 2>&AS_MESSAGE_LOG_FD],
                    [MPIRUN_BIND="--bind-to-core"],
                [${MPIRUN} -n 2 --bind-to core ./conftest 2>&AS_MESSAGE_LOG_FD],
                    [MPIRUN_BIND="--bind-to core"],
                [MPIRUN_BIND="none found"]
            )
        ])
        rm -f conftest
        AC_MSG_RESULT([${MPIRUN_BIND}])

        AS_IF([test "$MPIRUN_BIND" = "none found"],
        [
            AC_MSG_NOTICE([Cannot find a suitable binding option for MPI. Disabling MPI test suite.])
            mpi_tests=no
        ])
    ])

    AC_SUBST([MPICC])
    AC_SUBST([MPIRUN])
    AC_SUBST([MPIRUN_BIND])

   AX_FLAGS_RESTORE()
])

# AX_PROG_HWLOC
# -----------
AC_DEFUN([AX_PROG_HWLOC],
[
   AX_FLAGS_SAVE()

   AC_ARG_WITH([hwloc],
      AC_HELP_STRING(
         [--with-hwloc@<:@=DIR@:>@],
         [specify where to find HWLOC libraries and includes]
      ),
      [hwloc_paths=${withval}],
      [hwloc_paths="not_set"]
   )

   if test ${hwloc_paths} != "no" -a ${hwloc_paths} != "not_set" -a ! -d ${hwloc_paths} ; then
      AC_MSG_ERROR([Attention! You have passed an invalid HWLOC location.])
   fi

   # Search for HWLOC installation
   AX_FIND_INSTALLATION([HWLOC], [$hwloc_paths], [hwloc])

   if test "${HWLOC_INSTALLED}" = "yes" ; then

      # Check for the HWLOC header files.
      AC_CHECK_HEADERS([hwloc.h], [], [HWLOC_INSTALLED="no"])

      if test ${HWLOC_INSTALLED} = "no" ; then
         AC_MSG_ERROR([Cannot find hwloc.h file in the HWLOC specified path])
      fi

      # Check for the HWLOC library.
      AC_MSG_CHECKING([for HWLOC library])
      if test -f "${HWLOC_LIBSDIR}/libhwloc.a" -o -f "${HWLOC_LIBSDIR}/libhwloc.so" ; then
         HWLOC_LIBS="-lhwloc"
      else
         HWLOC_LIBS="not found"
      fi
      AC_MSG_RESULT([${HWLOC_LIBSDIR}, ${HWLOC_LIBS}])

      if test "${HWLOC_LIBS}" = "not found" ; then
         AC_MSG_ERROR([Cannot find HWLOC libraries file in the HWLOC specified path])
      fi

      AC_SUBST([HWLOC_LDFLAGS])
      AC_SUBST([HWLOC_LIBS])

   fi

   # Did the checks pass?
   AM_CONDITIONAL([HAVE_HWLOC], [test "${HWLOC_INSTALLED}" = "yes"])

   if test "${HWLOC_INSTALLED}" = "yes" ; then
      AC_DEFINE([HAVE_HWLOC], [1], [Determine if HWLOC in installed])
   fi

   AX_FLAGS_RESTORE()
])


# AX_PROG_LSCPU
# -----------
AC_DEFUN([AX_PROG_LSCPU],
[
   AC_REQUIRE([AX_PROG_HWLOC])
   AC_CHECK_PROG([lscpu], [lscpu], [yes], [no])
   if test ${HWLOC_INSTALLED} = "no" -a "$lscpu" = "no" ; then
      AC_MSG_ERROR([neither lscpu nor hwloc were found, one of them is mandatory])
   fi
])


# AX_SHOW_CONFIGURATION
# ----------
AC_DEFUN([AX_SHOW_CONFIGURATION],
[
   echo " *" $1 CONFIGURED: "$$1_INSTALLED"
   if test "$$1_INSTALLED" = "yes" ; then
            echo -e \\\t$1 home:      "$$1_HOME"
            echo -e \\\t$1 includes:  "$$1_INCLUDES"
            echo -e \\\t$1 libs:      "$$1_LIBSDIR, $$1_LIBS"
   fi
])

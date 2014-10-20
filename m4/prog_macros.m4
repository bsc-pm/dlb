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
      if test -f "${MPI_LIBSDIR}/libmpi.a" ; then
         MPI_LIBS="-lmpi"
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

   # Did the checks pass?
   AM_CONDITIONAL([HAVE_MPI], [test "${MPI_INSTALLED}" = "yes"])

   if test "${MPI_INSTALLED}" = "yes" ; then
      AC_DEFINE([HAVE_MPI], [1], [Determine if MPI in installed])
   fi

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

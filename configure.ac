
AC_INIT([DLB],[2.0],[pm-tools@bsc.es])
AC_CONFIG_SRCDIR([src/dlb/dlb.c])

AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_MACRO_DIR([m4])

# Enable silent rules if available
supported_silent_rules=1
m4_ifdef([AM_SILENT_RULES],
         [AM_SILENT_RULES([yes])],
         [supported_silent_rules=0])

AM_CONDITIONAL([SUPPORTED_SILENT_RULES], test x$supported_silent_rules = x1)

# Set compiler default flags
: ${CFLAGS=""}
: ${CXXFLAGS=""}
: ${FCFLAGS=""}

# Checks for programs
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT
AC_PROG_CC([$CC])
AC_PROG_CPP

# use the C compiler for the following checks
AC_LANG([C])

# check for OpenMP availability
AC_OPENMP

# check for MPI path
AC_ARG_WITH([mpi],
   AS_HELP_STRING([--with-mpi=dir], [Directory of MPI installation]),
   [
      mpi=yes
      MPI_HOME="$withval"
   ],
   [
      AC_CHECK_HEADERS([mpi.h], [mpi=yes], [mpi=no])
   ]
)

# check for HWLOC path
AC_ARG_WITH([hwloc],
   AS_HELP_STRING([--with-hwloc=dir], [Directory of HWLOC installation]),
   [
      hwloc=yes
      HWLOC_HOME="$withval"
   ],
   [
      AC_CHECK_HEADERS([hwloc.h], [hwloc=yes], [hwloc=no])
   ]
)

# check for number of cpus on node
AC_ARG_WITH([cpus-per-node],
   AS_HELP_STRING([--with-cpus-per-node=N], [Number of cpus per node]),
   [
      ncpus="$withval"
   ],
   [
      ncpus=$(nproc)
   ]
)

# Checks for header files.
#AC_HEADER_STDC
#AC_CHECK_HEADERS([string])
#AC_CHECK_HEADERS([iostream])

AC_CONFIG_HEADERS([config.h])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([ Makefile
                  src/Makefile
                  scripts/Makefile
                  tests/Makefile
                  tests/gens/Makefile
               ])


##########################################################################
# debug compilation support
##########################################################################

AC_MSG_CHECKING([whether to build with debug information])
AC_ARG_ENABLE([debug],
   AS_HELP_STRING([--enable-debug], [enable debug data generation [default=no]]),
   [ debug="$enableval" ],
   [ debug=no ]
)
AC_MSG_RESULT([$debug])

if test x"$debug" = x"yes"; then
    DEBUG_CFLAGS="-g -O0 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
    DEBUG_CPPFLAGS="-DCPUS_NODE=$ncpus -DdebugBasicInfo -DdebugBlockingMPI -DdebugLend -DdebugSharedMem -DENABLE_DEBUG"
fi
AM_CONDITIONAL([DEBUG_LIB], [test x"$debug" = x"yes"])

PERFO_CFLAGS="-g -O3 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
PERFO_CPPFLAGS="-DCPUS_NODE=$ncpus -DdebugBasicInfo"

##########################################################################

AC_SUBST([PERFO_CFLAGS])
AC_SUBST([PERFO_CPPFLAGS])
AC_SUBST([DEBUG_CFLAGS])
AC_SUBST([DEBUG_CPPFLAGS])


##########################################################################
# EXTERNAL LIBS CHECK
##########################################################################

if test x"$mpi" = x"no"; then
    AC_MSG_ERROR([mpi.h was not found. MPI headers are mandatory. You can use --with-mpi=dir option to set the MPI location or you can manually add an include directory in CPPFLAGS])
fi

AM_CONDITIONAL([MPI_SUPPORT], [test x"$mpi" = x"yes"])
AC_SUBST([MPI_HOME])
AM_CONDITIONAL([HWLOC_SUPPORT], [test x"$hwloc" = x"yes"])
AC_SUBST([HWLOC_HOME])

AC_CHECK_PROG([lscpu], [lscpu], [yes], [no])
if test x"$lscpu" = x"no"; then
   if test x"$hwloc" = x"no"; then
      AC_MSG_ERROR([neither lscpu nor hwloc are found, one of them is mandatory])
   fi
fi

##########################################################################

# generate the final Makefile etc.
AC_OUTPUT

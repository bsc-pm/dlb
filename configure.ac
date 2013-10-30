
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
AC_ARG_WITH([mpi_mt],
   AS_HELP_STRING([--with-mpi_mt=dir], [Directory of MPI installation (Threaded version)]),
   [
      mpi=yes
      mpi_mt=yes
      MPI_HOME="$withval"
   ]
)
AC_ARG_WITH([mpich],
   AS_HELP_STRING([--with-mpich=dir], [Directory of MPICH installation]),
   [
      mpi=yes
      mpich=yes
      MPI_HOME="$withval"
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
      ncpus=$(nproc) || ncpus=1
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
# Instrumentation & Debug compilation support
##########################################################################

AC_MSG_CHECKING([whether to compile the debug version])
AC_ARG_ENABLE([debug],
   AS_HELP_STRING([--disable-debug], [Disables generation of debug version]),
   [ debug="$enableval" ],
   [ debug=yes ]
)
AC_MSG_RESULT([$debug])

AC_MSG_CHECKING([whether to compile the instrumentation version])
AC_ARG_ENABLE([instrumentation],
   AS_HELP_STRING([--disable-instrumentation], [Disables generation of instrumentation version]),
   [ instrumentation="$enableval" ],
   [ instrumentation=yes ]
)
AC_MSG_RESULT([$instrumentation])

AC_MSG_CHECKING([whether to compile the instrumentation-debug version])
AC_ARG_ENABLE([instrumentation-debug],
   AS_HELP_STRING([--disable-instrumentation-debug], [Disables generation of instrumentation-debug version]),
   [ instrumentation_debug="$enableval" ],
   [ instrumentation_debug=yes ]
)
AC_MSG_RESULT([$instrumentation_debug])

AM_CONDITIONAL([DEBUG_LIB], [test x"$debug" = x"yes"])
AM_CONDITIONAL([INSTRUMENTATION_LIB], [test x"$instrumentation" = x"yes"])
AM_CONDITIONAL([INSTRUMENTATION_DEBUG_LIB], [test x"$instrumentation_debug" = x"yes"])

PERFO_CFLAGS="-g -O3 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
PERFO_CPPFLAGS="-DCPUS_NODE=$ncpus -DdebugBasicInfo"

DEBUG_CFLAGS="-g -O0 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
DEBUG_CPPFLAGS="-DCPUS_NODE=$ncpus -DdebugBasicInfo -DdebugBlockingMPI -DdebugLend -DdebugSharedMem -DDEBUG_VERSION"

INSTR_CFLAGS="-g -O3 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
INSTR_CPPFLAGS="-DCPUS_NODE=$ncpus -DdebugBasicInfo -DINSTRUMENTATION_VERSION"

INSTR_DEBUG_CFLAGS="-g -O0 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
INSTR_DEBUG_CPPFLAGS="-DCPUS_NODE=$ncpus -DdebugBasicInfo -DdebugBlockingMPI -DdebugLend -DdebugSharedMem -DDEBUG_VERSION -DINSTRUMENTATION_VERSION"

AC_SUBST([PERFO_CFLAGS])
AC_SUBST([PERFO_CPPFLAGS])
AC_SUBST([DEBUG_CFLAGS])
AC_SUBST([DEBUG_CPPFLAGS])
AC_SUBST([INSTR_CFLAGS])
AC_SUBST([INSTR_CPPFLAGS])
AC_SUBST([INSTR_DEBUG_CFLAGS])
AC_SUBST([INSTR_DEBUG_CPPFLAGS])


##########################################################################
# EXTERNAL LIBS CHECK
##########################################################################

if test x"$mpi" = x"no"; then
    AC_MSG_ERROR([mpi.h was not found. MPI headers are mandatory. You can use --with-mpi=dir option to set the MPI location or you can manually add an include directory in CPPFLAGS])
else
    if test x"$mpich" = x"yes"; then
        MPI_LIBS=-lmpich
    else
        if test x"$mpi_mt" = x"yes"; then
            MPI_LIBS=-lmpi_mt
        else
            MPI_LIBS=-lmpi
        fi
    fi
fi

AM_CONDITIONAL([MPI_SUPPORT], [test x"$mpi" = x"yes"])
AC_SUBST([MPI_HOME])
AC_SUBST([MPI_LIBS])
AM_CONDITIONAL([HWLOC_SUPPORT], [test x"$hwloc" = x"yes"])
AC_SUBST([HWLOC_HOME])

AC_CHECK_PROG([lscpu], [lscpu], [yes], [no])
if test x"$lscpu" = x"no"; then
   if test x"$hwloc" = x"no"; then
      AC_MSG_ERROR([neither lscpu nor hwloc were found, one of them is mandatory])
   fi
fi

##########################################################################

# generate the final Makefile etc.
AC_OUTPUT

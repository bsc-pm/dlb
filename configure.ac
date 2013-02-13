
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

LT_INIT

# Checks for programs
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_CC([$CC])
AC_PROG_CPP

# use the C compiler for the following checks
AC_LANG([C])

# check for MPI path
AC_ARG_WITH([mpi],
   AS_HELP_STRING([--with-mpi=dir], [Directory of MPI installation]),
   [
      mpi_dir=yes
      MPI_HOME="$withval"
   ],
   [
      mpi_dir=no
   ]
)

# check for number of cpus on node
AC_ARG_WITH([cpus-per-node],
   AS_HELP_STRING([--with-cpus-per-node=N], [Number of cpus per node]),
   [
      ncpus="$withval"
   ],
   [
      ncpus=1
   ]
)

AM_CONDITIONAL([MPI_SUPPORT], [test x"$mpi_dir" = x"yes"])
AC_SUBST([MPI_HOME])


# Checks for header files.
#AC_HEADER_STDC
#AC_CHECK_HEADERS([string])
#AC_CHECK_HEADERS([iostream])
AC_CHECK_HEADERS([mpi.h])

AC_CONFIG_HEADERS([config.h])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([Makefile src/Makefile scripts/Makefile])


##########################################################################
# debug compilation support
##########################################################################

AC_MSG_CHECKING([whether to build with debug information])
AC_ARG_ENABLE([debug],
   AS_HELP_STRING([--enable-debug], [enable debug data generation [default=no]]),
   [ debugit="$enableval" ],
   [ debugit=no ]
)
AC_MSG_RESULT([$debugit])

if test x"$debugit" = x"yes"; then
    AC_DEFINE([DEBUG],[],[Debug Mode])
    DEBUG_FLAGS="-g -O0 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
    DEBUG_MACRO_FLAGS="-DCPUS_NODE=$ncpus -DDEBUGBASICINFO -DDEBUGINOUT -DDEBUGLEND -DDEBUGSHAREDMEM"
else
    AC_DEFINE([NDEBUG],[],[No-debug Mode])
fi
PERFO_FLAGS="-g -O3 -Wall -Werror -Wno-error=unknown-pragmas -Wno-uninitialized"
PERFO_MACRO_FLAGS="-DCPUS_NODE=$ncpus -DDEBUGBASICINFO"

##########################################################################

AC_SUBST([PERFO_FLAGS])
AC_SUBST([PERFO_MACRO_FLAGS])
AC_SUBST([DEBUG_FLAGS])
AC_SUBST([DEBUG_MACRO_FLAGS])


##########################################################################
# MPI CHECK
##########################################################################

if test x"$ac_cv_header_mpi_h" = x"no"; then
    if test x"$mpi_dir" = x"no"; then
        AC_MSG_ERROR([mpi.h was not found. MPI headers are mandatory. You can use --with-mpi=dir option to set the MPI location or you can manually add an include directory in CPPFLAGS])
    fi
fi

##########################################################################

# generate the final Makefile etc.
AC_OUTPUT

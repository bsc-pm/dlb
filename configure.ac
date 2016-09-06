
AC_INIT([DLB],[1.2a],[pm-tools@bsc.es])
AC_CONFIG_SRCDIR([src/LB_core/DLB_kernel.c])

configure_orig_arguments="$@"

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])
AC_CONFIG_MACRO_DIR([m4])

# Enable silent rules if available
supported_silent_rules=1
m4_ifdef([AM_SILENT_RULES],
         [AM_SILENT_RULES([yes])],
         [supported_silent_rules=0])

AM_CONDITIONAL([SUPPORTED_SILENT_RULES], test x$supported_silent_rules = x1)

# Versioning things
# Make sure build_version is double-quoted!
build_version="\"unknown revision\""
if test -d $srcdir/.git;
then
   AC_PATH_PROG([GIT], [git], [])
   AC_MSG_CHECKING([revision of git repository])
   run_git=""
   if test x"$GIT" != x;
   then
      echo "$GIT" "$srcdir" >& AS_MESSAGE_LOG_FD
      run_gitversion=`"$GIT" --git-dir=$srcdir/.git show "--pretty=format:%h %ci" HEAD | head -n 1`
      run_gitbranch=`"$GIT" --git-dir=$srcdir/.git branch | grep ^* | sed s/*\ //g`
      build_version="\"git $run_gitbranch $run_gitversion developer version\""
   fi
   if test x"$GIT" = x -o x"$run_gitversion" = x;
   then
       build_version="\"unknown git commit\""
   fi
   AC_MSG_RESULT([$build_version])
elif test -e $srcdir/VERSION;
then
     AC_MSG_CHECKING([revision of distributed release])
     run_gitversion=`cat $srcdir/VERSION`
     build_version="\"$run_gitversion\""
     AC_MSG_RESULT([$build_version])
fi
AC_DEFINE_UNQUOTED([DLB_BUILD_VERSION], [$build_version], [Build version])

dlb_config_args='"'$(echo "$0 $configure_orig_arguments")'"'
AC_DEFINE_UNQUOTED([DLB_CONFIGURE_ARGS], [$dlb_config_args], [Configure line])

# Debian release
AC_CHECK_PROG([DEB_RELEASE], [lsb_release], [$(lsb_release -sc)], [])
AC_SUBST([DEB_RELEASE])

# Set compiler default flags
: ${CFLAGS=""}
: ${CXXFLAGS=""}
: ${FCFLAGS=""}

# Checks for programs
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT
AC_PROG_CC([$CC])
AM_PROG_CC_C_O
AC_PROG_CPP

# use the C compiler for the following checks
AC_LANG([C])

# Check for Python
AM_PATH_PYTHON

# check for OpenMP availability
AC_OPENMP

# check for MPI
AX_PROG_MPI
AX_PROG_MPITESTS
AM_CONDITIONAL([MPI_LIB], [test "${MPI_INSTALLED}" = "yes"])
AM_CONDITIONAL([IGNORE_MPI_TESTS], [test "${MPI_INSTALLED}" != "yes" -o "x${mpi_tests}" = "xno"])

# check for HWLOC
AX_PROG_HWLOC

# check for LSCPU
AX_PROG_LSCPU

# check for number of cpus on node
AC_ARG_WITH([cpus-per-node],
   AS_HELP_STRING([--with-cpus-per-node=N], [Number of cpus per node]),
   [
      ncpus="$withval"
   ],
   [
      ncpus=$(getconf _NPROCESSORS_ONLN) || ncpus=1
   ]
)

# check for SPHINX
AX_PROG_SPHINX

AC_CONFIG_HEADERS([config.h])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([ Makefile
                  README
                  tests/Makefile
                  tests/gens/Makefile
               ])


##########################################################################
# Instrumentation & Debug compilation support
##########################################################################

AC_MSG_CHECKING([whether to compile the debug version])
AC_ARG_ENABLE([debug],
   AS_HELP_STRING([--disable-debug], [Disable generation of debug version]),
   [ debug="$enableval" ],
   [ debug=yes ]
)
AC_MSG_RESULT([$debug])

AC_MSG_CHECKING([whether to compile the instrumentation version])
AC_ARG_ENABLE([instrumentation],
   AS_HELP_STRING([--disable-instrumentation], [Disable generation of instrumentation version]),
   [ instrumentation="$enableval" ],
   [ instrumentation=yes ]
)
AC_MSG_RESULT([$instrumentation])

AC_MSG_CHECKING([whether to compile the instrumentation-debug version])
AC_ARG_ENABLE([instrumentation-debug],
   AS_HELP_STRING([--disable-instrumentation-debug], [Disable generation of instrumentation-debug version]),
   [ instrumentation_debug="$enableval" ],
   [ instrumentation_debug=yes ]
)
AC_MSG_RESULT([$instrumentation_debug])

AM_CONDITIONAL([DEBUG_LIB], [test x"$debug" = x"yes"])
AM_CONDITIONAL([INSTRUMENTATION_LIB], [test x"$instrumentation" = x"yes"])
AM_CONDITIONAL([INSTRUMENTATION_DEBUG_LIB], [test x"$instrumentation_debug" = x"yes"])

AX_GCC_VERSION_CHECK
WARNING_FLAGS="-Wall -Werror -Wno-error=unknown-pragmas $ax_cv_gcc_version_warnings"

PERFO_CFLAGS="-g -O3 $WARNING_FLAGS"

DEBUG_CFLAGS="-g -O0 $WARNING_FLAGS"
DEBUG_CPPFLAGS="-DDEBUG_VERSION"

INSTR_CFLAGS="-g -O3 $WARNING_FLAGS"
INSTR_CPPFLAGS="-DINSTRUMENTATION_VERSION"

INSTR_DEBUG_CFLAGS="-g -O0 $WARNING_FLAGS"
INSTR_DEBUG_CPPFLAGS="-DDEBUG_VERSION -DINSTRUMENTATION_VERSION"

AC_SUBST([PERFO_CFLAGS])
AC_SUBST([PERFO_CPPFLAGS])
AC_SUBST([DEBUG_CFLAGS])
AC_SUBST([DEBUG_CPPFLAGS])
AC_SUBST([INSTR_CFLAGS])
AC_SUBST([INSTR_CPPFLAGS])
AC_SUBST([INSTR_DEBUG_CFLAGS])
AC_SUBST([INSTR_DEBUG_CPPFLAGS])

##########################################################################
# BlueGene/Q specific options
##########################################################################

case $host in
   powerpc64-bgq-linux*)
      AC_DEFINE([IS_BGQ_MACHINE], [1], [Defined if this machine is a BGQ machine])
      AC_LDFLAGS="-dynamic"
      ncpus=64
      ;;
   *)
      AC_LDFLAGS=""
      ;;
esac
AC_SUBST([AC_LDFLAGS])
AC_DEFINE_UNQUOTED([CPUS_NODE], [$ncpus], [Number of cpus per node])


##########################################################################

NCPUS=$ncpus
AC_SUBST([NCPUS])

# generate the final Makefile etc.
AC_OUTPUT

# Summary
echo ""
echo "$PACKAGE_STRING configuration summary"
echo "============================="
echo ""
echo " Installation prefix       : $prefix"
echo " Debug version             : $debug"
echo " Instrumentation version   : $instrumentation"
echo " Instr + debug version     : $instrumentation_debug"
echo ""
echo " Extra Packages:"
AX_SHOW_CONFIGURATION([MPI])
AX_SHOW_CONFIGURATION([HWLOC])

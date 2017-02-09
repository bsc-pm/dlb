
AC_INIT([DLB],[1.4a],[pm-tools@bsc.es])
AC_CONFIG_SRCDIR([src/LB_core/DLB_kernel.c])

configure_orig_arguments="$@"

AM_INIT_AUTOMAKE([foreign subdir-objects -Wall -Werror])
AM_EXTRA_RECURSIVE_TARGETS([coverage-report])
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

# If CFLAGS is not user-defined, define it as empty to avoid AC_PROG_CC setting it to default '-g -O2'
: ${CFLAGS=""}

# Checks for programs
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT
AC_PROG_CC([$CC])
AM_PROG_CC_C_O
AC_PROG_CPP

# use the C compiler for the following checks
AC_LANG([C])
AX_CHECK_COMPILE_FLAG([-pthread], [AC_CFLAGS="${AC_CFLAGS} -pthread"])
AX_CHECK_COMPILE_FLAG([-fvisibility=hidden], [AC_CFLAGS="${AC_CFLAGS} -fvisibility=hidden"])
AX_CHECK_COMPILE_FLAG([-Wall], [AC_CFLAGS="${AC_CFLAGS} -Wall"])
AX_CHECK_COMPILE_FLAG([-Werror], [AC_CFLAGS="${AC_CFLAGS} -Werror"])
AX_CHECK_COMPILE_FLAG([-Wno-error=unknown-pragmas],
                      [AC_CFLAGS="${AC_CFLAGS} -Wno-error=unknown-pragmas"])
AX_CONFIG_CC_WARNINGS
AX_CHECK_COMPILE_FLAG([$ax_cv_gcc_version_warnings],
                      [AC_CFLAGS="${AC_CFLAGS} $ax_cv_gcc_version_warnings"])
AC_SUBST([AC_CFLAGS])
AX_CHECK_COMPILE_FLAG([-coverage], [COVERAGE_FLAGS=-coverage])

AC_CHECK_HEADERS([stdatomic.h])

# Check for Python
AM_PATH_PYTHON

# check for OpenMP availability
AC_OPENMP

# check for MPI
AX_MPI

# check for HWLOC
AX_HWLOC

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
AC_CHECK_PROG([sphinx], [sphinx-build], [yes])
AM_CONDITIONAL([HAVE_SPHINX], [test "x$sphinx" = xyes])

# check for Doxygen
AC_CHECK_PROG([doxygen], [doxygen], [yes])
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$doxygen" = xyes])

# check for gcovr
AC_CHECK_PROG([gcovr], [gcovr], [yes])
AM_CONDITIONAL([HAVE_GCOVR], [test "x$gcovr" = xyes])

AC_CONFIG_HEADERS([config.h])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([
    Makefile
    README
    tests/Makefile
    doc/examples/mpi+omp/run.sh
    doc/examples/mpi+omp/Makefile
    doc/examples/mpi+ompss/run.sh
    doc/examples/mpi+ompss/Makefile
    doc/examples/statistics/run.sh
    doc/examples/statistics/Makefile
    doc/doxygen/Doxyfile
    doc/doxygen/Doxyfile_man
])
AC_CONFIG_FILES([tests/gens/basic-generator], [chmod +x tests/gens/basic-generator])
AC_CONFIG_FILES([tests/gens/mpi-generator], [chmod +x tests/gens/mpi-generator])


##########################################################################
# Instrumentation & Debug compilation support
##########################################################################

AC_MSG_CHECKING([whether to compile the debug version])
AC_ARG_ENABLE([debug],
   AS_HELP_STRING([--disable-debug], [Disable generation of debug version]),
   [], dnl Implicit: enable_debug=$enableval
   [enable_debug=yes]
)
AC_MSG_RESULT([$enable_debug])

AC_MSG_CHECKING([whether to compile the instrumentation version])
AC_ARG_ENABLE([instrumentation],
   AS_HELP_STRING([--disable-instrumentation], [Disable generation of instrumentation version]),
   [], dnl Implicit: enable_instrumentation=$enableval
   [enable_instrumentation=yes]
)
AC_MSG_RESULT([$enable_instrumentation])

AC_MSG_CHECKING([whether to compile the instrumentation-debug version])
AC_ARG_ENABLE([instrumentation-debug],
   AS_HELP_STRING([--disable-instrumentation-debug], [Disable generation of instrumentation-debug version]),
   [], dnl Implicit: enable_instrumentation_debug=$enableval
   [enable_instrumentation_debug=yes]
)
AC_MSG_RESULT([$enable_instrumentation_debug])

AM_CONDITIONAL([DEBUG_LIB], [test "x$enable_debug" = xyes])
AM_CONDITIONAL([INSTRUMENTATION_LIB], [test "x$enable_instrumentation" = xyes])
AM_CONDITIONAL([INSTRUMENTATION_DEBUG_LIB], [test "x$enable_instrumentation_debug" = xyes])

TEST_VERSIONS="performance"
AS_IF([test "x$enable_debug" = xyes], [
    TEST_VERSIONS="$TEST_VERSIONS debug"
])
AC_SUBST([TEST_VERSIONS])

COMMON_CPPFLAGS="-D_GNU_SOURCE"

PERFO_CFLAGS="-g -O3"
PERFO_CPPFLAGS="$COMMON_CPPFLAGS"

DEBUG_CFLAGS="-g -O0 $COVERAGE_FLAGS"
DEBUG_CPPFLAGS="$COMMON_CPPFLAGS -DDEBUG_VERSION"

INSTR_CFLAGS="-g -O3"
INSTR_CPPFLAGS="$COMMON_CPPFLAGS -DINSTRUMENTATION_VERSION"

INSTR_DEBUG_CFLAGS="-g -O0"
INSTR_DEBUG_CPPFLAGS="$COMMON_CPPFLAGS -DDEBUG_VERSION -DINSTRUMENTATION_VERSION"

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
AS_ECHO(["
$PACKAGE_STRING configuration summary
=============================

Installation prefix       : $prefix
Debug version             : $enable_debug
Instrumentation version   : $enable_instrumentation
Instr + debug version     : $enable_instrumentation_debug
External libraries:"])

AS_ECHO_N(["
* MPI: "])
AS_IF([test "x$with_mpi" = xno], [
    AS_ECHO(["no"])
], [
    AS_ECHO(["yes
  MPI_CPPFLAGS:   $MPI_CPPFLAGS
  MPI_LDFLAGS:    $MPI_LDFLAGS"])
])

AS_ECHO_N(["
* HWLOC: "])
AS_IF([test "x$with_hwloc" = xno], [
    AS_ECHO(["no"])
], [
    AS_ECHO(["yes
  HWLOC_CPPFLAGS: $HWLOC_CPPFLAGS
  HWLOC_LDFLAGS:  $HWLOC_LDFLAGS"])
])


AC_INIT([DLB],[3.2],[pm-tools@bsc.es])
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


################################################################################
# Obtain versioning information
################################################################################
AC_ARG_VAR([GIT], [distributed version control system])
AC_PATH_PROG([GIT], [git], [])
# Make sure build_version is double-quoted!
AS_IF([test -e "$srcdir/.git"], [
    AC_MSG_CHECKING([revision of git repository])
    AS_IF([test x"$GIT" != x], [
        echo "$GIT" "$srcdir" >& AS_MESSAGE_LOG_FD
        run_gitversion=`"$GIT" --git-dir=$srcdir/.git show "--pretty=format:%h %ci" HEAD | head -n 1`
        run_gitbranch=`"$GIT" --git-dir=$srcdir/.git branch | grep ^* | sed s/*\ //g`
        build_version="\"git $run_gitbranch $run_gitversion developer version\""
    ], [
        build_version="\"unknown git commit\""
    ])
    AC_MSG_RESULT([$build_version])
    developer_version=yes
], [test -e $srcdir/VERSION], [
    AC_MSG_CHECKING([revision of distributed release])
    run_gitversion=`cat $srcdir/VERSION`
    build_version="\"$run_gitversion\""
    developer_version=no
    AC_MSG_RESULT([$build_version])
], [
    build_version="\"unknown revision\""
    developer_version=no
])
AC_DEFINE_UNQUOTED([DLB_BUILD_VERSION], [$build_version], [Build version])

AC_MSG_CHECKING([whether to enable developer mode])
AC_ARG_ENABLE([developer-mode],
    AS_HELP_STRING([--enable-developer-mode], [use stricter error compilation flags]),
    [
        AS_IF([test x"$enable_developer_mode" = xyes], [
            developer_version=yes
        ])
     ],
     []
)
AC_MSG_RESULT([$developer_version])

dlb_config_args='"'$(echo "$0 $configure_orig_arguments")'"'
AC_DEFINE_UNQUOTED([DLB_CONFIGURE_ARGS], [$dlb_config_args], [Configure line])

# Debian release
AC_CHECK_PROG([DEB_RELEASE], [lsb_release], [$(lsb_release -sc)], [])
AC_SUBST([DEB_RELEASE])


################################################################################
# Configure compiler and compiler flags
################################################################################
# If CFLAGS is not user-defined, define it as empty to avoid AC_PROG_CC setting it to default '-g -O2'
: ${CFLAGS=""}

# Checks for programs
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT
AM_CONDITIONAL([HAVE_STATIC_LIBS], [test "x$enable_static" = xyes])
AC_PROG_CPP

##### C Compiler checks #####
AC_PROG_CC([$CC])
AM_PROG_CC_C_O

# use the C compiler for the following checks
AC_LANG([C])

# icc needs -we10006 to promote unrecognized flags from warning to error
AX_CHECK_COMPILE_FLAG([-we10006], [intel_we_flag="-we10006"])
AX_VAR_PUSHVALUE([CFLAGS], [$CFLAGS $intel_we_flag])
# icc >= 19 needs -we10148 instead
AX_CHECK_COMPILE_FLAG([-we10148], [intel_we_flag="-we10148"])
AX_VAR_PUSHVALUE([CFLAGS], [$CFLAGS $intel_we_flag])

# fccpx needs this env. variable to promote unrecognized flags from warning to error
AX_VAR_PUSHVALUE([FCOMP_UNRECOGNIZED_OPTION], [error])
export FCOMP_UNRECOGNIZED_OPTION

AX_CHECK_COMPILE_FLAG([-pthread], [AC_CFLAGS="${AC_CFLAGS} -pthread"])
AX_CHECK_COMPILE_FLAG([-fvisibility=hidden], [AC_CFLAGS="${AC_CFLAGS} -fvisibility=hidden"])
AX_CHECK_COMPILE_FLAG([-Wno-unused-result], [AC_CFLAGS="${AC_CFLAGS} -Wno-unused-result"])
AS_IF([test "x$developer_version" = xyes], [
    AX_CHECK_COMPILE_FLAG([-Wall], [AC_CFLAGS="${AC_CFLAGS} -Wall"])
    AX_CHECK_COMPILE_FLAG([-Werror], [AC_CFLAGS="${AC_CFLAGS} -Werror"])
    AX_CHECK_COMPILE_FLAG([-Wshadow], [AC_CFLAGS="${AC_CFLAGS} -Wshadow"])
    AX_CHECK_COMPILE_FLAG([-Wno-error=shadow], [AC_CFLAGS="${AC_CFLAGS} -Wno-error=shadow"])
    AX_CHECK_COMPILE_FLAG([-Wformat-security], [AC_CFLAGS="${AC_CFLAGS} -Wformat-security"])
    AX_CHECK_COMPILE_FLAG([-Wsign-compare], [AC_CFLAGS="${AC_CFLAGS} -Wsign-compare"])
    AX_CHECK_COMPILE_FLAG([-Wimplicit-fallthrough], [AC_CFLAGS="${AC_CFLAGS} -Wimplicit-fallthrough"])
    AX_CHECK_COMPILE_FLAG([-Wvla], [AC_CFLAGS="${AC_CFLAGS} -Wvla"])
])
AX_CHECK_COMPILE_FLAG([--coverage], [COVERAGE_FLAGS=--coverage])
AX_CHECK_LINK_FLAG([-lm], [AC_LDFLAGS="${AC_LDFLAGS} -lm"])
AX_CHECK_LINK_FLAG([-ldl], [AC_LDFLAGS="${AC_LDFLAGS} -ldl"])
AX_CHECK_LINK_FLAG([-lrt], [AC_LDFLAGS="${AC_LDFLAGS} -lrt"])
AC_CHECK_HEADERS([stdatomic.h])
AC_CHECK_HEADERS([execinfo.h])
AC_CHECK_HEADERS([immintrin.h])
AC_CHECK_HEADERS([emmintrin.h])

# restore fccpx environment variable
AX_VAR_POPVALUE([FCOMP_UNRECOGNIZED_OPTION])
export FCOMP_UNRECOGNIZED_OPTION

# restore CFLAGS
AX_VAR_POPVALUE([CFLAGS])

##### Fortran Compiler checks #####
AC_PROG_FC

# Disable Fortran tests if FC is not found
AS_IF([test "x$FC" = x], [
    AC_MSG_WARN([Fortran compiler was not found, disabling Fortran tests.])
], [
    # use the Fortran compiler for the following checks
    AC_LANG_PUSH([Fortran])
    AX_CHECK_COMPILE_FLAG([-pthread], [AC_FFLAGS="${AC_FFLAGS} -pthread"])
    AS_IF([test "x$developer_version" = xyes], [
        AX_CHECK_COMPILE_FLAG([-Wall], [AC_FFLAGS="${AC_FFLAGS} -Wall"])
        AX_CHECK_COMPILE_FLAG([-Werror], [AC_FFLAGS="${AC_FFLAGS} -Werror"])
    ])
    AC_LANG_POP([Fortran])
])
AM_CONDITIONAL([HAVE_FORTRAN], [test "x$FC" != x])


################################################################################
# Configure library versions
################################################################################

AC_MSG_CHECKING([whether to compile the debug version])
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--disable-debug], [do not compile debug version library]),
    [], dnl Implicit: enable_debug=$enableval
    [enable_debug=yes]
)
AC_MSG_RESULT([$enable_debug])

AC_MSG_CHECKING([whether to compile the instrumentation version])
AC_ARG_ENABLE([instrumentation],
    AS_HELP_STRING([--disable-instrumentation], [do not compile instrumentation version library]),
   [], dnl Implicit: enable_instrumentation=$enableval
   [enable_instrumentation=yes]
)
AC_MSG_RESULT([$enable_instrumentation])

AC_MSG_CHECKING([whether to compile the instrumentation-debug version])
AC_ARG_ENABLE([instrumentation-debug],
   AS_HELP_STRING([--disable-instrumentation-debug],
                  [do not compile instrumentation debug version library]),
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
PERFO_FFLAGS="-g -O3"
PERFO_CPPFLAGS="$COMMON_CPPFLAGS"

DEBUG_CFLAGS="-g -O0"
DEBUG_FFLAGS="-g -O0"
DEBUG_CPPFLAGS="$COMMON_CPPFLAGS -DDEBUG_VERSION"

INSTR_CFLAGS="-g -O3"
INSTR_FFLAGS="-g -O3"
INSTR_CPPFLAGS="$COMMON_CPPFLAGS -DINSTRUMENTATION_VERSION"

INSTR_DEBUG_CFLAGS="-g -O0"
INSTR_DEBUG_FFLAGS="-g -O0"
INSTR_DEBUG_CPPFLAGS="$COMMON_CPPFLAGS -DDEBUG_VERSION -DINSTRUMENTATION_VERSION"

### Coverage ###
AC_MSG_CHECKING([whether to enable coverage profiling])
AC_ARG_ENABLE([coverage],
    AS_HELP_STRING([--enable-coverage], [enable coverage profiling]),
    [], dnl Implicit: enable_coverage=$enableval
    [enable_coverage=no]
)
AC_MSG_RESULT([$enable_coverage])

AS_IF([test "x$enable_coverage" = xyes], [
    DEBUG_CFLAGS="$DEBUG_CFLAGS $COVERAGE_FLAGS"
    DEBUG_FFLAGS="$DEBUG_FFLAGS $COVERAGE_FLAGS"

    AC_CHECK_PROG([gcovr], [gcovr], [yes])
    AS_IF([test "x$gcovr" != xyes], [
        AC_MSG_ERROR([Coverage profiling requires gcovr tool])
    ])
    AS_IF([test "x$enable_static" != xyes], [
        AC_MSG_ERROR([--enable-coverage and --disable-static are incompatible options])
    ])
])
AM_CONDITIONAL([ENABLE_COVERAGE], [test "x$enable_coverage" = xyes])

### Python viewer scripts ###
AC_MSG_CHECKING([whether to install DLB python viewer scripts (deprecated)])
AC_ARG_ENABLE([viewer-scripts],
    AS_HELP_STRING([--enable-viewer-scripts], [enable deprecated viewer scripts]),
    [], dnl Implicit: enable_viewer_scripts=$enableval
    [enable_viewer_scripts=no]
)
AC_MSG_RESULT([$enable_viewer_scripts])
AM_CONDITIONAL([ENABLE_VIEWER_SCRIPTS], [test "x$enable_viewer_scripts" = xyes])


AC_SUBST([PERFO_CFLAGS])
AC_SUBST([PERFO_FFLAGS])
AC_SUBST([PERFO_CPPFLAGS])
AC_SUBST([DEBUG_CFLAGS])
AC_SUBST([DEBUG_FFLAGS])
AC_SUBST([DEBUG_CPPFLAGS])
AC_SUBST([INSTR_CFLAGS])
AC_SUBST([INSTR_FFLAGS])
AC_SUBST([INSTR_CPPFLAGS])
AC_SUBST([INSTR_DEBUG_CFLAGS])
AC_SUBST([INSTR_DEBUG_FFLAGS])
AC_SUBST([INSTR_DEBUG_CPPFLAGS])


################################################################################
# Configure external libraries and programs
################################################################################

# Check for Python
AM_PATH_PYTHON
AX_COMPARE_VERSION([$PYTHON_VERSION], [ge], [2.7], [lit=yes], [lit=no])
AM_CONDITIONAL([LIT_SUPPORT], [test "x$lit" = xyes])

# check for OpenMP availability
AC_OPENMP
AM_CONDITIONAL([HAVE_OPENMP], [test -n "$OPENMP_CFLAGS" || test "x$ac_cv_prog_c_openmp" = "xnone needed"])

# check for MPI
AC_ARG_VAR([MPICC], [MPI C compiler wrapper])
AX_MPI

# check for HWLOC
AX_HWLOC

# check for SPHINX
AC_CHECK_PROG([sphinx], [sphinx-build], [yes])
AM_CONDITIONAL([HAVE_SPHINX], [test "x$sphinx" = xyes])

# check for Doxygen
AC_CHECK_PROG([doxygen], [doxygen], [yes])
AM_CONDITIONAL([HAVE_DOXYGEN], [test "x$doxygen" = xyes])
AM_COND_IF([HAVE_DOXYGEN], [
    # Doxygen < '1.8.16' had some bugs with underline HTML tags:
    AC_MSG_CHECKING([doxygen version])
    doxygen_version="`doxygen --version`"
    AC_MSG_RESULT([$doxygen_version])
    AX_COMPARE_VERSION([$doxygen_version], [ge], [1.8.16],
                       [DOXYGEN_ALIASES="underline{1}=\"<U>\\1</U>\""],
                       [DOXYGEN_ALIASES="underline{1}=\"\\1\""])
])
AC_SUBST([DOXYGEN_ALIASES])

# check for Pandoc
AC_CHECK_PROG([pandoc], [pandoc], [yes])
AS_IF([test "x$pandoc" = xyes], [
    AC_MSG_CHECKING([whether pandoc supports the required formats])
    AS_IF([! pandoc --from=man --to=rst /dev/null > /dev/null 2>&1], [
           pandoc=no
    ])
    AC_MSG_RESULT([$pandoc])
])
AM_CONDITIONAL([HAVE_PANDOC], [test "x$pandoc" = xyes])

# Check if Sphinx will include man pages
AM_CONDITIONAL([SPHINX_HAS_MANPAGES], [test "x$pandoc" = xyes && test "x$doxygen" = xyes])
AM_COND_IF([SPHINX_HAS_MANPAGES], [
    SPHINX_HAS_MANPAGES=""
], [
    SPHINX_HAS_MANPAGES=".. "
])
AC_SUBST([SPHINX_HAS_MANPAGES])

AC_CONFIG_HEADERS([config.h])

# files to generate via autotools (.am or .in source files)
AC_CONFIG_FILES([
    Makefile
    tests/Makefile
    doc/examples/drom/Makefile
    doc/examples/mpi+omp/run.sh
    doc/examples/mpi+omp/Makefile
    doc/examples/mpi+omp_ompt/run.sh
    doc/examples/mpi+omp_ompt/Makefile
    doc/examples/mpi+ompss/run.sh
    doc/examples/mpi+ompss/Makefile
    doc/examples/ompt/Makefile
    doc/examples/statistics/run.sh
    doc/examples/statistics/Makefile
    doc/examples/talp/run.sh
    doc/examples/talp/Makefile
    doc/examples/monitoring_regions/run.sh
    doc/examples/monitoring_regions/Makefile
    doc/scripts/lewi_omp.sh
    doc/scripts/lewi_omp_trace.sh
    doc/scripts/lewi_ompss_trace.sh
    doc/scripts/talp.sh
    doc/doxygen/Doxyfile
    doc/doxygen/Doxyfile_man1
    doc/doxygen/Doxyfile_man3
    doc/user_guide/source/index.rst
])
AC_CONFIG_FILES([tests/gens/basic-generator], [chmod +x tests/gens/basic-generator])
AC_CONFIG_FILES([tests/gens/fortran-generator], [chmod +x tests/gens/fortran-generator])
AC_CONFIG_FILES([tests/gens/mpi-generator], [chmod +x tests/gens/mpi-generator])

# Some user guide files are generated, link the rest if necessary to the build dir
AC_CONFIG_LINKS([
    doc/user_guide/source/conf.py:doc/user_guide/source/conf.py
    doc/user_guide/source/advance_usage.rst:doc/user_guide/source/advance_usage.rst
    doc/user_guide/source/api.rst:doc/user_guide/source/api.rst
    doc/user_guide/source/contents.rst:doc/user_guide/source/contents.rst
    doc/user_guide/source/faq.rst:doc/user_guide/source/faq.rst
    doc/user_guide/source/how_to_install.rst:doc/user_guide/source/how_to_install.rst
    doc/user_guide/source/how_to_run.rst:doc/user_guide/source/how_to_run.rst
    doc/user_guide/source/intro.rst:doc/user_guide/source/intro.rst
    doc/user_guide/source/man_pages_appendix.rst:doc/user_guide/source/man_pages_appendix.rst
    doc/user_guide/source/technical_requirements.rst:doc/user_guide/source/technical_requirements.rst
    doc/user_guide/source/images/dlb_logo.png:doc/user_guide/source/images/dlb_logo.png
    doc/user_guide/source/images/dlb_logo_white.png:doc/user_guide/source/images/dlb_logo_white.png
    doc/user_guide/source/images/dlb_states.png:doc/user_guide/source/images/dlb_states.png
    doc/user_guide/source/images/dlb_icon.png:doc/user_guide/source/images/dlb_icon.png
    doc/user_guide/source/images/dlb_icon.ico:doc/user_guide/source/images/dlb_icon.ico
    doc/user_guide/source/images/drom.png:doc/user_guide/source/images/drom.png
    doc/user_guide/source/images/hpc_app.png:doc/user_guide/source/images/hpc_app.png
    doc/user_guide/source/images/LeWI.png:doc/user_guide/source/images/LeWI.png
    doc/user_guide/source/images/talp.png:doc/user_guide/source/images/talp.png
    doc/user_guide/source/_static/custom.css:doc/user_guide/source/_static/custom.css
])


################################################################################
# Host specific options
################################################################################

AS_CASE([$host],
    [powerpc64-bgq-linux*], [
        AC_DEFINE([IS_BGQ_MACHINE], [1], [Defined if this machine is a BGQ machine])
        AC_LDFLAGS="${AC_LDFLAGS} -dynamic"
        ncpus=64
    ], [
        ncpus=$(getconf _NPROCESSORS_ONLN) || ncpus=1
    ]
)
AC_DEFINE_UNQUOTED([NCPUS_AT_CONFIGURE_TIME], [$ncpus], [Number of CPUs detected at cofigure time])
################################################################################

AC_SUBST([AC_CFLAGS])
AC_SUBST([AC_FFLAGS])
AC_SUBST([AC_LDFLAGS])

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

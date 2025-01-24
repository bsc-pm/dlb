
AC_INIT([DLB],
        m4_esyscmd([scripts/git-version-gen .tarball-version]),
        [dlb@bsc.es],
        [dlb],
        [https://pm.bsc.es/dlb])
AC_CONFIG_SRCDIR([src/LB_core/DLB_kernel.c])

# Save configure arguments, ignore the ones we are not interested in
dlb_config_args="$0"
for arg in ${1+"$@"}; do
    case $arg in
        --no-create) ;;
        --no-recursion) ;;
        --*able-developer-mode) ;;
        --*able-extra-warnings) ;;
        *) AS_VAR_APPEND([dlb_config_args], [" $arg"])
    esac
done

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

build_from_git=no
dlb_git_branch=""
dlb_version_string="$PACKAGE $VERSION"
AS_IF([test -e "$srcdir/.git"], [
    AC_MSG_CHECKING([revision of git repository])
    AS_IF([test x"$GIT" != x], [
        dlb_git_branch=`"$GIT" --git-dir=$srcdir/.git rev-parse --abbrev-ref HEAD`
        dlb_git_describe=`"$GIT" --git-dir=$srcdir/.git describe HEAD`
        dlb_version_string="$PACKAGE $VERSION ($dlb_git_branch)"
        AC_MSG_RESULT([$dlb_git_describe ($dlb_git_branch)])
        build_from_git=yes
    ], [
        AC_MSG_RESULT([unknown])
    ])
], [test -e $srcdir/.tarball-version], [
    AC_MSG_CHECKING([revision of distributed release])
    AC_MSG_RESULT([$VERSION])
])
AC_DEFINE_UNQUOTED([DLB_VERSION_STRING], ["$dlb_version_string"], [dlb version string, and possibly branch])

# We need Python for the version parser
AM_PATH_PYTHON

# Major, minor, and patch numbers go to config.h
DLB_VERSION_MAJOR=$($PYTHON "$srcdir/scripts/parse_version.py" major $VERSION)
AC_DEFINE_UNQUOTED([DLB_VERSION_MAJOR], [$DLB_VERSION_MAJOR], [dlb major version])

DLB_VERSION_MINOR=$($PYTHON "$srcdir/scripts/parse_version.py" minor $VERSION)
AC_DEFINE_UNQUOTED([DLB_VERSION_MINOR], [$DLB_VERSION_MINOR], [dlb minor version])

DLB_VERSION_PATCH=$($PYTHON "$srcdir/scripts/parse_version.py" patch $VERSION)
AC_DEFINE_UNQUOTED([DLB_VERSION_PATCH], [$DLB_VERSION_PATCH], [dlb patch version])

# Version hexadecimal number go to dlb.h
DLB_VERSION_HEXA=$($PYTHON "$srcdir/scripts/parse_version.py" hexa $VERSION)
AC_SUBST([DLB_VERSION_HEXA])

# branch is needed for internal CI pipelines
DLB_GIT_BRANCH="$dlb_git_branch"
AC_SUBST([DLB_GIT_BRANCH])

# DEPRECATED --enable-developer-mode in favor of --enable-extra-warnings
AC_ARG_ENABLE([developer-mode],
    AS_HELP_STRING([--enable-developer-mode],
                   [(deprecated: use --enable-extra-warnings instead)
                    use stricter error compilation flags]),
    [], dnl Implicit: enable_developer_mode=$enableval
    [enable_developer_mode=no]
)

AC_MSG_CHECKING([whether to enable extra warnings])
AC_ARG_ENABLE([extra-warnings],
    AS_HELP_STRING([--enable-extra-warnings],
                   [enable -Wall, -Werror, and other extra warning flags
                    (enabled by default if project is a git repository, disabled otherwise)]),
    [], dnl Implicit: enable_extra_warnings=$enableval
    [enable_extra_warnings=$build_from_git]
)
# keep compatibility until removal
AS_IF([test x"$enable_developer_mode" = xyes && test x"$enable_extra_warnings" = xno], [
       enable_extra_warnings=yes
])
AC_MSG_RESULT([$enable_extra_warnings])

AC_DEFINE_UNQUOTED([DLB_CONFIGURE_ARGS], ["$dlb_config_args"], [Configure options])

# Debian release
AC_CHECK_PROG([DEB_RELEASE], [lsb_release], [$(lsb_release -sc)], [])
AC_SUBST([DEB_RELEASE])


################################################################################
# Configure compiler and compiler flags
################################################################################
# If CFLAGS / FCFLAGS are not user-defined, define then as empty to avoid
# AC_PROG_CC and AC_PROG_FC setting then to the default value '-g -O2'
: ${CFLAGS=""}
: ${FCFLAGS=""}

# Checks for programs
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
LT_INIT
AM_CONDITIONAL([HAVE_STATIC_LIBS], [test "x$enable_static" = xyes])
AC_PROG_CPP

##### C Compiler checks #####
AC_PROG_CC([$CC])
AM_PROG_CC_C_O

dnl Autoconf 2.69 and below need this macro to check for C99
dnl In newer versions the macro is deprecated
m4_version_prereq([2.70], [:], [AC_PROG_CC_C99])

# use the C compiler for the following checks
AC_LANG([C])

# Add minimum c99 standard
#       if ac_cv_prog_cc_c{11,99} is set and its value is not 'no', no-op
AS_IF([${ac_cv_prog_cc_c11+:} false && test x"$ac_cv_prog_cc_c11" != xno], [:],
      [${ac_cv_prog_cc_c99+:} false && test x"$ac_cv_prog_cc_c99" != xno], [:],
      # if ac_cv_prog_cc_c89 is set and its value is not 'no', add -std=c99
      [${ac_cv_prog_cc_c89+:} false && test x"$ac_cv_prog_cc_c89" != xno], [
        AX_CHECK_COMPILE_FLAG([-std=c99], [AC_CFLAGS="${AC_CFLAGS} -std=c99"])
      ]
)

# save CFLAGS before flag testing
AX_VAR_PUSHVALUE([CFLAGS])

# Intel compilers need additional flags to promote unrecognized flags from warning to error
# Ideally, -diag-error=warn should be used, but this flag promotes several other warnings
# that are otherwise hidden. Another solution is to use -diag-error=<code>, but GCC does not
# complain about it. The final solution is to use the flag -we<code>, although deprecated,
# it still works.
# Ref: https://www.intel.com/content/www/us/en/develop/documentation/cpp-compiler-developer-guide-and-reference/top/compiler-reference/error-handling-1.html
AX_CHECK_COMPILE_FLAG([-we10006], [CFLAGS="${CFLAGS} -we10006"])
AX_CHECK_COMPILE_FLAG([-we10148], [CFLAGS="${CFLAGS} -we10148"])

# fccpx needs this env. variable to promote unrecognized flags from warning to error
AX_VAR_PUSHVALUE([FCOMP_UNRECOGNIZED_OPTION], [error])
export FCOMP_UNRECOGNIZED_OPTION

AX_CHECK_COMPILE_FLAG([-pthread], [AC_CFLAGS="${AC_CFLAGS} -pthread"])
AX_CHECK_COMPILE_FLAG([-fvisibility=hidden], [AC_CFLAGS="${AC_CFLAGS} -fvisibility=hidden"])
AX_CHECK_COMPILE_FLAG([-Wno-unused-result], [AC_CFLAGS="${AC_CFLAGS} -Wno-unused-result"])
AS_IF([test "x$enable_extra_warnings" = xyes], [
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

# use Fortran free-form by default
# NOTE: we must override action-if-failure to no-op to avoid erroring if FC is
# not found, because testing FC doesn't seem to work
AC_FC_FREEFORM([:], [:])

# Disable Fortran tests if FC is not found
AS_IF([test "x$FC" = x], [
    AC_MSG_WARN([Fortran compiler was not found, disabling Fortran tests.])
], [
    # use the Fortran compiler for the following checks
    AC_LANG_PUSH([Fortran])

    # save FCFLAGS before flag testing
    AX_VAR_PUSHVALUE([FCFLAGS])

    # Intel flags to promote unrecognized flags from warning to error
    AX_CHECK_COMPILE_FLAG([-we10148], [FCFLAGS="${FCFLAGS} -we10148"])
    AX_CHECK_COMPILE_FLAG([-we10157], [FCFLAGS="${FCFLAGS} -we10157"])

    AX_CHECK_COMPILE_FLAG([-pthread], [AC_FCFLAGS="${AC_FCFLAGS} -pthread"])
    AS_IF([test "x$enable_extra_warnings" = xyes], [
        AX_CHECK_COMPILE_FLAG([-Wall], [AC_FCFLAGS="${AC_FCFLAGS} -Wall"])
        AX_CHECK_COMPILE_FLAG([-Wno-c-binding-type], [AC_FCFLAGS="${AC_FCFLAGS} -Wno-c-binding-type"])
        AX_CHECK_FC_HONORS_NO_C_BINDING_TYPE([
            dnl Old Fortran compilers may not fail with and unrecognized flag
            dnl '-Wno-c-binding-type', so flag '-Werror' cannot be used.
            AX_CHECK_COMPILE_FLAG([-Werror], [AC_FCFLAGS="${AC_FCFLAGS} -Werror"])
        ])
    ])

    # restore FCFLAGS
    AX_VAR_POPVALUE([FCFLAGS])

    AC_LANG_POP([Fortran])

    # Check for assumed-type support (TYPE(*)), or defer to REAL
    # and for assumed-rank entity (DIMENSION(..)), or defer to assumed-size arrays
    AX_CHECK_FC_IGNORE_TYPE
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
PERFO_FCFLAGS="-g -O3"
PERFO_CPPFLAGS="$COMMON_CPPFLAGS"

DEBUG_CFLAGS="-g -O0"
DEBUG_FCFLAGS="-g -O0"
DEBUG_CPPFLAGS="$COMMON_CPPFLAGS -DDEBUG_VERSION"

INSTR_CFLAGS="-g -O3"
INSTR_FCFLAGS="-g -O3"
INSTR_CPPFLAGS="$COMMON_CPPFLAGS -DINSTRUMENTATION_VERSION"

INSTR_DEBUG_CFLAGS="-g -O0"
INSTR_DEBUG_FCFLAGS="-g -O0"
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
    DEBUG_FCFLAGS="$DEBUG_FCFLAGS $COVERAGE_FLAGS"

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
AC_SUBST([PERFO_FCFLAGS])
AC_SUBST([PERFO_CPPFLAGS])
AC_SUBST([DEBUG_CFLAGS])
AC_SUBST([DEBUG_FCFLAGS])
AC_SUBST([DEBUG_CPPFLAGS])
AC_SUBST([INSTR_CFLAGS])
AC_SUBST([INSTR_FCFLAGS])
AC_SUBST([INSTR_CPPFLAGS])
AC_SUBST([INSTR_DEBUG_CFLAGS])
AC_SUBST([INSTR_DEBUG_FCFLAGS])
AC_SUBST([INSTR_DEBUG_CPPFLAGS])


################################################################################
# Configure external libraries and programs
################################################################################

# Check for Python
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

# check for PAPI
AX_PAPI

# check for SPHINX
AC_ARG_ENABLE([sphinx-doc],
              AS_HELP_STRING([--disable-sphinx-doc], [disable sphinx-doc detection]),
    [], dnl Implicit: enable_sphinx_doc=$enableval
    [enable_sphinx_doc=yes]
)
AS_IF([test "x$enable_sphinx_doc" = xyes], [
    AC_CHECK_PROG([sphinx], [sphinx-build], [yes])
], [
    AC_MSG_CHECKING([for sphinx-build])
    AC_MSG_RESULT([skipped])
    sphinx=no
])
AM_CONDITIONAL([HAVE_SPHINX], [test "x$sphinx" = xyes])


# check for Doxygen
AC_ARG_ENABLE([doxygen],
              AS_HELP_STRING([--disable-doxygen], [disable doxygen detection]),
    [], dnl Implicit: enable_doxygen=$enableval
    [enable_doxygen=yes]
)
AS_IF([test "x$enable_doxygen" = xyes], [
    AC_CHECK_PROG([doxygen], [doxygen], [yes])
], [
    AC_MSG_CHECKING([for doxygen])
    AC_MSG_RESULT([skipped])
    doxygen=no
])
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
AC_ARG_ENABLE([pandoc],
              AS_HELP_STRING([--disable-pandoc], [disable pandoc detection]),
    [], dnl Implicit: enable_pandoc=$enableval
    [enable_pandoc=yes]
)
AS_IF([test "x$enable_pandoc" = xyes], [
    AC_CHECK_PROG([pandoc], [pandoc], [yes])
], [
    AC_MSG_CHECKING([for pandoc])
    AC_MSG_RESULT([skipped])
    pandoc=no
])
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
    cmake/DLBConfigVersion.cmake
    doc/examples/barrier/Makefile
    doc/examples/drom/Makefile
    doc/examples/lewi_custom_rt/run.sh
    doc/examples/lewi_custom_rt/Makefile
    doc/examples/lewi_mask_consumer/run.sh
    doc/examples/lewi_mask_consumer/Makefile
    doc/examples/mpi+omp/run.sh
    doc/examples/mpi+omp/Makefile
    doc/examples/mpi+omp_ompt/run.sh
    doc/examples/mpi+omp_ompt/Makefile
    doc/examples/mpi+omp_roleshift/run.sh
    doc/examples/mpi+omp_roleshift/Makefile
    doc/examples/mpi+ompss/run.sh
    doc/examples/mpi+ompss/Makefile
    doc/examples/named_barrier/Makefile
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
    doc/user_guide/source/conf.py
    doc/user_guide/source/index.rst
    src/apis/dlb.h
])
AC_CONFIG_FILES([tests/gens/basic-generator], [chmod +x tests/gens/basic-generator])
AC_CONFIG_FILES([tests/gens/fortran-generator], [chmod +x tests/gens/fortran-generator])
AC_CONFIG_FILES([tests/gens/mpi-generator], [chmod +x tests/gens/mpi-generator])

# Some user guide files are generated, link the rest if necessary to the build dir
AC_CONFIG_LINKS([
    doc/user_guide/source/advance_usage.rst:doc/user_guide/source/advance_usage.rst
    doc/user_guide/source/api.rst:doc/user_guide/source/api.rst
    doc/user_guide/source/citing_dlb.rst:doc/user_guide/source/citing_dlb.rst
    doc/user_guide/source/contents.rst:doc/user_guide/source/contents.rst
    doc/user_guide/source/dlb_barrier.rst:doc/user_guide/source/dlb_barrier.rst
    doc/user_guide/source/faq.rst:doc/user_guide/source/faq.rst
    doc/user_guide/source/installation.rst:doc/user_guide/source/installation.rst
    doc/user_guide/source/how_to_run.rst:doc/user_guide/source/how_to_run.rst
    doc/user_guide/source/how_to_run_lewi.rst:doc/user_guide/source/how_to_run_lewi.rst
    doc/user_guide/source/how_to_run_drom.rst:doc/user_guide/source/how_to_run_drom.rst
    doc/user_guide/source/how_to_run_talp.rst:doc/user_guide/source/how_to_run_talp.rst
    doc/user_guide/source/talp_metrics.rst:doc/user_guide/source/talp_metrics.rst
    doc/user_guide/source/talp_pages.rst:doc/user_guide/source/talp_pages.rst
    doc/user_guide/source/overview.rst:doc/user_guide/source/overview.rst
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
    doc/user_guide/source/images/talp-efficiency-models.drawio.svg:doc/user_guide/source/images/talp-efficiency-models.drawio.svg
    doc/user_guide/source/images/talp-efficiency-models.png:doc/user_guide/source/images/talp-efficiency-models.png
    doc/user_guide/source/images/ld_preload.png:doc/user_guide/source/images/ld_preload.png
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
AC_SUBST([AC_FCFLAGS])
AC_SUBST([AC_LDFLAGS])

# generate the final Makefile etc.
AC_OUTPUT

# Generate an underlined title
summary_title="$PACKAGE_STRING configuration summary"
summary_title_len="${#summary_title}"
summary_title_underline="$(printf '%*s' "$summary_title_len" | tr ' ' '=')"

# Summary
AS_ECHO(["
$summary_title
$summary_title_underline

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
  F08 bindings:   $mpi_f08
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

AS_ECHO_N(["
* PAPI: "])
AS_IF([test "x$with_papi" = xno], [
    AS_ECHO(["no"])
], [
    AS_ECHO(["yes
  PAPI_CPPFLAGS: $PAPI_CPPFLAGS
  PAPI_LDFLAGS:  $PAPI_LDFLAGS"])
])

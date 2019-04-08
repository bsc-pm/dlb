#################################################################################
#  Copyright 2009-2019 Barcelona Supercomputing Center                          #
#                                                                               #
#  This file is part of the DLB library.                                        #
#                                                                               #
#  DLB is free software: you can redistribute it and/or modify                  #
#  it under the terms of the GNU Lesser General Public License as published by  #
#  the Free Software Foundation, either version 3 of the License, or            #
#  (at your option) any later version.                                          #
#                                                                               #
#  DLB is distributed in the hope that it will be useful,                       #
#  but WITHOUT ANY WARRANTY; without even the implied warranty of               #
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
#  GNU Lesser General Public License for more details.                          #
#                                                                               #
#  You should have received a copy of the GNU Lesser General Public License     #
#  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                #
#################################################################################

ACLOCAL_AMFLAGS=-I m4

SUBDIRS = . tests

lib_LTLIBRARIES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =
END =

if SUPPORTED_SILENT_RULES
PYGEN_verbose = $(PYGEN_verbose_@AM_V@)
PYGEN_verbose_ = $(PYGEN_verbose_@AM_DEFAULT_V@)
PYGEN_verbose_0 = @echo "  PYGEN   " $@;

GEN_verbose = $(GEN_verbose_@AM_V@)
GEN_verbose_ = $(GEN_verbose_@AM_DEFAULT_V@)
GEN_verbose_0 = @echo "  GEN     " $@;

DOXYMAN_verbose = $(DOXYMAN_verbose_@AM_V@)
DOXYMAN_verbose_ = $(DOXYMAN_verbose_@AM_DEFAULT_V@)
DOXYMAN_verbose_0 = @echo "  DOXYGEN  man pages";
endif

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CFLAGS = $(AC_CFLAGS)
AM_LDFLAGS = -lrt $(AC_LDFLAGS) -avoid-version
if HAVE_HWLOC
AM_CPPFLAGS += $(HWLOC_CPPFLAGS) -DHWLOC_LIB
AM_LDFLAGS += $(HWLOC_LDFLAGS) $(HWLOC_LIBS)
endif

#################################################################################
### src                                                                       ###
#################################################################################
#********************************************************************************
# Headers
#********************************************************************************
installheaders =                     \
	src/apis/dlb.h               \
	src/apis/dlb_sp.h            \
	src/apis/dlb_stats.h         \
	src/apis/dlb_drom.h          \
	src/apis/dlb_types.h         \
	src/apis/dlb_errors.h        \
	src/apis/dlbf.h              \
	src/apis/dlbf-errors.h       \
	$(END)

dist_include_HEADERS = $(installheaders)
nodist_include_HEADERS = src/LB_MPI/MPI_interface.h \
			 src/LB_MPI/MPI_interfaceF.h

#********************************************************************************
# Built Sources
#********************************************************************************
MPI_GENERATOR = $(top_srcdir)/scripts/pygen.py
MPICALLS_DB = $(top_srcdir)/src/LB_MPI/mpicalls.json
EXTRA_DIST    += src/LB_MPI/mpicalls.json

EXTRA_DIST    += src/LB_MPI/MPI_calls_coded.h.in
CLEANFILES    += src/LB_MPI/MPI_calls_coded.h
BUILT_SOURCES += src/LB_MPI/MPI_calls_coded.h
src/LB_MPI/MPI_calls_coded.h: $(top_srcdir)/src/LB_MPI/MPI_calls_coded.h.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)

EXTRA_DIST    += src/LB_MPI/MPI_calls_coded.c.in
CLEANFILES    += src/LB_MPI/MPI_calls_coded.c
BUILT_SOURCES += src/LB_MPI/MPI_calls_coded.c
src/LB_MPI/MPI_calls_coded.c: $(top_srcdir)/src/LB_MPI/MPI_calls_coded.c.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)

EXTRA_DIST    += src/LB_MPI/MPI_interface.h.in
CLEANFILES    += src/LB_MPI/MPI_interface.h
BUILT_SOURCES += src/LB_MPI/MPI_interface.h
src/LB_MPI/MPI_interface.h: $(top_srcdir)/src/LB_MPI/MPI_interface.h.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)

EXTRA_DIST    += src/LB_MPI/MPI_interface.c.in
CLEANFILES    += src/LB_MPI/MPI_interface.c
BUILT_SOURCES += src/LB_MPI/MPI_interface.c
src/LB_MPI/MPI_interface.c: $(top_srcdir)/src/LB_MPI/MPI_interface.c.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)

EXTRA_DIST    += src/LB_MPI/MPI_interfaceF.c.in
CLEANFILES    += src/LB_MPI/MPI_interfaceF.c
BUILT_SOURCES += src/LB_MPI/MPI_interfaceF.c
src/LB_MPI/MPI_interfaceF.c: $(top_srcdir)/src/LB_MPI/MPI_interfaceF.c.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)

EXTRA_DIST    += src/LB_MPI/MPI_interfaceF.h.in
CLEANFILES    += src/LB_MPI/MPI_interfaceF.h
BUILT_SOURCES += src/LB_MPI/MPI_interfaceF.h
src/LB_MPI/MPI_interfaceF.h: $(top_srcdir)/src/LB_MPI/MPI_interfaceF.h.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)

EXTRA_DIST    += src/LB_MPI/MPI_intercept.c.in
CLEANFILES    += src/LB_MPI/MPI_intercept.c
BUILT_SOURCES += src/LB_MPI/MPI_intercept.c
src/LB_MPI/MPI_intercept.c: $(top_srcdir)/src/LB_MPI/MPI_intercept.c.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)

EXTRA_DIST    += src/LB_MPI/MPI_interceptF.c.in
CLEANFILES    += src/LB_MPI/MPI_interceptF.c
BUILT_SOURCES += src/LB_MPI/MPI_interceptF.c
src/LB_MPI/MPI_interceptF.c: $(top_srcdir)/src/LB_MPI/MPI_interceptF.c.in $(MPICALLS_DB)
	$(PYGEN_verbose)$(MPI_GENERATOR) -i $< -o $@ -j $(MPICALLS_DB)


#********************************************************************************
# libdlb.so
#********************************************************************************
COMMON_SRCS = \
	$(installheaders)                       \
	src/support/debug.c                     \
	src/support/debug.h                     \
	src/support/env.c                       \
	src/support/env.h                       \
	src/support/error.c                     \
	src/support/error.h                     \
	src/support/gtree.c                     \
	src/support/gtree.h                     \
	src/support/mask_utils.c                \
	src/support/mask_utils.h                \
	src/support/mytime.c                    \
	src/support/mytime.h                    \
	src/support/options.c                   \
	src/support/options.h                   \
	src/support/queues.c                    \
	src/support/queues.h                    \
	src/support/tracing.c                   \
	src/support/tracing.h                   \
	src/support/types.c                     \
	src/support/types.h                     \
	src/LB_numThreads/numThreads.c          \
	src/LB_numThreads/numThreads.h          \
	src/LB_numThreads/omp_thread_manager.c  \
	src/LB_numThreads/omp_thread_manager.h  \
	src/LB_numThreads/omp_tool.c            \
	src/LB_numThreads/ompt.h                \
	src/LB_comm/comm_lend_light.c           \
	src/LB_comm/comm_lend_light.h           \
	src/LB_comm/shmem.c                     \
	src/LB_comm/shmem.h                     \
	src/LB_comm/shmem_async.c               \
	src/LB_comm/shmem_async.h               \
	src/LB_comm/shmem_barrier.c             \
	src/LB_comm/shmem_barrier.h             \
	src/LB_comm/shmem_cpuinfo.c             \
	src/LB_comm/shmem_cpuinfo.h             \
	src/LB_comm/shmem_procinfo.c            \
	src/LB_comm/shmem_procinfo.h            \
	src/LB_policies/lewi.c                  \
	src/LB_policies/lewi.h                  \
	src/LB_policies/lewi_mask.c             \
	src/LB_policies/lewi_mask.h             \
	src/LB_core/DLB_kernel.c                \
	src/LB_core/DLB_kernel.h                \
	src/LB_core/lb_funcs.c                  \
	src/LB_core/lb_funcs.h                  \
	src/LB_core/spd.c                       \
	src/LB_core/spd.h                       \
	src/apis/DLB_interface.c                \
	src/apis/DLB_interfaceF.c               \
	src/apis/DLB_interface_drom.c           \
	src/apis/DLB_interface_sp.c             \
	src/apis/DLB_interface_stats.c          \
	$(END)

lib_LTLIBRARIES += libdlb.la
libdlb_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_la_SOURCES = $(COMMON_SRCS)

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_dbg.la
libdlb_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_dbg_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_dbg_la_SOURCES = $(COMMON_SRCS)
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_instr.la
libdlb_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_instr_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_instr_la_SOURCES = $(COMMON_SRCS)
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_instr_dbg.la
libdlb_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_instr_dbg_la_SOURCES = $(COMMON_SRCS)
endif

#********************************************************************************
# libdlb_mpi.so
#********************************************************************************
if MPI_LIB
MPI_SRCS = \
	src/LB_MPI/DPD.c                \
	src/LB_MPI/DPD.h                \
	src/LB_MPI/process_MPI.c        \
	src/LB_MPI/process_MPI.h        \
	$(END)

MPI_SRCS_GEN = \
	src/LB_MPI/MPI_calls_coded.c    \
	src/LB_MPI/MPI_interface.c      \
	$(END)

MPI_INTR_GEN = src/LB_MPI/MPI_intercept.c

AM_MPI_CPPFLAGS = $(MPI_CPPFLAGS) -DMPI_LIB
AM_MPI_LDFLAGS = $(MPI_LDFLAGS)

lib_LTLIBRARIES += libdlb_mpi.la
libdlb_mpi_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_la_SOURCES = $(MPI_SRCS_GEN) $(MPI_INTR_GEN)

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpi_dbg.la
libdlb_mpi_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_dbg_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_dbg_la_SOURCES = $(MPI_SRCS_GEN) $(MPI_INTR_GEN)
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_mpi_instr.la
libdlb_mpi_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_instr_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_instr_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_instr_la_SOURCES = $(MPI_SRCS_GEN)
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpi_instr_dbg.la
libdlb_mpi_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_instr_dbg_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_instr_dbg_la_SOURCES = $(MPI_SRCS_GEN)
endif
endif #MPI_LIB

#********************************************************************************
# libdlb_mpif.so
#********************************************************************************
if MPI_LIB
MPIF_SRCS = \
	src/LB_MPI/DPD.c                \
	src/LB_MPI/DPD.h                \
	src/LB_MPI/process_MPI.c        \
	src/LB_MPI/process_MPI.h        \
	$(END)

MPIF_SRCS_GEN = \
	src/LB_MPI/MPI_calls_coded.c    \
	src/LB_MPI/MPI_interfaceF.c     \
	$(END)

MPIF_INTR_GEN = src/LB_MPI/MPI_interceptF.c

lib_LTLIBRARIES += libdlb_mpif.la
libdlb_mpif_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_la_SOURCES = $(MPIF_SRCS_GEN) $(MPIF_INTR_GEN)

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpif_dbg.la
libdlb_mpif_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_dbg_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_dbg_la_SOURCES = $(MPIF_SRCS_GEN) $(MPIF_INTR_GEN)
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_mpif_instr.la
libdlb_mpif_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_instr_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_instr_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_instr_la_SOURCES = $(MPIF_SRCS_GEN)
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpif_instr_dbg.la
libdlb_mpif_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_instr_dbg_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_instr_dbg_la_SOURCES = $(MPIF_SRCS_GEN)
endif
endif #MPI_LIB

#********************************************************************************
# Support binaries
#********************************************************************************
bin_PROGRAMS = dlb dlb_run dlb_shm dlb_taskset

dlb_SOURCES = src/utils/dlb.c
dlb_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
dlb_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
dlb_LDADD = libdlb.la

dlb_run_SOURCES = src/utils/dlb_run.c
dlb_run_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
dlb_run_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
dlb_run_LDADD = libdlb.la

dlb_shm_SOURCES = src/utils/dlb_shm.c
dlb_shm_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
dlb_shm_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
dlb_shm_LDADD = libdlb.la

dlb_taskset_SOURCES = src/utils/dlb_taskset.c
dlb_taskset_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
dlb_taskset_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
dlb_taskset_LDADD = libdlb.la

noinst_PROGRAMS = dlb_tester
dlb_tester_SOURCES = src/utils/dlb_tester.c
dlb_tester_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
dlb_tester_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS) $(OPENMP_CFLAGS)
dlb_tester_LDADD = libdlb.la

#################################################################################
### doc                                                                       ###
#################################################################################
#********************************************************************************
# Paraver cfgs
#********************************************************************************
paraver_cfgs = \
	doc/paraver_cfgs/DLB_cpus(4).cfg \
	doc/paraver_cfgs/DLB_cpus(8).cfg \
	doc/paraver_cfgs/DLB_cpus(16).cfg \
	doc/paraver_cfgs/DLB_idle_cpus.cfg \
	doc/paraver_cfgs/DLB_mode.cfg \
	doc/paraver_cfgs/DLB_runtime.cfg \
	doc/paraver_cfgs/DLB_num_threads.cfg \
	doc/paraver_cfgs/DLB_rebind_threads.cfg \
	$(END)

paravercfgsdir = $(datadir)/paraver_cfgs/DLB
dist_paravercfgs_DATA = $(paraver_cfgs)

#********************************************************************************
# Examples
#********************************************************************************
dromexamples_files = \
	doc/examples/drom/drom_01.c \
	doc/examples/drom/README \
	$(END)

dromexamples_files_nodist = \
	doc/examples/drom/Makefile \
	$(END)

dromexamplesdir = $(docdir)/examples/DROM
dist_dromexamples_DATA = $(dromexamples_files)
nodist_dromexamples_DATA = $(dromexamples_files_nodist)

mpiompexamples_files = \
	doc/examples/mpi+omp/README \
	doc/examples/mpi+omp/mpi_omp_pils.c \
	doc/examples/mpi+omp/extrae.xml \
	$(END)

mpiompexamples_files_nodist = \
	doc/examples/mpi+omp/Makefile \
	doc/examples/mpi+omp/run.sh \
	$(END)

mpiompexamplesdir = $(docdir)/examples/MPI+OMP
dist_mpiompexamples_DATA = $(mpiompexamples_files)
nodist_mpiompexamples_DATA = $(mpiompexamples_files_nodist)

mpiompssexamples_files = \
	doc/examples/mpi+ompss/README \
	doc/examples/mpi+ompss/Makefile \
	doc/examples/mpi+ompss/mpi_ompss_pils.c \
	doc/examples/mpi+ompss/run.sh \
	doc/examples/mpi+ompss/extrae.xml \
	$(END)

mpiompssexamplesdir = $(docdir)/examples/MPI+OmpSs
dist_mpiompssexamples_DATA = $(mpiompssexamples_files)

statsexamples_files = \
	doc/examples/statistics/README \
	doc/examples/statistics/get_cpu_usage.c \
	doc/examples/statistics/get_pid_list.c \
	doc/examples/statistics/mpi_ompss_pils.c \
	doc/examples/statistics/run.sh \
	$(END)

statsexamples_files_nodist = \
	doc/examples/statistics/Makefile \
	$(END)

statsexamplesdir = $(docdir)/examples/statistics
dist_statsexamples_DATA = $(statsexamples_files)
nodist_statsexamples_DATA = $(statsexamples_files_nodist)

#********************************************************************************
# User Guide
#********************************************************************************
sphinx_files = \
	doc/user_guide/source/conf.py \
	doc/user_guide/source/advance_usage.rst \
	doc/user_guide/source/api.rst \
	doc/user_guide/source/contents.rst \
	doc/user_guide/source/faq.rst \
	doc/user_guide/source/how_to_install.rst \
	doc/user_guide/source/how_to_run.rst \
	doc/user_guide/source/index.rst \
	doc/user_guide/source/intro.rst \
	doc/user_guide/source/technical_requirements.rst \
	doc/user_guide/source/images/dlb_logo.png \
	doc/user_guide/source/images/dlb_logo_white.png \
	doc/user_guide/source/images/dlb_states.png \
	doc/user_guide/source/images/dlb_icon.png \
	doc/user_guide/source/images/dlb_icon.ico \
	doc/user_guide/source/images/drom.png \
	doc/user_guide/source/images/hpc_app.png \
	doc/user_guide/source/images/LeWI.png \
	doc/user_guide/source/_static/custom.css \
	doc/user_guide/Makefile

EXTRA_DIST += $(sphinx_files)

if HAVE_SPHINX
SPHINXBUILD = sphinx-build
SPHINX_SRC_DIR = $(srcdir)/doc/user_guide/source/

doc: html pdf

html-local:
	$(SPHINXBUILD) -b html $(SPHINX_SRC_DIR) doc/user_guide/html
	@echo
	@echo "Build finished. The HTML pages are in doc/user_guide/html"

install-html-local: html-local
	$(mkdir_p) $(docdir)
	cp -R doc/user_guide/html $(docdir)

pdf-local:
	$(SPHINXBUILD) -b latex $(SPHINX_SRC_DIR) doc/user_guide/pdf
	@echo "Running LaTeX files through pdflatex..."
	$(MAKE) -C doc/user_guide/pdf all-pdf
	@echo "pdflatex finished; the PDF files are in doc/user_guide/pdf"

install-pdf-local: pdf-local
	$(mkdir_p) $(docdir)
	cp doc/user_guide/pdf/DLBUserGuide.pdf $(docdir)
else
nodoc:
	@echo "  SPHINX (http://sphinx-doc.org/) was not detected at configure time."
	@echo "  PDF and HTML generation is disabled"
doc: nodoc
html-local: nodoc
install-html-local: nodoc
pdf-local: nodoc
install-pdf-local: nodoc
endif

#********************************************************************************
# Doxygen
#********************************************************************************
if HAVE_DOXYGEN
DOXYGEN = doxygen
DOXYFILE = $(builddir)/doc/doxygen/Doxyfile

doxygen:
	$(DOXYGEN) $(DOXYFILE)
else
doxygen:
	@echo "  doxygen (hhtp://www.doxygen.org) was not detected at configure time."
endif

#********************************************************************************
# Man pages
#********************************************************************************
if HAVE_DOXYGEN
DOXYFILE_MAN = $(builddir)/doc/doxygen/Doxyfile_man
manpagesbuilddir = $(builddir)/doc/doxygen/man/man3

dist_man_MANS = \
	$(manpagesbuilddir)/dlb.h.3 \
	$(manpagesbuilddir)/dlb_drom.h.3 \
	$(manpagesbuilddir)/dlb_sp.h.3 \
	$(manpagesbuilddir)/dlb_stats.h.3 \
	$(manpagesbuilddir)/dlb_types.h.3 \
	$(manpagesbuilddir)/dlb_errors.h.3 \
	$(END)

$(manpagesbuilddir)/doxy.stamp: $(installheaders)
	$(DOXYMAN_verbose)$(DOXYGEN) $(DOXYFILE_MAN)
	@touch $(manpagesbuilddir)/doxy.stamp

$(manpagesbuilddir)/dlb.h.3: $(manpagesbuilddir)/doxy.stamp
$(manpagesbuilddir)/dlb_drom.h.3: $(manpagesbuilddir)/doxy.stamp
$(manpagesbuilddir)/dlb_sp.h.3: $(manpagesbuilddir)/doxy.stamp
$(manpagesbuilddir)/dlb_stats.h.3: $(manpagesbuilddir)/doxy.stamp
$(manpagesbuilddir)/dlb_types.h.3: $(manpagesbuilddir)/doxy.stamp
$(manpagesbuilddir)/dlb_errors.h.3: $(manpagesbuilddir)/doxy.stamp

CLEANFILES += $(manpagesbuilddir)/doxy.stamp
endif

#********************************************************************************
# Clean doc
#********************************************************************************
clean-doc:
	rm -rf doc/user_guide/build doc/user_guide/html doc/user_guide/pdf
	rm -rf doc/doxygen/html doc/doxygen/man

#################################################################################
### scripts                                                                   ###
#################################################################################
python_PYTHON = scripts/viewer/dlb_wrapper.py \
		scripts/viewer/progressmeter.py

nodist_python_PYTHON = scripts/viewer/dlb_viewer.py

nodist_bin_SCRIPTS = scripts/viewer/dlb_cpu_usage \
		     scripts/run_with_dlb.sh

DEBIAN_EXTRA = scripts/debian/compat \
	       scripts/debian/source/format \
	       scripts/debian/rules \
	       scripts/debian/control \
	       $(END)

LIT_EXTRA = scripts/lit/PKG-INFO \
	    scripts/lit/README.txt \
	    scripts/lit/lit.py \
	    scripts/lit/lit/BooleanExpression.py \
	    scripts/lit/lit/LitConfig.py \
	    scripts/lit/lit/LitTestCase.py \
	    scripts/lit/lit/ProgressBar.py \
	    scripts/lit/lit/ShCommands.py \
	    scripts/lit/lit/ShUtil.py \
	    scripts/lit/lit/Test.py \
	    scripts/lit/lit/TestRunner.py \
	    scripts/lit/lit/TestingConfig.py \
	    scripts/lit/lit/__init__.py \
	    scripts/lit/lit/discovery.py \
	    scripts/lit/lit/main.py \
	    scripts/lit/lit/run.py \
	    scripts/lit/lit/util.py \
	    scripts/lit/lit/builtin_commands/__init__.py \
	    scripts/lit/lit/builtin_commands/cat.py \
	    scripts/lit/lit/formats/__init__.py \
	    scripts/lit/lit/formats/base.py \
	    scripts/lit/lit/formats/googletest.py \
	    scripts/lit/lit/formats/shtest.py \
	    scripts/lit/lit/llvm/__init__.py \
	    scripts/lit/lit/llvm/config.py \
	    scripts/lit/lit/llvm/subst.py
	    $(END)

EXTRA_DIST += \
	scripts/bets \
	scripts/run_with_dlb.sh.in \
	scripts/dlb.spec \
	scripts/pygen.py \
	scripts/viewer/dlb_cpu_usage.in \
	scripts/viewer/dlb_viewer.py.in \
	$(DEBIAN_EXTRA) \
	$(LIT_EXTRA) \
	$(END)

CLEANFILES    += scripts/viewer/dlb_viewer.py
scripts/viewer/dlb_viewer.py: $(top_srcdir)/scripts/viewer/dlb_viewer.py.in
	$(GEN_verbose)$(MKDIR_P) scripts/viewer; \
	rm -f $@; \
	sed -e's,@PREFIX\@,"$(prefix)",g' $^ > $@

CLEANFILES    += scripts/viewer/dlb_cpu_usage
scripts/viewer/dlb_cpu_usage: $(top_srcdir)/scripts/viewer/dlb_cpu_usage.in
	$(GEN_verbose)$(MKDIR_P) scripts/viewer; \
	rm -f $@; \
	sed -e's,@PYTHONDIR\@,"$(pythondir)",g' $^ > $@

CLEANFILES    += scripts/run_with_dlb.sh
scripts/run_with_dlb.sh: $(top_srcdir)/scripts/run_with_dlb.sh.in
	$(GEN_verbose)$(MKDIR_P) scripts/; \
	rm -f $@; \
	sed \
		-e's,@PREFIX_LIB\@,"$(libdir)",g' \
		-e's,@NCPUS\@,$(NCPUS),g' \
	$^ > $@.tmp; \
	mv $@.tmp $@; \
	chmod 755 $@

rpm: dist-gzip
	@echo "Generating RPM structure"
	cd scripts; \
	$(MKDIR_P) SOURCES BUILD RPMS SPECS SRPMS; \
	cp ../$(distdir).tar.gz SOURCES; \
	THREADS=$$(getconf _NPROCESSORS_ONLN); THREADS=$${THREADS:-1};\
	rpmbuild -v -bb --clean $(abs_top_srcdir)/scripts/dlb.spec --define '_topdir $$(pwd)' \
		--define 'version $(VERSION)'\
		--define 'release $(shell date +%Y%m%d)'\
		--define "threads $${THREADS}"\
		--define 'configure_options CFLAGS=-D_FORTIFY_SOURCE=0'

clean-rpm:
	cd scripts; \
	rm -rf BUILDROOT SOURCES BUILD RPMS SPECS SRPMS

deb: dist-gzip
	cd scripts; \
	cp ../$(distdir).tar.gz $(PACKAGE_TARNAME)_$(VERSION).orig.tar.gz; \
	tar -xf $(PACKAGE_TARNAME)_$(VERSION).orig.tar.gz; \
	rm -rf upstream_dir; \
	mv $(PACKAGE_TARNAME)-$(VERSION) upstream_dir; \
	cp -rf $(abs_top_srcdir)/scripts/debian upstream_dir/; \
	cp -rf upstream_dir/COPYING upstream_dir/debian/copyright; \
	rm -f upstream_dir/debian/changelog; \
	if test ! -d $(abs_top_srcdir)/.git; \
	then dch --create -c upstream_dir/debian/changelog --empty --package $(PACKAGE_TARNAME) -v "$(VERSION)-$(shell date +%Y%m%d)$(DEB_RELEASE)"; \
	else git --git-dir="$(abs_top_srcdir)/.git" --work-tree="$(abs_top_srcdir)/.git" log -1 --pretty=format:"$(PACKAGE_TARNAME) ($(VERSION)-$(shell date +%Y%m%d)$(DEB_RELEASE)) unstable; urgency=low%x0A%x0A  * %h %s%x0A%x0A -- %an <%ae>  %aD" > upstream_dir/debian/changelog; \
	fi; \
	THREADS=$$(getconf _NPROCESSORS_ONLN); THREADS=$${THREADS:-1}; \
	cd upstream_dir; DEB_BUILD_OPTIONS="nocheck parallel=$${THREADS}" debuild -us -uc; \
	rm -rf upstream_dir

clean-deb:
	cd scripts; \
	rm -rf $(PACKAGE_TARNAME)_$(VERSION).orig.tar.gz $(PACKAGE_TARNAME)_$(VERSION)*.debian.tar.gz \
		$(PACKAGE_TARNAME)_$(VERSION)*.dsc $(PACKAGE_TARNAME)_$(DEB_RELEASE)*.build \
		$(PACKAGE_TARNAME)_$(DEB_RELEASE)*.changes *.deb


#################################################################################
### tests                                                                     ###
#################################################################################
EXTRA_DIST += tests/test \
	      tests/gens/config.py \
	      tests/lit.site.cfg \
	      tests/litsupport/__init__.py \
	      tests/litsupport/bets.py
CLEANFILES += tests/test.log tests/test.log.xml

coverage-local: all-am


#################################################################################
### Hooks                                                                     ###
#################################################################################

clean-local: clean-rpm clean-deb clean-doc

install-data-hook:
	chmod +x $(mpiompexamplesdir)/run.sh
	chmod +x $(mpiompssexamplesdir)/run.sh
	chmod +x $(statsexamplesdir)/run.sh

dist-hook:
	if [ -x "$(GIT)" ]; \
	then \
	    "$(GIT)" --git-dir=$(top_srcdir)/.git log --pretty=format:"[%h] %cd : %s (%an)" --date=short > $(distdir)/ChangeLog; \
	    git_run_version=`"$(GIT)" --git-dir=$(top_srcdir)/.git show --pretty=format:"%h %ci" HEAD | head -n 1`; \
	    git_run_branch=`"$(GIT)" --git-dir=$(top_srcdir)/.git branch | grep ^* | sed s/*\ //g`; \
	    git_version="git $${git_run_branch} $${git_run_version}"; \
	    echo $${git_version} > ${distdir}/VERSION; \
	elif [ -e $(top_srcdir)/VERSION ]; \
	then \
	    cp $(top_srcdir)/VERSION $(distdir)/VERSION; \
	fi

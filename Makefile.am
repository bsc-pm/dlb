#################################################################################
#  Copyright 2015 Barcelona Supercomputing Center                               #
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
#  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 #
#################################################################################

ACLOCAL_AMFLAGS=-I m4

SUBDIRS = . tests

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
endif

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CFLAGS = -fvisibility=hidden
AM_LDFLAGS = -lrt $(AC_LDFLAGS) -avoid-version
if HAVE_HWLOC
AM_CPPFLAGS += $(HWLOC_CPPFLAGS)
AM_LDFLAGS += $(HWLOC_LDFLAGS) $(HWLOC_LIBS)
endif

##########################################################################
# src                                                                  ###
##########################################################################
##########################################################################
# Headers
##########################################################################
dist_include_HEADERS = src/LB_core/DLB_interface.h
nodist_include_HEADERS = src/LB_MPI/MPI_interface.h \
			 src/LB_MPI/MPI_interfaceF.h
EXTRA_DIST += src/support/dlb_api.h

##########################################################################
# Built Sources
##########################################################################
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


##########################################################################
# libdlb_dummy.so
##########################################################################
lib_LTLIBRARIES = libdlb_dummy.la
libdlb_dummy_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_dummy_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_dummy_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_dummy_la_SOURCES = src/LB_core/DLB_dummy_interface.c

##########################################################################
# libdlb.so
##########################################################################
COMMON_SRCS = \
	src/DPD/DPD.c                   \
	src/DPD/DPD.h                   \
	src/LB_numThreads/numThreads.c  \
	src/LB_numThreads/numThreads.h  \
	src/LB_policies/JustProf.c      \
	src/LB_policies/JustProf.h      \
	src/LB_policies/Lend_light.c    \
	src/LB_policies/Lend_light.h    \
	src/LB_policies/Lewi_map.c      \
	src/LB_policies/Lewi_map.h      \
	src/LB_policies/Weight.c        \
	src/LB_policies/Weight.h        \
	src/LB_policies/lewi_mask.c     \
	src/LB_policies/autonomous_lewi_mask.h     \
	src/LB_policies/autonomous_lewi_mask.c     \
	src/LB_policies/lewi_mask.h     \
	src/LB_policies/RaL.c           \
	src/LB_policies/RaL.h           \
	src/LB_policies/PERaL.c         \
	src/LB_policies/PERaL.h         \
	src/LB_comm/comm_shMem.c        \
	src/LB_comm/comm.h              \
	src/LB_comm/comm_lend_light.c   \
	src/LB_comm/comm_lend_light.h   \
	src/LB_comm/comm_map.c          \
	src/LB_comm/comm_map.h          \
	src/LB_comm/shmem.c             \
	src/LB_comm/shmem.h             \
	src/LB_comm/shmem_bitset.c      \
	src/LB_comm/shmem_bitset.h      \
	src/LB_comm/shmem_cpuarray.c    \
	src/LB_comm/shmem_cpuarray.h    \
	src/LB_comm/shmem_stats.c       \
	src/LB_comm/shmem_stats.h       \
	src/LB_comm/shmem_drom.c        \
	src/LB_comm/shmem_drom.h        \
	src/LB_core/DLB_kernel.c        \
	src/LB_core/DLB_kernel.h        \
	src/LB_core/DLB_interface.c     \
	src/LB_core/DLB_interface.h     \
	src/LB_core/DLB_interfaceF.c    \
	src/LB_core/statistics.c        \
	src/LB_core/statistics.h        \
	src/LB_core/drom.c              \
	src/LB_core/drom.h              \
	src/support/sighandler.c        \
	src/support/sighandler.h        \
	src/support/globals.c           \
	src/support/globals.h           \
	src/support/debug.c             \
	src/support/debug.h             \
	src/support/tracing.c           \
	src/support/tracing.h           \
	src/support/mytime.c            \
	src/support/mytime.h            \
	src/support/mask_utils.c        \
	src/support/mask_utils.h        \
	src/support/options.c           \
	src/support/options.h           \
	src/support/types.c             \
	src/support/types.h             \
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

##########################################################################
# libdlb_mpi.so
##########################################################################
if MPI_LIB
MPI_SRCS = \
	src/LB_MPI/process_MPI.c        \
	src/LB_MPI/process_MPI.h        \
	$(END)

MPI_SRCS_GEN = \
	src/LB_MPI/MPI_calls_coded.c    \
	src/LB_MPI/MPI_interface.c      \
	$(END)

MPI_INTR_GEN = src/LB_MPI/MPI_intercept.c

AM_MPI_CPPFLAGS = $(MPI_CPPFLAGS) -DMPI_LIB
AM_MPI_LDFLAGS = $(MPI_LDFLAGS) -R $(MPI_LIBSDIR)
AM_MPI_LIBADD = $(MPI_LIBS)

lib_LTLIBRARIES += libdlb_mpi.la
libdlb_mpi_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpi_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_la_SOURCES = $(MPI_SRCS_GEN) $(MPI_INTR_GEN)

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpi_dbg.la
libdlb_mpi_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_dbg_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpi_dbg_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_dbg_la_SOURCES = $(MPI_SRCS_GEN) $(MPI_INTR_GEN)
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_mpi_instr.la
libdlb_mpi_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_instr_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_instr_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpi_instr_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_instr_la_SOURCES = $(MPI_SRCS_GEN)
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpi_instr_dbg.la
libdlb_mpi_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_instr_dbg_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpi_instr_dbg_la_SOURCES = $(MPI_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpi_instr_dbg_la_SOURCES = $(MPI_SRCS_GEN)
endif
endif #MPI_LIB

##########################################################################
# libdlb_mpif.so
##########################################################################
if MPI_LIB
MPIF_SRCS = \
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
libdlb_mpif_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpif_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_la_SOURCES = $(MPIF_SRCS_GEN) $(MPIF_INTR_GEN)

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpif_dbg.la
libdlb_mpif_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_dbg_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpif_dbg_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_dbg_la_SOURCES = $(MPIF_SRCS_GEN) $(MPIF_INTR_GEN)
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_mpif_instr.la
libdlb_mpif_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_instr_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_instr_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpif_instr_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_instr_la_SOURCES = $(MPIF_SRCS_GEN)
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpif_instr_dbg.la
libdlb_mpif_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_instr_dbg_la_LIBADD = $(AM_MPI_LIBADD)
libdlb_mpif_instr_dbg_la_SOURCES = $(MPIF_SRCS) $(COMMON_SRCS)
nodist_libdlb_mpif_instr_dbg_la_SOURCES = $(MPIF_SRCS_GEN)
endif
endif #MPI_LIB

##########################################################################
# Support binaries
##########################################################################
bin_PROGRAMS = dlb dlb_shm dlb_taskset

dlb_SOURCES = src/utils/dlb.c
dlb_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
dlb_LDADD = libdlb.la

dlb_shm_SOURCES = src/utils/dlb_shm.c
dlb_shm_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
dlb_shm_LDADD = libdlb.la

dlb_taskset_SOURCES = src/utils/dlb_taskset.c
dlb_taskset_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
dlb_taskset_LDADD = libdlb.la

##########################################################################
# doc                                                                  ###
##########################################################################
paraver_cfgs = \
	doc/paraver_cfgs/DLB_cpus(4).cfg \
	doc/paraver_cfgs/DLB_cpus(8).cfg \
	doc/paraver_cfgs/DLB_cpus(16).cfg \
	doc/paraver_cfgs/DLB_idle_cpus.cfg \
	doc/paraver_cfgs/DLB_mode.cfg \
	doc/paraver_cfgs/DLB_runtime.cfg \
	doc/paraver_cfgs/DLB_num_threads.cfg \
	$(END)

paravercfgsdir = $(datadir)/paraver_cfgs/DLB
dist_paravercfgs_DATA = $(paraver_cfgs)

mpiompexamples_files = \
	doc/examples/mpi+omp/README \
	doc/examples/mpi+omp/mpi_omp_pils.c \
	doc/examples/mpi+omp/extrae.xml \
	$(END)

mpiompexamples_files_nodist = \
	doc/examples/mpi+omp/Makefile \
	doc/examples/mpi+omp/run.sh \
	$(END)

EXTRA_DIST += doc/examples/mpi+omp/Makefile.in
CLEANFILES += doc/examples/mpi+omp/Makefile
doc/examples/mpi+omp/Makefile: $(top_srcdir)/doc/examples/mpi+omp/Makefile.in
	$(GEN_verbose)$(MKDIR_P) doc/examples/mpi+omp; \
	rm -f $@; \
	sed \
		-e's,@PREFIX\@,"$(prefix)",g' \
	$^ > $@;

EXTRA_DIST += doc/examples/mpi+omp/run.sh.in
CLEANFILES += doc/examples/mpi+omp/run.sh
doc/examples/mpi+omp/run.sh: $(top_srcdir)/doc/examples/mpi+omp/run.sh.in
	$(GEN_verbose)$(MKDIR_P) doc/examples/mpi+omp; \
	rm -f $@; \
	sed \
		-e's,@PREFIX\@,"$(prefix)",g' \
	$^ > $@;

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

EXTRA_DIST += doc/examples/statistics/Makefile.in
CLEANFILES += doc/examples/statistics/Makefile
doc/examples/statistics/Makefile: $(top_srcdir)/doc/examples/statistics/Makefile.in
	$(GEN_verbose)$(MKDIR_P) doc/examples/statistics; \
	rm -f $@; \
	sed \
		-e's,@PREFIX\@,"$(prefix)",g' \
	$^ > $@;

statsexamplesdir = $(docdir)/examples/statistics
dist_statsexamples_DATA = $(statsexamples_files)
nodist_statsexamples_DATA = $(statsexamples_files_nodist)


sphinx_files = \
	doc/user_guide/source/conf.py \
	doc/user_guide/source/api.rst \
	doc/user_guide/source/examples.rst \
	doc/user_guide/source/how_to_install.rst \
	doc/user_guide/source/how_to_run.rst \
	doc/user_guide/source/index.rst \
	doc/user_guide/source/intro.rst \
	doc/user_guide/source/images/dlb_logo.png \
	doc/user_guide/source/images/dlb_logo_white.png \
	doc/user_guide/source/images/dlb_states.png \
	doc/user_guide/source/images/dlb_icon.png \
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

clean-doc:
	rm -rf doc/user_guide/build doc/user_guide/html doc/user_guide/pdf


##########################################################################
# scripts                                                              ###
##########################################################################
python_PYTHON = scripts/viewer/dlb_wrapper.py \
		scripts/viewer/progressmeter.py

nodist_python_PYTHON = scripts/viewer/dlb_viewer.py

nodist_bin_SCRIPTS = scripts/viewer/dlb_cpu_usage \
		     scripts/run_with_dlb.sh

EXTRA_DIST += \
	scripts/bets \
	scripts/run_with_dlb.sh.in \
	scripts/dlb.spec \
	scripts/pygen.py \
	scripts/debian/compat \
	scripts/debian/source/format \
	scripts/debian/rules \
	scripts/debian/control \
	scripts/viewer/dlb_cpu_usage.in \
	scripts/viewer/dlb_viewer.py.in \
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


##########################################################################
# Hooks                                                                ###
##########################################################################

clean-local: clean-rpm clean-deb clean-doc

dist-hook:
	if [ -x "$(GIT)" ]; \
	then \
	    "$(GIT)" --git-dir=$(top_srcdir)/.git log --pretty=format:"[%h] %cd : %s (%an)" --date=short > $(distdir)/ChangeLog; \
	    git_run_version=`"$(GIT)" --git-dir=$(top_srcdir)/.git show --pretty=format:"%h %ci" HEAD | head -n 1`; \
	    git_run_branch=`"$(GIT)" --git-dir=$(top_srcdir)/.git branch | grep ^* | sed s/*\ //g`; \
	    git_version="git $${git_run_branch} $${git_run_version} developer version"; \
	    echo $${git_version} > ${distdir}/VERSION; \
	elif [ -e $(top_srcdir)/VERSION ]; \
	then \
	    cp $(top_srcdir)/VERSION $(distdir)/VERSION; \
	fi

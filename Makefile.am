#################################################################################
#  Copyright 2009-2021 Barcelona Supercomputing Center                          #
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
noinst_LTLIBRARIES =
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

DOXYMAN1_verbose = $(DOXYMAN1_verbose_@AM_V@)
DOXYMAN1_verbose_ = $(DOXYMAN1_verbose_@AM_DEFAULT_V@)
DOXYMAN1_verbose_0 = @echo "  DOXYGEN  man1 pages";

DOXYMAN3_verbose = $(DOXYMAN3_verbose_@AM_V@)
DOXYMAN3_verbose_ = $(DOXYMAN3_verbose_@AM_DEFAULT_V@)
DOXYMAN3_verbose_0 = @echo "  DOXYGEN  man3 pages";

PANDOC_verbose = $(PANDOC_verbose_@AM_V@)
PANDOC_verbose_ = $(PANDOC_verbose_@AM_DEFAULT_V@)
PANDOC_verbose_0 = @echo "  PANDOC  " $@;
endif

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_builddir)/src
AM_CFLAGS = $(AC_CFLAGS)
AM_LDFLAGS = $(AC_LDFLAGS) -avoid-version
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
	src/apis/dlb_talp.h          \
	src/apis/dlbf_talp.h         \
	$(END)

dist_include_HEADERS = $(installheaders)
nodist_include_HEADERS = src/LB_MPI/MPI_interface.h \
			 src/LB_MPI/MPI_interfaceF.h

#********************************************************************************
# Built Sources
#********************************************************************************
MPI_GENERATOR = $(PYTHON) $(top_srcdir)/scripts/pygen.py
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
# libdlb_common.la
#********************************************************************************
# These sources do not depend on any MPI nor instrumentation version, only debug
COMMON_SRCS = \
	$(installheaders)                       \
	src/support/atomic.h                    \
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
	src/support/small_array.h               \
	src/support/talp_output.c               \
	src/support/talp_output.h               \
	src/support/types.c                     \
	src/support/types.h                     \
	src/LB_comm/shmem.c                     \
	src/LB_comm/shmem.h                     \
	src/LB_comm/shmem_async.c               \
	src/LB_comm/shmem_async.h               \
	src/LB_comm/shmem_barrier.c             \
	src/LB_comm/shmem_barrier.h             \
	src/LB_core/lb_funcs.c                  \
	src/LB_core/lb_funcs.h                  \
	src/LB_core/spd.c                       \
	src/LB_core/spd.h                       \
	src/apis/DLB_interface.c                \
	src/apis/DLB_interfaceF.c               \
	src/apis/DLB_interface_drom.c           \
	src/apis/DLB_interface_sp.c             \
	src/apis/DLB_interface_stats.c          \
	src/apis/DLB_interface_talp.c           \
	$(END)

noinst_LTLIBRARIES += libdlb_common.la
libdlb_common_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_common_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_common_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_common_la_SOURCES = $(COMMON_SRCS)

if DEBUG_LIB
noinst_LTLIBRARIES += libdlb_common_dbg.la
libdlb_common_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_common_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_common_dbg_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_common_dbg_la_SOURCES = $(COMMON_SRCS)
endif

#********************************************************************************
# libdlb.so
#********************************************************************************
SRCS = \
	src/support/debug.c                     \
	src/support/debug.h                     \
	src/support/tracing.c                   \
	src/support/tracing.h                   \
	src/LB_numThreads/numThreads.c          \
	src/LB_numThreads/numThreads.h          \
	src/LB_numThreads/omp_thread_manager.c  \
	src/LB_numThreads/omp_thread_manager.h  \
	src/LB_numThreads/omp_tool.c            \
	src/LB_numThreads/ompt.h                \
	src/LB_comm/comm_lend_light.c           \
	src/LB_comm/comm_lend_light.h           \
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
	src/LB_core/DLB_talp.c                  \
	src/LB_core/DLB_talp.h                  \
	$(END)

lib_LTLIBRARIES += libdlb.la
libdlb_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_la_SOURCES = $(SRCS)
libdlb_la_LIBADD = libdlb_common.la

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_dbg.la
libdlb_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_dbg_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_dbg_la_SOURCES = $(SRCS)
libdlb_dbg_la_LIBADD = libdlb_common_dbg.la
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_instr.la
libdlb_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_instr_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_instr_la_SOURCES = $(SRCS)
libdlb_instr_la_LIBADD = libdlb_common.la
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_instr_dbg.la
libdlb_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS)
libdlb_instr_dbg_la_SOURCES = $(SRCS)
libdlb_instr_dbg_la_LIBADD = libdlb_common_dbg.la
endif

#********************************************************************************
# libdlb_mpi_common.la
#********************************************************************************
if MPI_LIB
MPI_COMMON_SRCS = \
	src/LB_MPI/DPD.c                \
	src/LB_MPI/DPD.h                \
	src/LB_MPI/process_MPI.c        \
	src/LB_MPI/process_MPI.h        \
	$(END)

MPI_COMMON_SRCS_GEN = \
	src/LB_MPI/MPI_calls_coded.c    \
	$(END)

AM_MPI_CPPFLAGS = $(MPI_CPPFLAGS) -DMPI_LIB
AM_MPI_LDFLAGS = $(MPI_LDFLAGS)

noinst_LTLIBRARIES += libdlb_mpi_common.la
libdlb_mpi_common_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_common_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_common_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_common_la_SOURCES = $(MPI_COMMON_SRCS) $(SRCS)
nodist_libdlb_mpi_common_la_SOURCES = $(MPI_COMMON_SRCS_GEN)
libdlb_mpi_common_la_LIBADD = libdlb_common.la

if DEBUG_LIB
noinst_LTLIBRARIES += libdlb_mpi_common_dbg.la
libdlb_mpi_common_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_common_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_common_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_common_dbg_la_SOURCES = $(MPI_COMMON_SRCS) $(SRCS)
nodist_libdlb_mpi_common_dbg_la_SOURCES = $(MPI_COMMON_SRCS_GEN)
libdlb_mpi_common_dbg_la_LIBADD = libdlb_common_dbg.la
endif

if INSTRUMENTATION_LIB
noinst_LTLIBRARIES += libdlb_mpi_common_instr.la
libdlb_mpi_common_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_common_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_common_instr_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_common_instr_la_SOURCES = $(MPI_COMMON_SRCS) $(SRCS)
nodist_libdlb_mpi_common_instr_la_SOURCES = $(MPI_COMMON_SRCS_GEN)
libdlb_mpi_common_instr_la_LIBADD = libdlb_common.la
endif

if INSTRUMENTATION_DEBUG_LIB
noinst_LTLIBRARIES += libdlb_mpi_common_instr_dbg.la
libdlb_mpi_common_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_common_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_common_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_common_instr_dbg_la_SOURCES = $(MPI_COMMON_SRCS) $(SRCS)
nodist_libdlb_mpi_common_instr_dbg_la_SOURCES = $(MPI_COMMON_SRCS_GEN)
libdlb_mpi_common_instr_dbg_la_LIBADD = libdlb_common_dbg.la
endif
endif #MPI_LIB

#********************************************************************************
# libdlb_mpi.so
#********************************************************************************
if MPI_LIB

MPI_SRCS_GEN = src/LB_MPI/MPI_interface.c
MPI_INTR_GEN = src/LB_MPI/MPI_intercept.c

lib_LTLIBRARIES += libdlb_mpi.la
libdlb_mpi_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_la_SOURCES =
nodist_libdlb_mpi_la_SOURCES = $(MPI_SRCS_GEN) $(MPI_INTR_GEN)
libdlb_mpi_la_LIBADD = libdlb_mpi_common.la

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpi_dbg.la
libdlb_mpi_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_dbg_la_SOURCES =
nodist_libdlb_mpi_dbg_la_SOURCES = $(MPI_SRCS_GEN) $(MPI_INTR_GEN)
libdlb_mpi_dbg_la_LIBADD = libdlb_mpi_common_dbg.la
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_mpi_instr.la
libdlb_mpi_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_instr_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_instr_la_SOURCES =
nodist_libdlb_mpi_instr_la_SOURCES = $(MPI_SRCS_GEN)
libdlb_mpi_instr_la_LIBADD = libdlb_mpi_common_instr.la
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpi_instr_dbg.la
libdlb_mpi_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpi_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpi_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpi_instr_dbg_la_SOURCES =
nodist_libdlb_mpi_instr_dbg_la_SOURCES = $(MPI_SRCS_GEN)
libdlb_mpi_instr_dbg_la_LIBADD = libdlb_mpi_common_instr_dbg.la
endif
endif #MPI_LIB

#********************************************************************************
# libdlb_mpif.so
#********************************************************************************
if MPI_LIB

MPIF_SRCS_GEN = src/LB_MPI/MPI_interfaceF.c
MPIF_INTR_GEN = src/LB_MPI/MPI_interceptF.c

lib_LTLIBRARIES += libdlb_mpif.la
libdlb_mpif_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_la_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_la_SOURCES =
nodist_libdlb_mpif_la_SOURCES = $(MPIF_SRCS_GEN) $(MPIF_INTR_GEN)
libdlb_mpif_la_LIBADD = libdlb_mpi_common.la

if DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpif_dbg.la
libdlb_mpif_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_dbg_la_CFLAGS = $(DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_dbg_la_SOURCES =
nodist_libdlb_mpif_dbg_la_SOURCES = $(MPIF_SRCS_GEN) $(MPIF_INTR_GEN)
libdlb_mpif_dbg_la_LIBADD = libdlb_mpi_common_dbg.la
endif

if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_mpif_instr.la
libdlb_mpif_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_instr_la_CFLAGS = $(INSTR_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_instr_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_instr_la_SOURCES =
nodist_libdlb_mpif_instr_la_SOURCES = $(MPIF_SRCS_GEN)
libdlb_mpif_instr_la_LIBADD = libdlb_mpi_common_instr.la
endif

if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_mpif_instr_dbg.la
libdlb_mpif_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(AM_MPI_CPPFLAGS)
libdlb_mpif_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS) $(AM_CFLAGS)
libdlb_mpif_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(AM_MPI_LDFLAGS)
libdlb_mpif_instr_dbg_la_SOURCES =
nodist_libdlb_mpif_instr_dbg_la_SOURCES = $(MPIF_SRCS_GEN)
libdlb_mpif_instr_dbg_la_LIBADD = libdlb_mpi_common_instr_dbg.la
endif
endif #MPI_LIB

#********************************************************************************
# Support binaries
#********************************************************************************
bin_sources = \
	src/utils/dlb.c \
	src/utils/dlb_run.c \
	src/utils/dlb_shm.c \
	src/utils/dlb_taskset.c

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

if HAVE_OPENMP
noinst_PROGRAMS = dlb_tester
dlb_tester_SOURCES = src/utils/dlb_tester.c
dlb_tester_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
dlb_tester_CFLAGS = $(PERFO_CFLAGS) $(AM_CFLAGS) $(OPENMP_CFLAGS)
dlb_tester_LDADD = libdlb.la
endif

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
	doc/paraver_cfgs/DLB_loop.cfg \
	doc/paraver_cfgs/DLB_regions.cfg \
	doc/paraver_cfgs/DLB_Give_Want_number_of_cpus.cfg \
	doc/paraver_cfgs/DLB_callbacks.cfg \
	$(END)

paravercfgsdir = $(datadir)/paraver_cfgs/DLB
dist_paravercfgs_DATA = $(paraver_cfgs)

#********************************************************************************
# Examples
#********************************************************************************
### DROM ###
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

### MPI+OMP ###
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

### MPI+OMP (OMPT)  ###
mpiompomptexamples_files = \
	doc/examples/mpi+omp_ompt/README \
	doc/examples/mpi+omp_ompt/mpi_omp_pils.c \
	doc/examples/mpi+omp_ompt/extrae.xml \
	$(END)

mpiompomptexamples_files_nodist = \
	doc/examples/mpi+omp_ompt/Makefile \
	doc/examples/mpi+omp_ompt/run.sh \
	$(END)

mpiompomptexamplesdir = $(docdir)/examples/MPI+OMP_OMPT
dist_mpiompomptexamples_DATA = $(mpiompomptexamples_files)
nodist_mpiompomptexamples_DATA = $(mpiompomptexamples_files_nodist)

### MPI+OmpSs ###
mpiompssexamples_files = \
	doc/examples/mpi+ompss/README \
	doc/examples/mpi+ompss/Makefile \
	doc/examples/mpi+ompss/mpi_ompss_pils.c \
	doc/examples/mpi+ompss/run.sh \
	doc/examples/mpi+ompss/extrae.xml \
	$(END)

mpiompssexamplesdir = $(docdir)/examples/MPI+OmpSs
dist_mpiompssexamples_DATA = $(mpiompssexamples_files)

### OMPT ###
omptexamples_files = \
	doc/examples/ompt/app.c \
	doc/examples/ompt/run.sh \
	doc/examples/ompt/README \
	$(END)

omptexamples_files_nodist = \
	doc/examples/ompt/Makefile \
	$(END)

omptexamplesdir = $(docdir)/examples/OMPT
dist_omptexamples_DATA = $(omptexamples_files)
nodist_omptexamples_DATA = $(omptexamples_files_nodist)

### statistics ###
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

### talp ###
talpexamples_files = \
	doc/examples/talp/README \
	doc/examples/talp/talp_01.c \
	doc/examples/talp/get_times.c \
	$(END)

talpexamples_files_nodist = \
	doc/examples/talp/run.sh \
	doc/examples/talp/Makefile \
	$(END)

talpexamplesdir = $(docdir)/examples/TALP
dist_talpexamples_DATA = $(talpexamples_files)
nodist_talpexamples_DATA = $(talpexamples_files_nodist)

### Monitoring Regions ###
regionsexamples_files = \
	doc/examples/monitoring_regions/README \
	doc/examples/monitoring_regions/mpi_pils.c \
	$(END)

regionsexamples_files_nodist = \
	doc/examples/monitoring_regions/run.sh \
	doc/examples/monitoring_regions/Makefile \
	$(END)

regionsexamplesdir = $(docdir)/examples/monitoring_regions
dist_regionsexamples_DATA = $(regionsexamples_files)
nodist_regionsexamples_DATA = $(regionsexamples_files_nodist)

#********************************************************************************
# User scripts
#********************************************************************************
userscripts_files = \
	doc/scripts/lewi_ompss.sh \
	$(END)

userscripts_files_nodist = \
	doc/scripts/lewi_omp.sh \
	doc/scripts/lewi_omp_trace.sh \
	doc/scripts/lewi_ompss_trace.sh \
	doc/scripts/talp.sh \
	$(END)

userscriptsdir = $(docdir)/scripts
dist_userscripts_DATA = $(userscripts_files)
nodist_userscripts_DATA = $(userscripts_files_nodist)

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
	doc/user_guide/source/images/talp.png \
	doc/user_guide/source/_static/custom.css \
	doc/user_guide/Makefile

EXTRA_DIST += $(sphinx_files)

# Generated reST files from man pages
sphinx_manpages_files = \
	doc/user_guide/source/dlb.rst \
	doc/user_guide/source/dlb_run.rst \
	doc/user_guide/source/dlb_shm.rst \
	doc/user_guide/source/dlb_taskset.rst \
	doc/user_guide/source/dlb.h.rst \
	doc/user_guide/source/dlb_drom.h.rst \
	doc/user_guide/source/dlb_errors.h.rst \
	doc/user_guide/source/dlb_sp.h.rst \
	doc/user_guide/source/dlb_talp.h.rst \
	doc/user_guide/source/dlb_types.h.rst \
	$(END)

CLEANFILES += $(sphinx_manpages_files)

if HAVE_SPHINX
SPHINXBUILD = sphinx-build
SPHINX_SRC_DIR = doc/user_guide/source/
SPHINX_FLAGS = -D version=$(VERSION) -D release=$(VERSION)

if SPHINX_HAS_MANPAGES
SPHINX_FLAGS += -t man_pages
PANDOC = pandoc
PANDOC_FLAGS = --from=man --to=rst --shift-heading-level-by=3

doc/user_guide/source/dlb.rst: $(man1_builddir)/dlb.1
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_run.rst: $(man1_builddir)/dlb_run.1
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_run' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_shm.rst: $(man1_builddir)/dlb_shm.1
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_shm' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_taskset.rst: $(man1_builddir)/dlb_taskset.1
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_taskset' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb.h.rst: $(man3_builddir)/dlb.h.3
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb.h' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_drom.h.rst: $(man3_builddir)/dlb_drom.h.3
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_drom.h' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_errors.h.rst: $(man3_builddir)/dlb_errors.h.3
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_error.h' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_sp.h.rst: $(man3_builddir)/dlb_sp.h.3
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_sp.h' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_talp.h.rst: $(man3_builddir)/dlb_talp.h.3
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_talp.h' $(PANDOC_FLAGS) -o $@ $<

doc/user_guide/source/dlb_types.h.rst: $(man3_builddir)/dlb_types.h.3
	$(PANDOC_verbose)$(PANDOC) --title-prefix='dlb_types.h' $(PANDOC_FLAGS) -o $@ $<

else
$(sphinx_manpages_files) : % :
	@touch $@
endif

doc: html pdf

html-local: $(sphinx_manpages_files)
	$(SPHINXBUILD) -b html $(SPHINX_FLAGS) $(SPHINX_SRC_DIR) doc/user_guide/html
	@echo
	@echo "Build finished. The HTML pages are in doc/user_guide/html"

install-html-local: html-local
	$(mkdir_p) $(docdir)
	cp -R doc/user_guide/html $(docdir)

pdf-local: $(sphinx_manpages_files)
	$(SPHINXBUILD) -b latex $(SPHINX_FLAGS) $(SPHINX_SRC_DIR) doc/user_guide/pdf
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
DOXYFILE_MAN1 = $(builddir)/doc/doxygen/Doxyfile_man1
DOXYFILE_MAN3 = $(builddir)/doc/doxygen/Doxyfile_man3
man1_builddir = $(builddir)/doc/doxygen/man/man1
man3_builddir = $(builddir)/doc/doxygen/man/man3

dist_man_MANS = \
	$(man1_builddir)/dlb.1 \
	$(man1_builddir)/dlb_run.1 \
	$(man1_builddir)/dlb_shm.1 \
	$(man1_builddir)/dlb_taskset.1 \
	$(man3_builddir)/dlb.h.3 \
	$(man3_builddir)/dlb_drom.h.3 \
	$(man3_builddir)/dlb_talp.h.3 \
	$(man3_builddir)/dlb_sp.h.3 \
	$(man3_builddir)/dlb_stats.h.3 \
	$(man3_builddir)/dlb_types.h.3 \
	$(man3_builddir)/dlb_errors.h.3 \
	$(END)

$(man1_builddir)/doxy.stamp: $(bin_sources)
	$(DOXYMAN1_verbose)$(DOXYGEN) $(DOXYFILE_MAN1)
	@touch $(man1_builddir)/doxy.stamp

$(man3_builddir)/doxy.stamp: $(installheaders)
	$(DOXYMAN3_verbose)$(DOXYGEN) $(DOXYFILE_MAN3)
	@touch $(man3_builddir)/doxy.stamp

$(man1_builddir)/dlb.1: $(man1_builddir)/doxy.stamp
$(man1_builddir)/dlb_run.1: $(man1_builddir)/doxy.stamp
$(man1_builddir)/dlb_shm.1: $(man1_builddir)/doxy.stamp
$(man1_builddir)/dlb_taskset.1: $(man1_builddir)/doxy.stamp
$(man3_builddir)/dlb.h.3: $(man3_builddir)/doxy.stamp
$(man3_builddir)/dlb_drom.h.3: $(man3_builddir)/doxy.stamp
$(man3_builddir)/dlb_talp.h.3: $(man3_builddir)/doxy.stamp
$(man3_builddir)/dlb_sp.h.3: $(man3_builddir)/doxy.stamp
$(man3_builddir)/dlb_stats.h.3: $(man3_builddir)/doxy.stamp
$(man3_builddir)/dlb_types.h.3: $(man3_builddir)/doxy.stamp
$(man3_builddir)/dlb_errors.h.3: $(man3_builddir)/doxy.stamp

CLEANFILES += $(man1_builddir)/doxy.stamp
CLEANFILES += $(man3_builddir)/doxy.stamp
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
if ENABLE_VIEWER_SCRIPTS
python_PYTHON = scripts/viewer/dlb_wrapper.py \
		scripts/viewer/progressmeter.py

nodist_python_PYTHON = scripts/viewer/dlb_viewer.py

nodist_bin_SCRIPTS = scripts/viewer/dlb_cpu_usage
endif

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

if ENABLE_COVERAGE
COVERAGE_DIR = tests/coverage
coverage: check-recursive
	$(mkdir_p) $(COVERAGE_DIR)
	gcovr . --root $(top_srcdir) --html --html-details -o $(COVERAGE_DIR)/index.html

clean-coverage:
	find $(abs_top_builddir) -name '*.gcno' -or -name '*.gcda' -delete
	rm -rf $(COVERAGE_DIR)
else
coverage:
	@echo "coverage is not configured."
clean-coverage:
endif


#################################################################################
### Hooks                                                                     ###
#################################################################################

clean-local: clean-rpm clean-deb clean-doc clean-coverage

install-data-hook:
	chmod +x $(DESTDIR)$(mpiompexamplesdir)/run.sh
	chmod +x $(DESTDIR)$(mpiompomptexamplesdir)/run.sh
	chmod +x $(DESTDIR)$(mpiompssexamplesdir)/run.sh
	chmod +x $(DESTDIR)$(omptexamplesdir)/run.sh
	chmod +x $(DESTDIR)$(statsexamplesdir)/run.sh
	chmod +x $(DESTDIR)$(talpexamplesdir)/run.sh
	chmod +x $(DESTDIR)$(regionsexamplesdir)/run.sh
	chmod +x $(DESTDIR)$(userscriptsdir)/lewi_omp.sh
	chmod +x $(DESTDIR)$(userscriptsdir)/lewi_omp_trace.sh
	chmod +x $(DESTDIR)$(userscriptsdir)/lewi_ompss.sh
	chmod +x $(DESTDIR)$(userscriptsdir)/lewi_ompss_trace.sh
	chmod +x $(DESTDIR)$(userscriptsdir)/talp.sh

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


# additional include paths necessary to compile the C++ library
#AM_CFLAGS = -I$(top_srcdir)/src @AM_CFLAGS@

###############################################################################
# HELPER BINARY
###############################################################################

bin_PROGRAMS = dlb dlb_shm

dlb_SOURCES = utils/dlb.c

dlb_shm_SOURCES = utils/dlb_shm.c
dlb_shm_LDADD = libdlb_nanos.la

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

lib_LTLIBRARIES = libdlb_dummy.la libdlb_nanos.la
if DEBUG_LIB
lib_LTLIBRARIES += libdlb_nanos_dbg.la
endif
if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_nanos_instr.la
endif
if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_nanos_instr_dbg.la
endif

if MPI_LIB
lib_LTLIBRARIES += libdlb.la
if DEBUG_LIB
lib_LTLIBRARIES += libdlb_dbg.la
endif
if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_instr.la
endif
if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_instr_dbg.la
endif
endif #MPI_LIB


include_HEADERS = $(top_srcdir)/src/LB_core/DLB_interface.h \
                  $(top_srcdir)/src/LB_MPI/MPI_interface.h

END =
common_sources = \
	  DPD/DPD.c                   \
	  DPD/DPD.h                   \
	  LB_numThreads/numThreads.c  \
	  LB_numThreads/numThreads.h  \
	  LB_policies/JustProf.c      \
	  LB_policies/JustProf.h      \
	  LB_policies/Lend_light.c    \
	  LB_policies/Lend_light.h    \
	  LB_policies/Lewi_map.c      \
	  LB_policies/Lewi_map.h      \
	  LB_policies/Weight.c        \
	  LB_policies/Weight.h        \
	  LB_policies/lewi_mask.c     \
	  LB_policies/autonomous_lewi_mask.h     \
	  LB_policies/autonomous_lewi_mask.c     \
	  LB_policies/lewi_mask.h     \
	  LB_policies/RaL.c           \
	  LB_policies/RaL.h           \
	  LB_policies/PERaL.c         \
	  LB_policies/PERaL.h         \
	  LB_comm/comm_shMem.c        \
	  LB_comm/comm.h              \
	  LB_comm/comm_lend_light.c   \
	  LB_comm/comm_lend_light.h   \
	  LB_comm/comm_map.c          \
	  LB_comm/comm_map.h          \
	  LB_comm/shmem.c             \
	  LB_comm/shmem.h             \
	  LB_comm/shmem_lewi_mask.c   \
	  LB_comm/shmem_lewi_mask.h   \
	  LB_core/DLB_kernel.c        \
	  LB_core/DLB_kernel.h        \
	  LB_core/DLB_interface.c     \
	  LB_core/DLB_interface.h     \
	  support/globals.c           \
	  support/globals.h           \
	  support/debug.c             \
	  support/debug.h             \
	  support/tracing.c           \
	  support/tracing.h           \
	  support/utils.c             \
	  support/utils.h             \
	  support/mytime.c            \
	  support/mytime.h            \
	  support/mask_utils.c        \
	  support/mask_utils.h        \
	  $(END)

mpi_sources = \
	  LB_MPI/MPI_calls_coded.c    \
	  LB_MPI/MPI_calls_coded.h    \
	  LB_MPI/MPI_interface.c      \
	  LB_MPI/MPI_interface.h      \
	  LB_MPI/process_MPI.c        \
	  LB_MPI/process_MPI.h        \
	  $(END)

intercept = LB_MPI/MPI_intercept.c

dummy_iface = LB_core/DLB_dummy_interface.c

# UNUSED
#LB_policies/Lend_complex.c
#LB_policies/Lend_cpu.c
#LB_policies/justMPI.c
#LB_policies/DWB_Eco.c
#LB_policies/LeWI_active.c
#LB_policies/Lend.c
#LB_policies/Lend_simple.c
#LB_MPI/MPI_interfaceF

includes = \
         -I$(top_srcdir)/src                 \
         -I$(top_srcdir)/src/LB_numThreads   \
         -I$(top_srcdir)/src/LB_policies     \
         -I$(top_srcdir)/src/LB_MPI          \
         -I$(top_srcdir)/src/LB_comm         \
         -I$(top_srcdir)/src/LB_core         \
         -I$(top_srcdir)/src/support         \
         $(END)

AM_CPPFLAGS = $(includes)
AM_LDFLAGS = -lrt $(AC_LDFLAGS)
if HAVE_HWLOC
AM_CPPFLAGS += $(HWLOC_CPPFLAGS)
AM_LDFLAGS += $(HWLOC_LDFLAGS) $(HWLOC_LIBS)
endif


libdlb_dummy_la_CPPFLAGS = $(PERFO_CPPFLAGS)
libdlb_dummy_la_CFLAGS = $(PERFO_CFLAGS)
libdlb_dummy_la_LDFLAGS = -avoid-version
libdlb_dummy_la_SOURCES = $(dummy_iface)

libdlb_nanos_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_nanos_la_CFLAGS = $(PERFO_CFLAGS)
libdlb_nanos_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version
libdlb_nanos_la_SOURCES = $(common_sources)

if DEBUG_LIB
libdlb_nanos_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_nanos_dbg_la_CFLAGS = $(DEBUG_CFLAGS)
libdlb_nanos_dbg_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version
libdlb_nanos_dbg_la_SOURCES = $(common_sources)
endif

if INSTRUMENTATION_LIB
libdlb_nanos_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_nanos_instr_la_CFLAGS = $(INSTR_CFLAGS)
libdlb_nanos_instr_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version
libdlb_nanos_instr_la_SOURCES = $(common_sources)
endif

if INSTRUMENTATION_DEBUG_LIB
libdlb_nanos_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_nanos_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS)
libdlb_nanos_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version
libdlb_nanos_instr_dbg_la_SOURCES = $(common_sources)
endif

if MPI_LIB
MPI_CPPFLAGS += -DMPI_LIB

libdlb_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS) $(MPI_CPPFLAGS)
libdlb_la_CFLAGS = $(PERFO_CFLAGS)
libdlb_la_LDFLAGS = $(AM_LDFLAGS) $(MPI_LDFLAGS) $(MPI_LIBS) -avoid-version
libdlb_la_SOURCES = $(common_sources) $(mpi_sources) $(intercept)

if DEBUG_LIB
libdlb_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(MPI_CPPFLAGS)
libdlb_dbg_la_CFLAGS = $(DEBUG_CFLAGS)
libdlb_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(MPI_LDFLAGS) $(MPI_LIBS) -avoid-version
libdlb_dbg_la_SOURCES = $(common_sources) $(mpi_sources) $(intercept)
endif

if INSTRUMENTATION_LIB
libdlb_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS) $(MPI_CPPFLAGS)
libdlb_instr_la_CFLAGS = $(INSTR_CFLAGS)
libdlb_instr_la_LDFLAGS = $(AM_LDFLAGS) $(MPI_LDFLAGS) $(MPI_LIBS) -avoid-version
libdlb_instr_la_SOURCES = $(common_sources) $(mpi_sources)
endif

if INSTRUMENTATION_DEBUG_LIB
libdlb_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS) $(MPI_CPPFLAGS)
libdlb_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS)
libdlb_instr_dbg_la_LDFLAGS = $(AM_LDFLAGS) $(MPI_LDFLAGS) $(MPI_LIBS) -avoid-version
libdlb_instr_dbg_la_SOURCES = $(common_sources) $(mpi_sources)
endif
endif #MPI_LIB

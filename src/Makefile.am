
# additional include paths necessary to compile the C++ library
#AM_CFLAGS = -I$(top_srcdir)/src @AM_CFLAGS@

###############################################################################
# HELPER BINARY
###############################################################################

bin_PROGRAMS = dlb

dlb_LDFLAGS =
dlb_SOURCES = $(top_srcdir)/src/utils/dlb.c

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

lib_LTLIBRARIES = libdlb.la libdlb_dummy.la
if DEBUG_LIB
lib_LTLIBRARIES += libdlb_dbg.la
endif
if INSTRUMENTATION_LIB
lib_LTLIBRARIES += libdlb_instr.la
endif
if INSTRUMENTATION_DEBUG_LIB
lib_LTLIBRARIES += libdlb_instr_dbg.la
endif


include_HEADERS = $(top_srcdir)/src/LB_core/DLB_interface.h \
                  $(top_srcdir)/src/LB_MPI/MPI_interface.h

END =
sources = \
	  $(top_srcdir)/src/DPD/DPD.c           	\
	  $(top_srcdir)/src/DPD/DPD.h           	\
	  $(top_srcdir)/src/LB_numThreads/numThreads.c  \
	  $(top_srcdir)/src/LB_numThreads/numThreads.h  \
	  $(top_srcdir)/src/LB_policies/JustProf.c      \
	  $(top_srcdir)/src/LB_policies/JustProf.h      \
	  $(top_srcdir)/src/LB_policies/Lend_light.c    \
	  $(top_srcdir)/src/LB_policies/Lend_light.h    \
	  $(top_srcdir)/src/LB_policies/Lewi_map.c      \
	  $(top_srcdir)/src/LB_policies/Lewi_map.h      \
	  $(top_srcdir)/src/LB_policies/Weight.c        \
	  $(top_srcdir)/src/LB_policies/Weight.h        \
	  $(top_srcdir)/src/LB_policies/lewi_mask.c     \
	  $(top_srcdir)/src/LB_policies/lewi_mask.h     \
	  $(top_srcdir)/src/LB_policies/RaL.c		\
	  $(top_srcdir)/src/LB_policies/RaL.h		\
	  $(top_srcdir)/src/LB_policies/PERaL.c		\
	  $(top_srcdir)/src/LB_policies/PERaL.h		\
	  $(top_srcdir)/src/LB_comm/comm_shMem.c 	\
	  $(top_srcdir)/src/LB_comm/comm.h 		\
	  $(top_srcdir)/src/LB_comm/comm_lend_light.c 	\
	  $(top_srcdir)/src/LB_comm/comm_lend_light.h 	\
	  $(top_srcdir)/src/LB_comm/comm_map.c      	\
	  $(top_srcdir)/src/LB_comm/comm_map.h      	\
	  $(top_srcdir)/src/LB_comm/shmem.c           	\
	  $(top_srcdir)/src/LB_comm/shmem.h           	\
	  $(top_srcdir)/src/LB_comm/shmem_lewi_mask.c 	\
	  $(top_srcdir)/src/LB_comm/shmem_lewi_mask.h 	\
	  $(top_srcdir)/src/LB_MPI/MPI_calls_coded.c    \
	  $(top_srcdir)/src/LB_MPI/MPI_calls_coded.h    \
	  $(top_srcdir)/src/LB_MPI/MPI_interface.c     	\
	  $(top_srcdir)/src/LB_MPI/MPI_interface.h     	\
	  $(top_srcdir)/src/LB_MPI/process_MPI.c       	\
	  $(top_srcdir)/src/LB_MPI/process_MPI.h       	\
	  $(top_srcdir)/src/LB_core/DLB_kernel.c       	\
	  $(top_srcdir)/src/LB_core/DLB_kernel.h       	\
	  $(top_srcdir)/src/LB_core/DLB_interface.c 	\
	  $(top_srcdir)/src/LB_core/DLB_interface.h 	\
	  $(top_srcdir)/src/support/globals.c       	\
	  $(top_srcdir)/src/support/globals.h       	\
	  $(top_srcdir)/src/support/debug.c         	\
	  $(top_srcdir)/src/support/debug.h         	\
	  $(top_srcdir)/src/support/tracing.c       	\
	  $(top_srcdir)/src/support/tracing.h       	\
	  $(top_srcdir)/src/support/utils.c         	\
	  $(top_srcdir)/src/support/utils.h         	\
	  $(top_srcdir)/src/support/mytime.c        	\
	  $(top_srcdir)/src/support/mytime.h        	\
	  $(top_srcdir)/src/support/mask_utils.c      	\
	  $(top_srcdir)/src/support/mask_utils.h      	\
	  $(END)

intercept = $(top_srcdir)/src/LB_MPI/MPI_intercept.c

dummy_iface = $(top_srcdir)/src/LB_core/DLB_dummy_interface.c

# UNUSED
#$(top_srcdir)/src/LB_policies/Lend_complex.c
#$(top_srcdir)/src/LB_policies/Lend_cpu.c
#$(top_srcdir)/src/LB_policies/justMPI.c
#$(top_srcdir)/src/LB_policies/DWB_Eco.c    	
#$(top_srcdir)/src/LB_policies/LeWI_active.c    
#$(top_srcdir)/src/LB_policies/Lend.c         	
#$(top_srcdir)/src/LB_policies/Lend_simple.c    
#$(top_srcdir)/src/LB_MPI/MPI_interfaceF

includes = \
         -I$(top_srcdir)/src                       	\
         -I$(top_srcdir)/src/LB_numThreads              \
         -I$(top_srcdir)/src/LB_policies              	\
         -I$(top_srcdir)/src/LB_MPI                   	\
         -I$(top_srcdir)/src/LB_comm                 	\
         -I$(top_srcdir)/src/LB_core               	\
         -I$(top_srcdir)/src/support               	\
         $(END)

AM_CPPFLAGS = $(includes)
AM_LDFLAGS = -lrt
if MPI_SUPPORT
AM_CPPFLAGS += -DUSE_MPI -I$(MPI_HOME)/include
AM_LDFLAGS += -L$(MPI_HOME)/lib $(MPI_LIBS)
endif
if HWLOC_SUPPORT
AM_CPPFLAGS += -DUSE_HWLOC -I$(HWLOC_HOME)/include
AM_LDFLAGS += -L$(HWLOC_HOME)/lib -lhwloc
endif

libdlb_la_CPPFLAGS = $(PERFO_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_la_CFLAGS = $(PERFO_CFLAGS)
libdlb_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version
libdlb_la_SOURCES = $(sources) $(intercept)

libdlb_dummy_la_CPPFLAGS = $(PERFO_CPPFLAGS)
libdlb_dummy_la_CFLAGS = $(PERFO_CFLAGS)
libdlb_dummy_la_LDFLAGS = -avoid-version
libdlb_dummy_la_SOURCES = $(dummy_iface)

if DEBUG_LIB
libdlb_dbg_la_CPPFLAGS = $(DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_dbg_la_CFLAGS = $(DEBUG_CFLAGS)
libdlb_dbg_la_LDFLAGS = $(libdlb_la_LDFLAGS)
libdlb_dbg_la_SOURCES = $(sources) $(intercept)
endif

if INSTRUMENTATION_LIB
libdlb_instr_la_CPPFLAGS = $(INSTR_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_instr_la_CFLAGS = $(INSTR_CFLAGS)
libdlb_instr_la_LDFLAGS = $(libdlb_la_LDFLAGS)
libdlb_instr_la_SOURCES = $(sources)
endif

if INSTRUMENTATION_DEBUG_LIB
libdlb_instr_dbg_la_CPPFLAGS = $(INSTR_DEBUG_CPPFLAGS) $(AM_CPPFLAGS)
libdlb_instr_dbg_la_CFLAGS = $(INSTR_DEBUG_CFLAGS)
libdlb_instr_dbg_la_LDFLAGS = $(libdlb_la_LDFLAGS)
libdlb_instr_dbg_la_SOURCES = $(sources)
endif

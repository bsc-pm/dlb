
# additional include paths necessary to compile the C++ library
#AM_CFLAGS = -I$(top_srcdir)/src @AM_CFLAGS@

###############################################################################
# THE LIBRARIES TO BUILD
###############################################################################

lib_LTLIBRARIES = libdlb.la libTdlb.la libUpdRes.la
if DEBUG_LIB
lib_LTLIBRARIES += libdlb_dbg.la
endif

include_HEADERS = $(top_srcdir)/src/dlb/DLB_interface.h \
                  $(top_srcdir)/src/LB_MPI/MPI_interface.h

END =
sources = \
         $(top_srcdir)/src/DPD/DPD.c           		\
         $(top_srcdir)/src/LB_numThreads/numThreads.c   \
         $(top_srcdir)/src/LB_policies/DWB_Eco.c    	\
         $(top_srcdir)/src/LB_policies/JustProf.c       \
         $(top_srcdir)/src/LB_policies/LeWI_active.c    \
         $(top_srcdir)/src/LB_policies/Lend.c         	\
         $(top_srcdir)/src/LB_policies/Lend_light.c     \
         $(top_srcdir)/src/LB_policies/Lend_simple.c    \
         $(top_srcdir)/src/LB_policies/Lewi_map.c       \
         $(top_srcdir)/src/LB_policies/Weight.c         \
         $(top_srcdir)/src/LB_policies/lewi_mask.c      \
         $(top_srcdir)/src/LB_comm/comm.c           	\
         $(top_srcdir)/src/LB_comm/comm_lend_light.c 	\
         $(top_srcdir)/src/LB_comm/comm_map.c      	\
         $(top_srcdir)/src/LB_comm/shmem.c           	\
         $(top_srcdir)/src/LB_comm/shmem_lewi_mask.c 	\
         $(top_srcdir)/src/LB_MPI/MPI_interface.c     	\
	 $(top_srcdir)/src/LB_MPI/MPI_interfaceF.c    	\
         $(top_srcdir)/src/LB_MPI/process_MPI.c       	\
         $(top_srcdir)/src/dlb/dlb.c       		\
         $(top_srcdir)/src/dlb/api.c       		\
         $(top_srcdir)/src/support/globals.c       	\
         $(top_srcdir)/src/support/debug.c         	\
         $(top_srcdir)/src/support/tracing.c       	\
         $(top_srcdir)/src/support/utils.c         	\
         $(top_srcdir)/src/support/mytime.c        	\
         $(END)

intercept = $(top_srcdir)/src/LB_MPI/MPI_intercept.c

# UNUSED
#$(top_srcdir)/src/LB_policies/Lend_complex.c
#$(top_srcdir)/src/LB_policies/Lend_cpu.c
#$(top_srcdir)/src/LB_policies/justMPI.c
#$(top_srcdir)/src/LB_comm/comm_sockets.c
#$(top_srcdir)/src/LB_comm/comm_shMem_states.c
#$(top_srcdir)/src/LB_comm/comm_shMem.c

includes = \
         -I$(top_srcdir)/src                       	\
         -I$(top_srcdir)/src/LB_numThreads              \
         -I$(top_srcdir)/src/LB_policies              	\
         -I$(top_srcdir)/src/LB_MPI                   	\
         -I$(top_srcdir)/src/LB_comm                 	\
         -I$(top_srcdir)/src/dlb               		\
         -I$(top_srcdir)/src/support               	\
         $(END)

if MPI_SUPPORT
includes += -I$(MPI_HOME)/include
AM_LDFLAGS = -L$(MPI_HOME)/lib -lmpi
endif

libdlb_la_CPPFLAGS = $(PERFO_MACRO_FLAGS)
libTdlb_la_CPPFLAGS = $(PERFO_MACRO_FLAGS)
libUpdRes_la_CPPFLAGS = $(PERFO_MACRO_FLAGS)

libdlb_la_CFLAGS = $(PERFO_FLAGS) $(includes)
libTdlb_la_CFLAGS = $(PERFO_FLAGS) $(includes)
libUpdRes_la_CFLAGS = $(PERFO_FLAGS)

libdlb_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version
libTdlb_la_LDFLAGS = $(AM_LDFLAGS) -avoid-version
libUpdRes_la_LDFLAGS = -avoid-version

libdlb_la_SOURCES = $(sources)
libTdlb_la_SOURCES = $(sources) $(intercept)
libUpdRes_la_SOURCES = dlb/api_stub.c

if DEBUG_LIB
libdlb_dbg_la_CPPFLAGS = $(DEBUG_MACRO_FLAGS)
libdlb_dbg_la_CFLAGS = $(DEBUG_FLAGS) $(includes)
libdlb_dbg_la_LDFLAGS = $(libdlb_la_LDFLAGS)
libdlb_dbg_la_SOURCES = $(libTdlb_la_SOURCES)
endif

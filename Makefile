################ DEBUG OPTIONS ######################

## -DdebugBasicInfo :Prints the configuration values used (recommended to set always)- printed in stdout

## printed in stderr:
## -DdebugInOutMPI :Prints when getting in and out the intercepted MPI calls
## -DdebugConfig :Prints the initialization process and config values
## -DdebugComm :Prints communication between threads
## -DdebugDistribution :Prints the changes in the distribution of cpus
## -DdebugSharedMem :Prints the operations on shared memory
## -DdebugLend :Prints debug information of Lend policy
## -DOMPI_events: adds debug events to the trace, but should run with tracing OMPItrace library
## -DdebugLoads: Prints the computational time, MPI time and weight of each MPI

DEBUG= -DdebugBasicInfo
export DEBUG 


#################### FILES ##########################

OBJ= LB_MPI/MPI_interface.o LB_MPI/process_MPI.o dlb/dlb_API.o DPD/DPD.o LB_arch/MyTime.o LB_policies/JustProf.o LB_policies/Lend.o LB_policies/Weight.o LB_policies/Lend_simple.o LB_policies/LeWI_active.o LB_numThreads/numThreads.o LB_policies/DWB_Eco.o LB_arch/common.o LB_policies/Lend_light.o LB_comm/comm_lend_light.o 

SOCKT_OBJ= LB_comm/comm_sockets.o

SHMEM_OBJ=LB_comm/comm_shMem.o

COMM_OBJ=$(SHMEM_OBJ)

INTERCEPT_OBJ= LB_MPI/MPI_intercept.o

DEPENDS= Makefile LB_MPI/MPI_calls_coded.h dlb/dlb_API.h LB_numThreads/numThreads.h LB_comm/comm.h

SUB_DIRS= LB_MPI dlb LB_arch LB_policies DPD LB_comm LB_numThreads


#################### FLAGS ##########################

CC= xlc_r
export CC

MPICC= mpicc
export MPICC

CFLAGS= -qPIC -q64 -I. -I.. -I/home/bsc41/bsc41273/foreign-pkgs/mpitrace-dlb/32/include/ $(DEBUG) -qinfo=gen -qformat=all
export CFLAGS

LDFLAGS= -qPIC -q64 -qmkshrobj 
export LDFLAGS


#################### RULES ##########################

all: lib/libdlb.so lib/libTdlb.so lib/libUpdRes.so

dlb: lib/libdlb.so 

trace_dlb: lib/libTdlb.so

updRes: lib/libUpdRes.so


lib/libdlb.so: $(DEPENDS) $(OBJ) $(INTERCEPT_OBJ) $(COMM_OBJ)
	$(CC) $(LDFLAGS) -o lib/libdlb.so $(OBJ) $(INTERCEPT_OBJ) $(COMM_OBJ)
	

lib/libTdlb.so: $(DEPENDS) $(OBJ) $(COMM_OBJ)
	$(CC) $(LDFLAGS) -o lib/libTdlb.so $(OBJ) $(COMM_OBJ)


%.o : %.c
	$(MAKE) -C `dirname $@` `basename $@`

######### Library UpdRes just includes de call to updateResources to compile applications #####
lib/libUpdRes.so: dlb/updateResources.o
	$(CC) $(LDFLAGS) -o lib/libUpdRes.so dlb/updateResources.o

##	$(CC) $(CFLAGS) -c  dlb/updateResources.c -o dlb/updateResources.o

clean:
	for i in $(SUB_DIRS);\
	do\
		$(MAKE) -C $${i} clean;\
	done
	rm -f lib/libdlb.so
	rm -f lib/libTdlb.so

#for i in $(SUB_DIRS);\
#	do\
#		$(MAKE) -C $${i} dlb;\
#	done

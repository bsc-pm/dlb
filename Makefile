include Makefile.obj

################ DEBUG OPTIONS ######################

## -DdebugBasicInfo :Prints the configuration values used (recommended to set always)- printed in stdout

## printed in stderr:
## -DdebugInOutMPI :Prints when getting in and out the intercepted MPI calls
## -DdebugConfig :Prints the initialization process and config values
## -DdebugComm :Prints communication between threads
## -DdebugDistribution :Prints the changes in the distribution of cpus
## -DdebugSharedMem :Prints the operations on shared memory
## -DdebugLend :Prints debug information of Lend policy
## -DdebugMap :Prints debug information of Map policy
## -DdebugLoads: Prints the computational time, MPI time and weight of each MPI
## -DdebugBinding: Prints the binding of threads to cpus if binding is set

DEBUG= -DdebugBasicInfo 
export DEBUG 

#################### FLAGS ##########################

CC= xlc_r 
CC= mpicc 
CC= gcc
export CC

CFLAGS_xl= -qPIC -I. -I.. $(DEBUG) -qinfo=gen -qformat=all
CFLAGS= -fPIC -I. -I.. $(DEBUG) -Wall -O2 
export CFLAGS
export CFLAGS_xl

LDFLAGS_xl= -qPIC -qmkshrobj 
LDFLAGS= -fPIC -shared -Wall -O2 -lrt
export LDFLAGS
export LDFLAGS_xl

FLAGS32_xl= -q32
FLAGS32= -m32
export FLAGS32
export FLAGS32_xl


FLAGS64_xl= -q64
FLAGS64= -m64
export FLAGS64
export FLAGS64_xl

MPIINCLUDE=-I/opt/osshpc/mpich-mx/64/include
MPI2INCLUDE=-I/gpfs/apps/MPICH2/mx/default/64/include/
export MPIINCLUDE
export MPI2INCLUDE

#################### RULES ##########################

all: lib32 lib64 libmpi2-64 


dlb: lib/32/libdlb.so lib/64/libdlb.so


trace_dlb: lib/32/libTdlb.so lib/64/libTdlb.so

updRes: lib/32/libUpdRes.so lib/64/libUpdRes.so

lib32: lib/32/libdlb.so lib/32/libTdlb.so lib/32/libUpdRes.so

lib64: lib/64/libdlb.so lib/64/libTdlb.so lib/64/libUpdRes.so

libmpi2-64: lib/mpi2-64/libdlb.so lib/mpi2-64/libTdlb.so lib/64/libUpdRes.so 

libmpi2-32: lib/mpi2-32/libdlb.so lib/mpi2-32/libTdlb.so lib/32/libUpdRes.so 


#------- 64bits library ----------#
lib/64/libdlb.so: $(DEPENDS) $(OBJ) $(OBJ_MPI) $(INTERCEPT_OBJ)
	$(CC) $(LDFLAGS) $(FLAGS64) -o lib/64/libdlb.so $(OBJ) $(OBJ_MPI) $(INTERCEPT_OBJ) 
	

lib/64/libTdlb.so: $(DEPENDS) $(OBJ) $(OBJ_MPI)
	$(CC) $(LDFLAGS) $(FLAGS64) -o lib/64/libTdlb.so $(OBJ) $(OBJ_MPI) 

#------- 32bits library ----------#
lib/32/libdlb.so: $(DEPENDS) $(OBJ32) $(OBJ32_MPI) $(INTERCEPT_OBJ32)
	$(CC) $(LDFLAGS) $(FLAGS32) -o lib/32/libdlb.so $(OBJ32) $(OBJ32_MPI) $(INTERCEPT_OBJ32)
	

lib/32/libTdlb.so: $(DEPENDS) $(OBJ) $(OBJ32_MPI)
	$(CC) $(LDFLAGS) $(FLAGS32) -o lib/32/libTdlb.so $(OBJ32) $(OBJ32_MPI)

#------- 64bits library for MPICH2 ----------#
lib/mpi2-64/libdlb.so: $(DEPENDS) $(OBJ) $(OBJ_MPI2) $(INTERCEPT_OBJ_MPI2)
	$(CC) $(LDFLAGS) $(FLAGS64) -o lib/mpi2-64/libdlb.so $(OBJ) $(OBJ_MPI2) $(INTERCEPT_OBJ_MPI2)
	
lib/mpi2-64/libTdlb.so: $(DEPENDS) $(OBJ) $(OBJ_MPI2)
	$(CC) $(LDFLAGS) $(FLAGS64) -o lib/mpi2-64/libTdlb.so $(OBJ) $(OBJ_MPI2)

#------- 32bits library for MPICH2 ----------#
lib/mpi2-32/libdlb.so: $(DEPENDS) $(OBJ32) $(OBJ32_MPI2) $(INTERCEPT_OBJ32_MPI2)
	$(CC) $(LDFLAGS) $(FLAGS32) -o lib/mpi2-32/libdlb.so $(OBJ32) $(OBJ32_MPI2) $(INTERCEPT_OBJ32_MPI2)
	
lib/mpi2-32/libTdlb.so: $(DEPENDS) $(OBJ32) $(OBJ32_MPI2)
	$(CC) $(LDFLAGS) $(FLAGS32) -o lib/mpi2-32/libTdlb.so $(OBJ32) $(OBJ32_MPI2)



%32-mpi2.o: %.c
	$(MAKE) -C `dirname $@` `basename $@`

%-mpi2.o : %.c
	$(MAKE) -C `dirname $@` `basename $@`

%32.o: %.c
	$(MAKE) -C `dirname $@` `basename $@`

%.o : %.c
	$(MAKE) -C `dirname $@` `basename $@`



######### Library UpdRes just includes de call to updateResources to compile applications #####

lib/64/libUpdRes.so: dlb/updateResources.o
	$(CC) $(LDFLAGS) $(FLAGS64) -o lib/64/libUpdRes.so dlb/updateResources.o

lib/32/libUpdRes.so: dlb/updateResources32.o
	$(CC) $(LDFLAGS) $(FLAGS32) -o lib/32/libUpdRes.so dlb/updateResources32.o

################### CLEAN ###############################################

clean:
	for i in $(SUB_DIRS);\
	do\
		$(MAKE) -C $${i} clean;\
	done
	rm -f lib/32/libdlb.so
	rm -f lib/32/libTdlb.so
	rm -f lib/64/libdlb.so
	rm -f lib/64/libTdlb.so

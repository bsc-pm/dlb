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
## -DdebugLoads: Prints the computational time, MPI time and weight of each MPI

DEBUG= -DdebugBasicInfo  
export DEBUG 

#################### FLAGS ##########################

CC= xlc_r 
CC= gcc 
export CC

MPICC= mpicc -cc=gcc
export MPICC

CFLAGS_xl= -qPIC -I. -I.. $(DEBUG) -qinfo=gen -qformat=all
CFLAGS= -fPIC -I. -I.. $(DEBUG) -Wall -O2
export CFLAGS
export CFLAGS_xl

LDFLAGS_xl= -qPIC -qmkshrobj 
LDFLAGS= -fPIC -shared -Wall -O2
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


#################### RULES ##########################

all: lib32 lib64


dlb: lib/32/libdlb.so lib/64/libdlb.so


trace_dlb: lib/32/libTdlb.so lib/64/libTdlb.so

updRes: lib/32/libUpdRes.so lib/64/libUpdRes.so

lib32: lib/32/libdlb.so lib/32/libTdlb.so lib/32/libUpdRes.so
  echo "Building: " $@

lib64: lib/64/libdlb.so lib/64/libTdlb.so lib/64/libUpdRes.so


#------- 64bits library ----------#
lib/64/libdlb.so: $(DEPENDS) $(OBJ) $(INTERCEPT_OBJ)
	$(CC) $(LDFLAGS) $(FLAGS64) -o lib/64/libdlb.so $(OBJ) $(INTERCEPT_OBJ)
	

lib/64/libTdlb.so: $(DEPENDS) $(OBJ)
	$(CC) $(LDFLAGS) $(FLAGS64) -o lib/64/libTdlb.so $(OBJ)

#------- 32bits library ----------#
lib/32/libdlb.so: $(DEPENDS) $(OBJ32) $(INTERCEPT_OBJ32)
	$(CC) $(LDFLAGS) $(FLAGS32) -o lib/32/libdlb.so $(OBJ32) $(INTERCEPT_OBJ32)
	

lib/32/libTdlb.so: $(DEPENDS) $(OBJ)
	$(CC) $(LDFLAGS) $(FLAGS32) -o lib/32/libTdlb.so $(OBJ32)


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

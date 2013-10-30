#!/bin/bash

if [ $# != 8 ]
then
	echo "ERROR: wrong number of parameters"
	echo "Usage: set_dlb.sh <procs per node> <version> <policy> <tracing> <block mode> <conf_extrae> <thread distribution> <params>"
	echo $*
	echo Found $# parameters instead of 8 
	echo
	echo "procs per node: Num mpi procs inside a node"
	echo "version: SERIE | SMPSS | OMP | OMPSS"
	echo "policy: ORIG | LeWI | LeWI_mask | RaL | PeRaL"
	echo "tracing: YES | NO"
	echo "block mode: BLOCK | 1CPU"
	echo "conf extrae: path to extrae configuration file"
	echo "thread distribution: - (default thread distribution) | x-y-z-w (MPI0 x threads, MPI1 y threads...)"
	echo "params: execution line with all its parameters"
	exit
else
	procs_node=$1
	version=$2
	policy=$3
	tracing=$4 
	block_mode=$5
	conf_extrae=$6	
	thread_distrib=$7
	params=${8}	
fi

if [ $version == "SERIE" -o $version == "ORIG" ]
then
	MPI_TRACE_LIB=libmpitrace.so
	MPI_TRACE_LIB_DLB=libmpitrace.so

elif [ $version == "SMPSS" ]
then
	MPI_TRACE_LIB=libsmpssmpitrace.so
	MPI_TRACE_LIB_DLB=libsmpssmpitrace-lb.so

elif [ $version == "OMP" ]
then
	MPI_TRACE_LIB=libompitrace.so
	MPI_TRACE_LIB_DLB=libompitrace-lb.so

elif [ $version == "OMPSS" -o $version == "OMP-NANOS" ]
then
	MPI_TRACE_LIB=libnanosmpitrace.so
	MPI_TRACE_LIB_DLB=libnanosmpitrace-lb.so

else
	echo "Unknown version $version"
	exit
fi

## Deciding blocking mode
if [ ${block_mode} == "BLOCK" ]
then
	#export MXMPI_RECV=blocking
	export OMPI_MCA_mpi_yield_when_idle=1 
elif [ ${block_mode} == "1CPU" ]
then
	export LB_LEND_MODE=1CPU
else
	echo "Unknown blocking mode ${block_mode}"
	exit
fi

CPUS_NODE=16
CPUS_PROC=$(($CPUS_NODE/$procs_node))

if [ $policy != "LeWI_mask" -a $policy != "RaL" ]
then
	NANOS_ARGS=" --disable-binding " 
###--instrument-cpuid"
fi

if [ $policy != "ORIG" ]
then
	NANOS_ARGS="$NANOS_ARGS --enable-dlb"
fi

export NX_ARGS+=$NANOS_ARGS

##############  PATHS  #############
DLB_PATH=/home/bsc15/bsc15994/MN3/dlb/install/lib
SMPSS_PATH=/home/bsc15/bsc15994/SMPSs-install${bits}/lib
TRACE_PATH=/home/bsc15/bsc15994/MN3/extrae/lib


##############  OpenMP VARS ################
export OMP_SCHEDULE=STATIC
#export OMP_SCHEDULE=DYNAMIC
##export OMP_SCHEDULE=GUIDED
export OMP_NUM_THREADS=$CPUS_PROC


############## DLB ENV VARS ################
export LB_MPIxNODE=$procs_node
export LB_POLICY=$policy
export LB_LEND_MODE=${block_mode}
#export LB_BIND=YES
#export LB_JUST_BARRIER=1

if [ $thread_distrib != "-" ]
then
	export LB_THREAD_DISTRIBUTION=$thread_distrib
fi

############## CSS ENV VARS ################
export CSS_NUM_CPUS=$(($CPUS_NODE/$procs_node))                                                                                                                                                                                              
export CSS_MAX_CPUS=$CPUS_NODE
export CSS_CONFIG_FILE=${conf_smpss}


############## EXTRAE ENV VARS ################
if [ $tracing == "YES" ]
then
	export MPITRACE_ON=1
	export EXTRAE_CONFIG_FILE=${conf_extrae}
	export NX_ARGS+=" --instrumentation=extrae --extrae-skip-merge" 
fi


export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${DLB_PATH}:${SMPSS_PATH}:$TRACE_PATH:

if [ $policy == "ORIG" ]
then
################### RUN ORIGINAL APPLICATION ###################

	if [ $tracing == "YES" ]
        then
                export LD_PRELOAD=${TRACE_PATH}/${MPI_TRACE_LIB}
        fi
	$params

################### RUN APPLICATION WITH DLB ###################
else

	if [ $tracing == "YES" ]
	then
		export LD_PRELOAD=${TRACE_PATH}/${MPI_TRACE_LIB_DLB}:${DLB_PATH}/libdlb_instr.so
#		export LD_PRELOAD=${TRACE_PATH}/${MPI_TRACE_LIB_DLB}:${DLB_PATH}/libdlb_dbg.so
	else
		export LD_PRELOAD=${DLB_PATH}/libdlb.so
	fi
	$params
fi 

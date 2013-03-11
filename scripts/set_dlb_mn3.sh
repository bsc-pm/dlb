#!/bin/bash

if [ $# != 10 ]
then
	echo "ERROR: wrong number of parameters"
	echo "Usage: set_dlb.sh <procs per node> <profiling> <version> <policy> <tracing> <block mode> <bits> <conf_smpss> <conf_extrae> <params>"
	echo $*
	echo Found $# parameters instead of 10
	echo ${11}
	echo ${12}
	exit
else
	procs_node=$1
	profiling=$2
	version=$3
	policy=$4
	tracing=$5 
	block_mode=$6
	bits=$7
	conf_smpss=$8	
	conf_extrae=$9	
	params=${10}	
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

else
	echo "Unknown version $version"
	exit
fi

## Deciding blocking mode
if [ ${block_mode} == "BLOCK" ]
then
	export MXMPI_RECV=blocking
elif [ ${block_mode} == "1CPU" ]
then
	export LB_LEND_MODE=1CPU
else
	echo "Unknown blocking mode ${block_mode}"
	exit
fi

##############  PATHS  #############
DLB_PATH=/home/bsc15/bsc15994/MN3/dlb/install/lib
SMPSS_PATH=/home/bsc15/bsc15994/SMPSs-install${bits}/lib
TRACE_PATH=/home/bsc15/bsc15994/extrae-2.3.2/install_MN3/lib

CPUS_NODE=16

##############  OpenMP VARS ################
export OMP_SCHEDULE=STATIC
##export OMP_SCHEDULE=DYNAMIC
##export OMP_SCHEDULE=GUIDED


############## DLB ENV VARS ################
export LB_MPIxNODE=$procs_node
export LB_POLICY=$policy
export LB_PROFILE=$profiling
export LB_LEND_MODE=${block_mode}
#export LB_BIND=YES
#export LB_JUST_BARRIER=1


############## CSS ENV VARS ################
export CSS_NUM_CPUS=$(($CPUS_NODE/$procs_node))                                                                                                                                                                                              
export CSS_MAX_CPUS=$CPUS_NODE
export CSS_CONFIG_FILE=${conf_smpss}


############## EXTRAE ENV VARS ################
if [ $tracing == "YES" ]
then
	export MPITRACE_ON=1
	export EXTRAE_CONFIG_FILE=${conf_extrae}
fi


export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${DLB_PATH}:${SMPSS_PATH}:$TRACE_PATH:

if [ $policy == "ORIG" ]
then
################### RUN ORIGINAL APPLICATION ###################

        export OMP_NUM_THREADS=$(($CPUS_NODE/$procs_node))

	if [ $tracing == "YES" ]
        then
                export LD_PRELOAD=${TRACE_PATH}/${MPI_TRACE_LIB}
        fi
echo $OMP_NUM_THREADS
	$params

################### RUN APPLICATION WITH DLB ###################
else
	if [ $policy == "NO" ]
	then
       		export OMP_NUM_THREADS=$(($CPUS_NODE/$procs_node))
	else
       		export OMP_NUM_THREADS=$CPUS_NODE
	fi

	if [ $tracing == "YES" ]
	then
		export LD_PRELOAD=${TRACE_PATH}/${MPI_TRACE_LIB_DLB}:${DLB_PATH}/libTdlb.so
	else
		export LD_PRELOAD=${DLB_PATH}/libdlb.so
	fi

echo $OMP_NUM_THREADS
	$params
fi 

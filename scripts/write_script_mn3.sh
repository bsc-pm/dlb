#!/bin/bash

##### VARS ###############################
DLB_PATH=/home/bsc15/bsc15994/MN3/dlb/install
##########################################

profiling=NO

if [ $# != 12 ]
then
	echo "ERROR: Wrong number of parameters"
	echo "Usage: write_script.sh <script_name> <ini_dir> <mpi_procs> <procs per node> <params> <tracing> <version> <policy> <trace_path> <BITS> <distribution> <duration>"
	echo $* 
	exit
else
	script_name=$1	
	ini_dir=$2
	mpi_procs=$3
	procs_node=$4
	params=$5
	tracing=$6
	version=$7
	policy=$8
	trace_path=$9
	bits=${10}
	distribution=${11}
	duration=${12}
fi

if [ $distribution == "CYCLIC" ]
then
	dist="--bynode"
elif [ $distribution == "BASE" ]
then
	dist=""
else
	dist="--hostfile $distribution"
fi

program=$(echo $params | cut -d"\"" -f2 | cut -d " " -f1 )

CPUS_NODE=16
CPUS_PROC=$(($CPUS_NODE/$procs_node))

 cat > ${ini_dir}/MN_SCRIPTS/${script_name}.mn <<EOF
#!/bin/bash

### Job Name
#BSUB -J ${script_name}

### Initial Dir
#BSUB -cwd ${ini_dir}

### Output and Error files
#BSUB -oo ${ini_dir}/OUTS/${script_name}.%J.out
#BSUB -eo ${ini_dir}/OUTS/${script_name}.%J.err

### Number of processes
#BSUB -n ${mpi_procs}

### Number of processes per node
#BSUB -R"span[ptile=${procs_node}]"

### Exclusive use
#BSUB -x 

### Wall clock time in HH:MM
#BSUB -W ${duration}

ulimit -c unlimited
export TMPDIR=$TMPDIR/extrae
mkdir -p $TMPDIR
 
output="${ini_dir}/OUTS/${script_name}.\$LSB_JOBID.app"

mpirun --cpus-per-proc $CPUS_PROC $dist ${DLB_PATH}/bin/set_dlb_mn3.sh ${params} > \$output


if [ $tracing == "YES" ]
then
	if [ $version == "SMPSS" ]
	then
		export EXTRAE_LABELS=StarSs.pcf
	fi

#	if [ $mpi_procs == "1" ]
#	then
		${MPITRACE_HOME}/bin/mpi2prv -e ${program} -f ${trace_path}/TRACE.mpits -syn -o ${trace_path}/$script_name.prv >> \$output
#	else
#		mpirun ${MPITRACE_HOME}/bin/mpimpi2prv -e ${program} -f ${trace_path}/TRACE.mpits -syn -o ${trace_path}/$script_name.prv >> \$output
#	fi
	rm ${trace_path}/set-0/TRACE*


	if [ $policy != "ORIG" ]
	then
		$DLB_PATH/bin/adderDLB ${trace_path}/$script_name.pcf
	fi

fi



EOF

bsub <  ${ini_dir}/MN_SCRIPTS/${script_name}.mn

echo Submiting ${ini_dir}/MN_SCRIPTS/${script_name}.mn

